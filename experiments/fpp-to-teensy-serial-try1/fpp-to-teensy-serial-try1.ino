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

// Other interesting ideas:
//   Thread looking at how to quickly setup the drawingMemory: https://forum.pjrc.com/threads/28115-Trouble-wrapping-my-head-around-some-parts-of-movie2serial-PDE
//   Link to another conversion method: https://github.com/bitogre/OctoWS2811/commit/18e3c93fac3aa7546c152969c9f9485c153a6ad7
//   Create an output type plugin for FPP that streams the drawingMemory format - Shift all computation to the FPP host

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
byte drawingMemory[MAX_PIXELS_PER_STRIP * 8 * 3] __attribute__((aligned(32))); // Ensure 32 byte boundary because of Byte type doesn't require one

// Using an aligned attribute to force the linker to place megaBuffer such that it doesn't cross over the SRAM_L and SRAM_U boundary at 0x20000000.
// The memory placement can be verified in the fpp-to-teensy-serial.ino.sym file located in C:\Users\<username>\AppData\Local\Temp\arduino\sketches\<rnd_number>
// Older version of the Arduino IDE would place it in C:\Users\<username>\AppData\Local\Temp\arduino_build_<rnd_number>
// Search for megaBuffer - Location should be typically be 20000000 or higher, but at the least, the location + size should not cross 20000000
// attribute aligned value must be a power of 2
char megaBuffer[MAX_PIXELS_PER_STRIP * 8 * 3] __attribute__((aligned(2048)));

OctoWS2811 leds(MAX_PIXELS_PER_STRIP, displayMemory, drawingMemory, WS2811_RGB | WS2813_800kHz);

void setup() {
  Serial.setTimeout(16); //IDEA: Timeout is for total time to readBytes. Set to expected frame time. Changed from 1000ms to 17ms

  leds.begin();
  leds.show();

  // Leave led on by default, easier to see the Teensy is running. Ideally, should always be on. If it goes on and off, it indicates an issue. See bottom of the code for more info.
  pinMode(LED_BUILTIN, OUTPUT);
  qBlink();
  delay(250);
  qBlink();
}

int offset, curItem, i, mask, tmp;
int pixel[8];
byte b;
unsigned long millisSinceBlink = 0;

FASTRUN void loop() {
  // Quickly check if next bytes are the header
  // IDEA: Serial.available isn't really needed functionally, but it does yield() when no data present
  //if (Serial.available() > 0 && Serial.read() == '<' && Serial.read() == '>') {
  if (Serial.read() == '<' && Serial.read() == '>') {
    //Serial.readBytes(megaBuffer, MAX_PIXELS_PER_STRIP * 8 * 3);
    if (Serial.readBytes(megaBuffer, MAX_PIXELS_PER_STRIP * 8 * 3) == MAX_PIXELS_PER_STRIP * 8 * 3) {
      // IDEA: If count is as expected, then process as normal
      Serial.clear(); // IDEA: Dump anything left in the USB buffers
      offset = 0;
      curItem = 0;
      while (curItem < MAX_PIXELS_PER_STRIP * 3) {

        for (i = 0; i < 8; i++) {
          // Color order should be managed by the Sequencer (like Vixen/xLights) configuration
          pixel[i] = (megaBuffer[curItem + i * MAX_PIXELS_PER_STRIP * 3] << 16) | (megaBuffer[curItem + 1 + i * MAX_PIXELS_PER_STRIP * 3] << 8) | megaBuffer[curItem + 2 + i * MAX_PIXELS_PER_STRIP * 3];
        }

        for (mask = 0x800000; mask != 0; mask >>= 1) {
          b = 0;
          for (i = 0; i < 8; i++) {
            //if ((pixel[i] & mask) != 0) b |= (1 << i);
            b |= ((pixel[i] & mask) != 0) << i; // Seems to be a bit quicker - Good @ 17ms, A bit less minor glitching @ 16ms
          }
          // Load data directly into OctoWS2811's drawingMemory
          drawingMemory[offset++] = b;
        }
        curItem += 3;
      }
      // IDEA: Just before starting leds.show, check if last one is still running. If so, then processing can't keep up.
      // See this a bit at 17ms, quite a bit more at 16ms + minor glitching
      //if (leds.busy()) qBlink();
      leds.show();
      tmp = Serial.peek();
      if (!(tmp == -1 || tmp == '<')) qBlink();
      //if (Serial.available() > 0) {
        // IDEA: If here and Serial data is available, what to do? Likely not going to be keeping up with data coming in this fast
        // Since leds.show isn't blocking for the DMA transfer time, this case should never happen
      //}
    } else {
      qBlink();
      // IDEA: Didn't get the expected data count, what to do? Jump out of loop or scan for next header?
      //       This is made more critical with a lower Serial timeout
      //       Test cases:
      //         Too little data sent per frame = Timeout should catch
      //         Too much data sent per frame = Post leds.show() should catch if Serial.avaiable & NOT the header <>
      //         Frames being sent too fast = Post leds.show() should catch if Serial.avaiable & IS the header <>
      // Code is based on movie2serial.pde
      // While loop runs through one strip worth - Number of pixels * 3 channels R,G,B
      // First For loop gets each strip's nth (i) pixel's color data as an int
      // Second For loop set which bit from each pixel we process
      //  Inner For loop gets that bit from each pixel and accumulates them into a byte (b)
    }
  } //else {
    // Is there USB Serial data that isn't a header? If so, flash LED every 750ms
    // Flashing can indicate the following:
    //    Serial header of <> is missing (Lights most likely aren't working) - Check FPP settings that the header is set
    //    Serial header of <> isn't where it is expected (Lights might be working) - Verify MAX_PIXELS_PER_STRIP * 8 * 3 matches the number of channels
    //    Serial data not being processed fast enough - Sequence refresh rate should be slowed down until flashing stops (Tested with MAX_PIXELS_PER_STRIP = 517 with 17ms sequence timing)
    //tmp = Serial.peek();
    //if (tmp != -1 && tmp != '<') { // Ok if there is no data or the start of the header is present
      //} && Serial.peek() != '<' && millis() - millisSinceBlink > 750) {
    //if (Serial.available() > 0 && Serial.peek() != '<' && millis() - millisSinceBlink > 750) {
      //qBlink();
    //  millisSinceBlink = millis();
    //} else if (millis() - millisSinceBlink > 3000) // Turn LED back on in case it was left off, but no more data has come in
    //  digitalWriteFast(LED_BUILTIN, 1);
    //}
  //}
}
