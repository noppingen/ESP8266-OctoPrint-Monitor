# OctoPrint-Monitor

A basic OctoPrint-Monitor with ESP8266 (I use an NodeMCU v2, use `$random_hardware` that has WiFi and supports I2C) and an SSD1306 OLED

## Summary

Code for a (very) basic OctoPrint monitor with a SSD1306 OLED display. It displays job information when printing, is dimmed when OctoPrint is offline. 

The code has basic support for NTP and time handling included, but is unused in the code. Feel free to adopt to your needs.

## Copyright and sources

* Copyright (c) 2020 noppingen: https://github.com/noppingen/ESP8266-OctoPrint-Monitor/

## Get it running

* Create a new PlatformIO project, paste `main.cpp`
* Add required libraries: 
  * `ArduinoJson` version 6: https://github.com/bblanchon/ArduinoJson
  * `ThingPulse esp8266-oled-ssd1306`: https://github.com/ThingPulse/esp8266-oled-ssd1306
  * `NTPclient`: https://github.com/arduino-libraries/NTPClient
  * `Time`: https://playground.arduino.cc/Code/Time/
* Set
  * `YOUR_OCTOPRINT_HOST`
  * `YOUR_OCTOPRINT_API_KEY`
  * `YOUR_WIFI_SSID`
  * `YOUR_WIFI_PASS`
  * `YOUR_NTP_HOST`
  * `SCL` pin
  * `SDA` pin
* Wire 4 pins, 
* When using an Node MCU, do **not** use GPIO9/10 for I2C as I did in the pictures below:  https://www.letscontrolit.com/forum/viewtopic.php?t=1462
* Print a case

## Images

![Online](online.jpg)

![Offline](offline.jpg)
