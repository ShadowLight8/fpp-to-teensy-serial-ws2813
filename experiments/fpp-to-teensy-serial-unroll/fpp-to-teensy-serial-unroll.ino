// Created by Nick Anderson 7/2/2017
// Updated for Halloween 2018 Show 10/8/2018
//  Updated MAX_PIXELS_PER_STRIP to 517
//  Removed color order changing, will use Vixen/xLight to manage
// Added comments from forum posts to help with setting up to use this code 12/19/2018
// Updated after Halloween 2019 11/5/2019
//  Major refactor to use Serial.readBytes
//  Improved performance and reduced glitches seen
//  Build with Teensy running at 120Mhz and Fastest optimizations
// Ongoing effort to figure out why a single large Serial.readBytes hangs 3/10/2020
//  Ultimately, the issue was memory writes across the SRAM_L / SRAM_U ram segments
//  Link to post on the PJRC forums: https://forum.pjrc.com/threads/58326-Teensy-3-2-OctoWS2811-and-large-Serial-readBytes-hang?p=232994&viewfull=1#post232994
//  Added the alignment option to the megaBuffer to help the linker to avoid the SRAM_L / SRAM_U boundary 7/9/2020
// Switched to method of building drawingMemory directly based on movie2serial.pde: https://github.com/PaulStoffregen/OctoWS2811/blob/master/extras/VideoDisplay/Processing/movie2serial.pde
//  With a RPi4 running FPP 4.0.1, MAX_PIXELS_PER_STRIP set to 517, CPU Speed @ 120 Mhz, and Fastest with LTO set, able to run at 17ms per frame or about ~58.8 fps 7/15/2020
//  Updated when/how the Teensy blinks to make it easier to see when a potential issue exists
//  Final clean up and testing 8/26/2020
// Updated for Halloween 2023 Show 9/24/2023
//  Updated Arduino IDE to 2.2.1
//  Updated Teensyduino to 1.58.1
//  Set MAX_PIXELS_PER_STRIP to 541 to support new spinner
// Updated for Speed Improvments 11/4/2023
//  Unrolled a few loops, reduced branches
//  Tested with Fastest, Fastest + pure-code, Fastest with LTO, Fastest + pure-code + LTO

// Other interesting ideas:
//   Thread looking at how to quickly setup the drawingMemory: https://forum.pjrc.com/threads/28115-Trouble-wrapping-my-head-around-some-parts-of-movie2serial-PDE
//   Link to another conversion method: https://github.com/bitogre/OctoWS2811/commit/18e3c93fac3aa7546c152969c9f9485c153a6ad7

// ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
#include <OctoWS2811.h>
#define qBlink() (digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN))) // Fast way to toggle LED on and off

// This code is designed to get the serial data stream into the OctoWS2811's drawingMemory buffer as fast as possible.

// The OctoWS2811 library is designed for 8 equal length led strips, define the max number of pixels on the longest, single strip.
// For example, if you plan to run 3 strips of lengths 48, 50, and 75, set the value to 75.
// See the LED Address & Different Strip Lengths section of https://www.pjrc.com/teensy/td_libs_OctoWS2811.html for more detail.

// When you change the MAX_PIXELS_PER_STRIP, be sure to update Vixen/xLight/FPP/etc with the new channel/output count of MAX_PIXELS_PER_STRIP * 8 * 3
#define MAX_PIXELS_PER_STRIP 541

// Within the sequencing software such as Vixen or Falcon Pi Player (FPP), the serial connection should be configured for MAX_PIXELS_PER_STRIP * 8 * 3 outputs and to send a header of <>
// 8 for the number of strips and 3 for each of the R, G, and B channels. The mapping of the channels to align with each of the 8 output the OctoWS2811 library should be within the sequencing software.
// Color order must be configured in the sequencing software.

// When you patch up the controller in Vixen (and likely other programs), you'll end up with unconnected gaps in the outputs from the last used channel of one strip to the Starting Output of the next strip.
// These gaps are critical to the Teensy code since all it's really doing it taking a stream of data and feeding it to the OctoWS2811 library. I know this adds an extra headache when setting up the channel outputs.
// You can calculate a strip's Starting Output with ((Strip_Number - 1) * MAX_PIXELS_PER_STRIP * 3) + 1. If you had MAX_PIXELS_PER_STRIP as 444, the start of Strip 3 would be ((3 - 1) * 444 * 3) + 1 = 2665.
// When connecting the outputs in Vixen for Strip 3, you'd start with channel 2665 on your Generic Serial / Teensy controller.

// Under the Arduino Tools menu, make sure the "USB Type" is set to just "Serial". Testing with CPU speed @ 120 Mhz and Optimizations as Fastest with LTO worked well.

DMAMEM int displayMemory[MAX_PIXELS_PER_STRIP * 6];
DMAMEM byte drawingMemory[MAX_PIXELS_PER_STRIP * 8 * 3] __attribute__((aligned(32))); // Ensure 32 byte boundary because of Byte type doesn't require one

// Using an aligned attribute to force the linker to place megaBuffer such that it doesn't cross over the SRAM_L and SRAM_U boundary at 0x20000000.
// The memory placement can be verified in the fpp-to-teensy-serial.ino.sym file located in C:\Users\<username>\AppData\Local\Temp\arduino\sketches\<rnd_number>
// Older version of the Arduino IDE would place it in C:\Users\<username>\AppData\Local\Temp\arduino_build_<rnd_number>
// Search for megaBuffer - Location should be typically be 20000000 or higher, but at the least, the location + size should not cross 20000000
// attribute aligned value must be a power of 2
char megaBuffer[MAX_PIXELS_PER_STRIP * 8 * 3] __attribute__((aligned(2048)));

OctoWS2811 leds(MAX_PIXELS_PER_STRIP, displayMemory, drawingMemory, WS2811_RGB | WS2813_800kHz);

void setup() {
  Serial.begin(115200);

  leds.begin();
  leds.show();

  // Leave led on by default, easier to see the Teensy is running. Ideally, should always be on. If it goes on and off, it indicates an issue. See bottom of the code for more info.
  pinMode(LED_BUILTIN, OUTPUT);
  qBlink();
  delay(750);
  qBlink();
  delay(250);
}

int offset, curItem, mask;
int pixel[8];
unsigned long millisSinceBlink = 0;

FASTRUN void loop() {
  // Quickly check if next bytes are the header
  if (Serial.read() == '<' && Serial.read() == '>') {
    Serial.readBytes(megaBuffer, MAX_PIXELS_PER_STRIP * 8 * 3);
    
    offset = 0;
    curItem = 0;
    
    // While loop runs through one strip worth - Number of pixels * 3 channels R,G,B
    while (curItem < MAX_PIXELS_PER_STRIP * 3) {

      // Get each strip's nth (i) pixel's color data as an int, unrolled to improve performance by eliminating some math operations at run time
      pixel[0] = (megaBuffer[curItem + 0 * MAX_PIXELS_PER_STRIP * 3] << 16) | (megaBuffer[curItem + 1 + 0 * MAX_PIXELS_PER_STRIP * 3] << 8) | megaBuffer[curItem + 2 + 0 * MAX_PIXELS_PER_STRIP * 3];
      pixel[1] = (megaBuffer[curItem + 1 * MAX_PIXELS_PER_STRIP * 3] << 16) | (megaBuffer[curItem + 1 + 1 * MAX_PIXELS_PER_STRIP * 3] << 8) | megaBuffer[curItem + 2 + 1 * MAX_PIXELS_PER_STRIP * 3];
      pixel[2] = (megaBuffer[curItem + 2 * MAX_PIXELS_PER_STRIP * 3] << 16) | (megaBuffer[curItem + 1 + 2 * MAX_PIXELS_PER_STRIP * 3] << 8) | megaBuffer[curItem + 2 + 2 * MAX_PIXELS_PER_STRIP * 3];
      pixel[3] = (megaBuffer[curItem + 3 * MAX_PIXELS_PER_STRIP * 3] << 16) | (megaBuffer[curItem + 1 + 3 * MAX_PIXELS_PER_STRIP * 3] << 8) | megaBuffer[curItem + 2 + 3 * MAX_PIXELS_PER_STRIP * 3];
      pixel[4] = (megaBuffer[curItem + 4 * MAX_PIXELS_PER_STRIP * 3] << 16) | (megaBuffer[curItem + 1 + 4 * MAX_PIXELS_PER_STRIP * 3] << 8) | megaBuffer[curItem + 2 + 4 * MAX_PIXELS_PER_STRIP * 3];
      pixel[5] = (megaBuffer[curItem + 5 * MAX_PIXELS_PER_STRIP * 3] << 16) | (megaBuffer[curItem + 1 + 5 * MAX_PIXELS_PER_STRIP * 3] << 8) | megaBuffer[curItem + 2 + 5 * MAX_PIXELS_PER_STRIP * 3];
      pixel[6] = (megaBuffer[curItem + 6 * MAX_PIXELS_PER_STRIP * 3] << 16) | (megaBuffer[curItem + 1 + 6 * MAX_PIXELS_PER_STRIP * 3] << 8) | megaBuffer[curItem + 2 + 6 * MAX_PIXELS_PER_STRIP * 3];
      pixel[7] = (megaBuffer[curItem + 7 * MAX_PIXELS_PER_STRIP * 3] << 16) | (megaBuffer[curItem + 1 + 7 * MAX_PIXELS_PER_STRIP * 3] << 8) | megaBuffer[curItem + 2 + 7 * MAX_PIXELS_PER_STRIP * 3];

      // For loop sets which bit from each pixel we process starting from bit 23 going to bit 0
      for (mask = 0x800000; mask != 0; mask >>= 1) {
        // Get the bit from each pixel and load directly into OctoWS2811's drawingMemory
        drawingMemory[offset++] = ((pixel[0] & mask) != 0) << 0 |
                                  ((pixel[1] & mask) != 0) << 1 |
                                  ((pixel[2] & mask) != 0) << 2 |
                                  ((pixel[3] & mask) != 0) << 3 |
                                  ((pixel[4] & mask) != 0) << 4 |
                                  ((pixel[5] & mask) != 0) << 5 |
                                  ((pixel[6] & mask) != 0) << 6 |
                                  ((pixel[7] & mask) != 0) << 7;
      }
      curItem += 3;
    }

    leds.show();
  } else {
    // Is there USB Serial data that isn't a header? If so, flash LED every 750ms
    // Flashing can indicate the following:
    //    Serial header of <> is missing (Lights most likely aren't working) - Check FPP settings that the header is set
    //    Serial header of <> isn't where it is expected (Lights might be working) - Verify MAX_PIXELS_PER_STRIP * 8 * 3 matches the number of channels
    //    Serial data not being processed fast enough - Sequence refresh rate should be slowed down until flashing stops (Tested with MAX_PIXELS_PER_STRIP = 517 with 17ms sequence timing)
    if (Serial.peek() != -1 && Serial.peek() != '<' && millis() - millisSinceBlink > 750) {
      qBlink();
      millisSinceBlink = millis();
    } else if (millis() - millisSinceBlink > 3000) // Turn LED back on in case it was left off, but no more data has come in
      digitalWriteFast(LED_BUILTIN, 1);
  }
}
