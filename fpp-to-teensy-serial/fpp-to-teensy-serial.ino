// Created by Nick Anderson 7/2/2017
// Updated for Halloween 2018 Show 10/8/2018
//  Updated MAX_PIXELS_PER_STRIP to 517
//  Removed color order changing, will use Vixen/xLight to manage
// Added comments from forum posts to help with setting up to use this code 12/19/2018
// Updated after Halloween 2019 11/5/2019
//  Major refactor to use Serial.readBytes
//  Improved performance and reduced glitches seen
//  Build with Teensy running at 120Mhz and Fastest optimizations

#include <OctoWS2811.h>
#define LEDPIN 13 // Teensy 3.2 LED Pin
#define qBlink() {GPIOC_PTOR=32;} // Fast way to toggle LED on and off

// Since the OctoWS2811 library is designed for 8 equal length led strips, we must define the max number of pixels on a single strip.
// For example, if you plan to run 3 strips of lengths 48, 50, and 75, set the value to 75.
// See the LED Address & Different Strip Lengths section of https://www.pjrc.com/teensy/td_libs_OctoWS2811.html for more detail.
#define MAX_PIXELS_PER_STRIP 517
// When you change the MAX_PIXELS_PER_STRIP, be sure to update Vixen/xLight/FPP/etc with the new channel/output count of MAX_PIXELS_PER_STRIP * 8 * 3

// Within the sequencing software such as Vixen or Falcon Pi Player (FPP), the serial connection should be configured for MAX_PIXELS_PER_STRIP * 8 * 3 outputs.
// 8 for the number of strips and 3 for each of the R, G, and B channels. The mapping of the channels to align with each of the 8 output the OctoWS2811 library should be within the sequencing software.
// Color order should be configured in the sequencing software.
// This code simply puts the serial stream into the OctoWS2811's input.

// Under the Arduino Tools menu, make sure the "USB Type" is set to just "Serial". You might also try changing "Optimize" from "Faster" to "Fast".

// When you patch up the controller in Vixen (and likely other programs), you'll end up with unconnected gaps in the outputs from the last used channel of one strip to the Starting Output of the next strip.
// These gaps are critical to the Teensy code since all it's really doing it taking a stream of data and feeding it to the OctoWS2811 library. I know this adds an extra headache when setting up the channel outputs.
// You can calculate a strip's Starting Output with ((Strip_Number - 1) * MAX_PIXELS_PER_STRIP * 3) + 1. If you had MAX_PIXELS_PER_STRIP as 444, the start of Strip 3 would be ((3 - 1) * 444 * 3) + 1 = 2665.
// When connecting the outputs in Vixen for Strip 3, you'd start with channel 2665 on your Generic Serial / Teensy controller.

DMAMEM int displayMemory[MAX_PIXELS_PER_STRIP * 6];
int drawingMemory[MAX_PIXELS_PER_STRIP * 6];

// Configured for WS2813 LEDs which use a 300ns delay after transmitting data to latch.
OctoWS2811 leds(MAX_PIXELS_PER_STRIP, displayMemory, drawingMemory, WS2811_RGB | WS2813_800kHz);

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20);
  
  leds.begin();
  leds.show();

  pinMode(LEDPIN, OUTPUT);
  qBlink();
  // Leave led on by default, easier to see the Teensy is running. Ideally, should always be on. If it goes on and off, it indicates the Serial Header is not where it is expected to be. Check channel/output counts.
}

char megaBuffer[MAX_PIXELS_PER_STRIP * 8 * 3]; // 12408 currently
int curItem = 0;
//int n = 0;
unsigned long millisSinceBlink;

void loop() {
  // Try to read 2 bytes or hit the timeout, loop around and try again
  if (Serial.readBytes(megaBuffer, 2) == 2) {
    if (megaBuffer[0] == '<' && megaBuffer[1] == '>') {
    //Header found, set curItem to 0 to start loading data by reading big chunks of data at a time
      
// TODO: This only works with usb_desc.h NUM_USB_BUFFERS set to 31 from 12
      curItem = 0;
      while (curItem < 24) {
        Serial.readBytes(&megaBuffer[curItem * MAX_PIXELS_PER_STRIP], MAX_PIXELS_PER_STRIP);
        curItem++;
      }
      
// TODO: This doesn't work
//      curItem = 0;
//      while (curItem < MAX_PIXELS_PER_STRIP * 8 * 3) {
//        n = Serial.readBytes(megaBuffer + curItem, MAX_PIXELS_PER_STRIP * 8 * 3 - curItem);
//        if (millis() - millisSinceBlink > 25) {
//          millisSinceBlink = millis();
//          qBlink();
//        }
//        curItem += n;
//      }

// TODO: This doesn't work
//      Serial.readBytes(megaBuffer, MAX_PIXELS_PER_STRIP * 8 * 3);
      
      // TODO: Directly load the Serial data into the leds' drawingMemory if more speed is needed
      curItem = 0;
      while (curItem < (MAX_PIXELS_PER_STRIP * 8)) {
        leds.setPixel(curItem, megaBuffer[curItem * 3], megaBuffer[curItem * 3 + 1], megaBuffer[curItem * 3 + 2]);
        curItem++;
      }
      leds.show();
    }
    else {
      // Toggle LED to indicate data that isn't a header.
      if (millis() - millisSinceBlink > 250) {
         millisSinceBlink = millis();
         qBlink();
      }
    }
  }
}
