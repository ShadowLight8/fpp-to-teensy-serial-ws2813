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
//  Added a 16k alignment to the megaBuffer to force the linker to avoid the SRAM_L / SRAM_U boundary 7/9/2020
// Thread looking at how to quickly setup the drawingMemory: https://forum.pjrc.com/threads/28115-Trouble-wrapping-my-head-around-some-parts-of-movie2serial-PDE
// Link to fast conversion method: https://github.com/bitogre/OctoWS2811/commit/18e3c93fac3aa7546c152969c9f9485c153a6ad7

#include <OctoWS2811.h>
#define qBlink() (digitalWriteFast(LED_BUILTIN, !digitalReadFast(LED_BUILTIN))) // Fast way to toggle LED on and off

// The OctoWS2811 library is designed for 8 equal length led strips, define the max number of pixels on the longest, single strip.
// For example, if you plan to run 3 strips of lengths 48, 50, and 75, set the value to 75.
// See the LED Address & Different Strip Lengths section of https://www.pjrc.com/teensy/td_libs_OctoWS2811.html for more detail.

// When you change the MAX_PIXELS_PER_STRIP, be sure to update Vixen/xLight/FPP/etc with the new channel/output count of MAX_PIXELS_PER_STRIP * 8 * 3
#define MAX_PIXELS_PER_STRIP 517

// Within the sequencing software such as Vixen or Falcon Pi Player (FPP), the serial connection should be configured for MAX_PIXELS_PER_STRIP * 8 * 3 outputs.
// 8 for the number of strips and 3 for each of the R, G, and B channels. The mapping of the channels to align with each of the 8 output the OctoWS2811 library should be within the sequencing software.
// Color order should be configured in the sequencing software.

// This code simply puts the serial stream into the OctoWS2811's input.
// When you patch up the controller in Vixen (and likely other programs), you'll end up with unconnected gaps in the outputs from the last used channel of one strip to the Starting Output of the next strip.
// These gaps are critical to the Teensy code since all it's really doing it taking a stream of data and feeding it to the OctoWS2811 library. I know this adds an extra headache when setting up the channel outputs.
// You can calculate a strip's Starting Output with ((Strip_Number - 1) * MAX_PIXELS_PER_STRIP * 3) + 1. If you had MAX_PIXELS_PER_STRIP as 444, the start of Strip 3 would be ((3 - 1) * 444 * 3) + 1 = 2665.
// When connecting the outputs in Vixen for Strip 3, you'd start with channel 2665 on your Generic Serial / Teensy controller.

// Under the Arduino Tools menu, make sure the "USB Type" is set to just "Serial". You might also try changing "Optimize" from "Faster" to "Fast".

DMAMEM int displayMemory[MAX_PIXELS_PER_STRIP * 6];
byte drawingMemory[MAX_PIXELS_PER_STRIP * 24] __attribute__((aligned(32))); // Changed from int to support fast way of loading data

// Using a 16k aligned attribute to force the linker to place megaBuffer such that it doesn't cross over the SRAM_L and SRAM_U boundary at 0x20000000.
// The memory placement can be verified in the fpp-to-teensy-serial.ino.sym file located in C:\Users\<username>\AppData\Local\Temp\arduino_build_<rnd_number>
// Search for megaBuffer - Location should be 20000000 or higher
// Aligned value may have to be changed lower under some conditions
char megaBuffer[MAX_PIXELS_PER_STRIP * 8 * 3] __attribute__((aligned(4096)));

// Configured for WS2813 LEDs which use a 300ns delay after transmitting data to latch.
OctoWS2811 leds(MAX_PIXELS_PER_STRIP, displayMemory, drawingMemory, WS2811_RGB | WS2813_800kHz);

void setup() {
  Serial.begin(115200);

  Serial1.begin(115200);

  leds.begin();
  leds.show();

  pinMode(LED_BUILTIN, OUTPUT);
  qBlink();
  // Leave led on by default, easier to see the Teensy is running. Ideally, should always be on. If it goes on and off, it indicates the Serial Header is not where it is expected to be. Check channel/output counts.
}

int curItem = 0;

int pixel[8];
int offset = 0;
int mask;
byte b;
int i;

unsigned long millisSinceBlink;

void loop() {
  // Try to read 2 bytes or hit the timeout, loop around and try again
  if (Serial.readBytes(megaBuffer, 2) == 2) {
    // Are the 2 bytes the header <>
    if (megaBuffer[0] == '<' && megaBuffer[1] == '>') {
      // Read the expected non-header bytes into megaBuffer
      Serial1.print("S:");
      Serial1.print(millis());
      Serial.readBytes(megaBuffer, MAX_PIXELS_PER_STRIP * 8 * 3); // TODO: Check the return value to make sure everything was read
      Serial1.print(" E:");
      Serial1.println(millis());
      // Original, but slower, method for populating drawingMemory
      //      curItem = 0;
      //      while (curItem < (MAX_PIXELS_PER_STRIP * 8)) {
      //        leds.setPixel(curItem, megaBuffer[curItem * 3], megaBuffer[curItem * 3 + 1], megaBuffer[curItem * 3 + 2]);
      //        curItem++;
      //      }

      offset = 0;
      curItem = 0;
      // Code is based on movie2serial.pde https://github.com/PaulStoffregen/OctoWS2811/blob/master/extras/VideoDisplay/Processing/movie2serial.pde
      // Outer loop runs through one strip worth of data
      // Inner loop gets the nth byte of each strip, 8 in total
      while (curItem < MAX_PIXELS_PER_STRIP) {
        for (i = 0; i < 8; i++) {
          // Color order should be managed by the Sequencer (aka Vixen) configuration
          pixel[i] = (megaBuffer[curItem * 3 + i * MAX_PIXELS_PER_STRIP] << 16) | (megaBuffer[curItem * 3 + 1 + i * MAX_PIXELS_PER_STRIP] << 8) | megaBuffer[curItem * 3 + 2 + i * MAX_PIXELS_PER_STRIP];
        }
        // convert 8 pixels to 24 bytes
        for (mask = 0x800000; mask != 0; mask >>= 1) {
          b = 0;
          for (i = 0; i < 8; i++) {
            if ((pixel[i] & mask) != 0) b |= (1 << i);
          }
          // Load data directly into OctoWS2811's drawingMemory
          drawingMemory[offset++] = b;
        }
        curItem++;
      }

      // Check if prior leds.show() is still running and blink if it is. Needed to determine max frame rate possible.
      if (leds.busy()) {
        Serial1.println(millis());
        qBlink();
      }
      leds.show();
    }
    else {
      // Toggle LED up to 4x per second to indicate data that isn't a header.
      if (millis() - millisSinceBlink > 250) {
        millisSinceBlink = millis();
        qBlink();
      }
    }
  }
}
