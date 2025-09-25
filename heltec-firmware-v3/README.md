# Heltec Firmware
Prototype RadioDoge firmware for the Heltec WiFi LoRa 32 (v2 and v3) modules

https://heltec.org/project/wifi-lora-32-v3/

## Installing the development environment
Instructions for installing the Arduino IDE and required libraries can be found by following the links below.
<ul>
  <li>https://wiki-content.arduino.cc/en/software</li>
  <li>https://heltec.org/wifi_kit_install/</li>
</ul>

If working in Windows you may need to install USB drivers for the device which can be found at...
<ul>
  <li>https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads</li>
</ul>

## Current Progress
Currently custom LoRa packets are formed and addressed to individual LoRa modules through the following addressing scheme:

Region.Community.Node (e.g., 10.1.3)

Devices are able to send the following messages to each other through the custom LoRa packets:

<ul>
  <li>Pings</li>
  <li>ACKs</li>
  <li>User defined messages (e.g., text messages)</li>
</ul>

Command and control of the modules is done over serial communication with a host device. The host is in charge of issuing commands to the module(s) such as setting the module's address and issuing pings. Please note that there is an excess of serial writing from the LoRa modules to the host right now, however, this is intended to just be temporary for debugging purposes.
