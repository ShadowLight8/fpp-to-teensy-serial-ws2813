#include <OctoWS2811.h>

#define NUM_PATCHED_CHANS 1944

#define LEDPIN 13

const int ledsPerStrip = 32;

DMAMEM int displayMemory[ledsPerStrip*6];
int drawingMemory[ledsPerStrip*6];

const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config);

void setup() {
  Serial.begin(115200);
  
  leds.begin();
  leds.show();

  // Indicate that this code is running
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, HIGH);
  delay(1000);
  digitalWrite(LEDPIN, LOW);
}

int curPixel = -1;
char serialBuffer[3];
uint8_t serialBufferCounter = 0;

void loop() {
  // Attempting to follow best practices from https://www.pjrc.com/teensy/td_serial.html
  // Teensy buffers 2 USB packets, ~128 bytes, but Serial.available only returns the bytes remaining in the first packet,
  // this means multi-byte data could be split between packets, so we'll plan for the worst case.

  // Process serial data until the header <> is found
  while (curPixel != 0) {
    // Make sure there is data, then read 2 bytes. Second read may return -1 if there isn't a second byte to read, which is ok since that wasn't a header.
    if (Serial.available() && Serial.read() == '<' && Serial.read() == '>') {
      //Header found
      curPixel = 0;
    }
    digitalWrite(LEDPIN, !digitalRead(LEDPIN));
  }
  
  while (curPixel < NUM_PATCHED_CHANS) {
    // If we have 3 bytes, then directly read and setPixel
    if (Serial.available() >= 3) {
      leds.setPixel(curPixel, Serial.read(), Serial.read(), Serial.read());
    } else { // If not, then buffer 3 bytes prior to setPixel
      //digitalWrite(LEDPIN, !digitalRead(LEDPIN)); // Help monitor when this condition occurs
      while (serialBufferCounter < sizeof(serialBuffer)) {
        if (Serial.available()) {
          serialBuffer[serialBufferCounter] = Serial.read();
          serialBufferCounter++;
        }
      }
      serialBufferCounter = 0;
      leds.setPixel(curPixel, serialBuffer[0], serialBuffer[1], serialBuffer[2]);
    }
    curPixel++;
  }
  leds.show();
  curPixel = -1;
}
