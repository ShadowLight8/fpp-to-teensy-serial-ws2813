#include <OctoWS2811.h>

#define LEDPIN 13

const int ledsPerStrip = 32;

DMAMEM int displayMemory[ledsPerStrip*6];
int drawingMemory[ledsPerStrip*6];

const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config);

void setup() {
  pinMode(LEDPIN, OUTPUT);
  
  leds.begin();
  leds.show();

  Serial.begin(115200);
}

int incByte;
int curPixel = 0;

void loop() {
  while (Serial.available() >= 3) {
    digitalWrite(LEDPIN, HIGH);
    leds.setPixel(curPixel, Serial.read(), Serial.read(), Serial.read());
    ++curPixel %= ledsPerStrip;
    digitalWrite(LEDPIN, LOW);
  }
  leds.show();
  delay(5);
}

