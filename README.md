# Dementia-Radio
ESP32 based radio for people with dementia.
Project created with Arduino IDE.
Can play audio from SD card (default) via SPI interface or using WiFi connection of ESP32 for playing audio stream from onlöine source (webradio).
Uses I2S audio interface of ESP32 for audio output. Can used together with MAX98357A breakout boards.

Includes modified version of ESP32-audioI2S from schreibfaul1: https://github.com/schreibfaul1/ESP32-audioI2S
Requires ESP32 Arduino library from espressif: https://github.com/espressif/arduino-esp32

## Installation
Copy modified files for EPS32-I2S li9brary from project to your install gloabel library  and replace library content. That will change the argument for connecting the I23S library to a file system (like SD card) file from a const char[] to a String, to use dynamic names.

## Usage
An external pin can bes used to switch on/off the board.
During switched off, the audio stream stops and ESp32 is in deep sleep mode to save energy.

It can be powerded by an 2A, 5V micro USB power supply.
Battry usage is possible ion general, but not tested until now.

The project is still under development and in alpha state. Usage is on your own risk!
