# fpp-to-teensy-serial-ws2813
Code for controlling WS2813 led lights with a Teensy 3.2 connected via USB/Serial to a Raspberry Pi running Falcon Pi Player

Full stack from Light Sequence to LED

Windows 10 PC<br>
---running<br>
Vixen 3 (http://www.vixenlights.com/)<br>
---sequences created, then exported as Falcon Player Sequence to<br>
Raspberry Pi 3 running Falcon Pi Player, FPP (http://falconchristmas.com/forum/index.php/topic,483.0.html)<br>
---connected via USB/Serial to<br>
Teensy 3.2 with OctoWS2811 adapter (https://www.pjrc.com/store/teensy32.html and https://www.pjrc.com/store/octo28_adaptor.html)<br>
---running the code here (plus the OctoWS2811 library) to control via Cat6 wire<br>
WS2811 Adafruit Neopixels (for now. Switching to WS2813 / WS2818 leds.)<br>
