// Created by Nick Anderson 7/2/2017
// Updated for Halloween 2018 Show 10/8/2018
//  Updated MAX_PIXELS_PER_STRIP to 517
//  Removed color order changing, will use Vixen/xLight to manage
// Added comments from forum posts to help with setting up to use this code 12/19/2018

#include <OctoWS2811.h>
#define LEDPIN 13 // Teensy 3.2 LED Pin

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
  
  leds.begin();
  leds.show();

  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, HIGH); // Led on by default, easier to see the Teensy is running. Ideally, should always be on. If it goes on and off, it indicates the Serial Header is not where it is expected to be. Check channel/output counts.
}

int curPixel = -1;
char serialBuffer[3];
uint8_t serialBufferCounter = 0;

void loop() {
  // Attempting to follow best practices from https://www.pjrc.com/teensy/td_serial.html
  // Teensy buffers 2 USB packets, ~128 bytes, but Serial.available only returns the bytes remaining in the first packet,
  // this means multi-byte data could be split between packets, so we'll plan for the worst case.

  OctoWS2811 leds(MAX_PIXELS_PER_STRIP, displayMemory, drawingMemory, WS2811_RGB | WS2813_800kHz);

  // Process serial data until the header <> is found
  while (curPixel != 0)
    // Make sure there is data, then read 2 bytes. Second read may return -1 if there isn't a second byte to read, which is ok since that wasn't a header.
    if (Serial.available())
    {
      if (Serial.read() == '<' && Serial.read() == '>')
        //Header found, set curPixel to 0 to start loading data
        curPixel = 0;
      else
        // Toggle LED to indicate data that isn't a header. Ideally, this shouldn't happen.
        digitalWrite(LEDPIN, !digitalRead(LEDPIN));
    }
  
  while (curPixel < (MAX_PIXELS_PER_STRIP * 8)) {
    // If we have 3 bytes, then directly read and setPixel
    if (Serial.available() >= 3)
      leds.setPixel(curPixel++, Serial.read(), Serial.read(), Serial.read());
    
    // If not, then buffer 3 bytes prior to setPixel
    else { 
      //digitalWrite(LEDPIN, !digitalRead(LEDPIN)); // Option to monitor when this condition occurs, which is a lot
      while (serialBufferCounter < 3) 
        if (Serial.available()) 
          serialBuffer[serialBufferCounter++] = Serial.read();

      serialBufferCounter = 0;
      leds.setPixel(curPixel++, serialBuffer[0], serialBuffer[1], serialBuffer[2]);
    }
  }
  leds.show();
  curPixel = -1;
}
