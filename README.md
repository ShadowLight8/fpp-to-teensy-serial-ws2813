# fpp-to-teensy-serial-ws2813
Code for controlling WS2813 led lights with a Teensy 3.2 connected via USB/Serial to a Raspberry Pi running Falcon Pi Player

Full stack from Light Sequence to LED

Windows 10 PC
   running
Vixen 3 (http://www.vixenlights.com/)
   sequences created, then exported as Falcon Player Sequence to
Raspberry Pi 3 running Falcon Pi Player, FPP (http://falconchristmas.com/forum/index.php/topic,483.0.html)
   connected via USB/Serial to
Teensy 3.2 with OctoWS2811 adapter (https://www.pjrc.com/store/teensy32.html and https://www.pjrc.com/store/octo28_adaptor.html)
   running the code here (plus the OctoWS2811 library) to control via Cat6 wire
WS2811 Adafruit Neopixels (for now. Switching to WS2813 / WS2818 leds.)
