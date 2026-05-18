# DeskBuddy
A small desk gadget that shows the time, date, temperature and humidity on a round display. It has a few different clock faces you can switch between, goes to sleep when nobody is around, and wakes up again when you walk by.

## How it works
 
DeskBuddy is split into two parts that talk to each other over a serial connection.
 
**The Brick** is a small ESP8266 board with a temperature and humidity sensor. It connects to your WiFi, syncs the time from the internet, and sends sensor readings to the display every few seconds. If WiFi goes down, it keeps sending sensor data anyway.
 
**The Buddy** is an Arduino with a round 240x240 display. It receives the data from the Brick, shows the time and sensor values, and handles the button and the motion sensor.
 
---
 
## Display modes
 
There are five modes you can cycle through with the button:
 
- Astro Face - a little robot face that blinks and looks around
- Digital Clock - time, date, temperature and humidity as text
- Analog Clock - a classic clock with hands
- Environment - bigger view of just the temperature and humidity
- NASA Mission - a decorative status screen
---
 
## Hardware
 
- Arduino (Uno or similar) with a GC9A01A round display
- ESP8266 (like a Wemos D1 Mini) with a DHT22 sensor
- A push button and a PIR motion sensor
- The two boards connected via serial (TX/RX)
---
 
## Setup
 
1. Open `DeskBuddy_Brick_Code.ino` in the Arduino IDE and enter your WiFi name and password where it says `DEIN_WLAN_NAME` and `DEIN_WLAN_PASSWORT`
2. Upload it to the ESP8266
3. Open `DeskBuddy_Buddy_Code.ino` and upload it to the Arduino
4. Connect the two boards via serial and power everything up
---
 
## Libraries needed
 
- Adafruit GC9A01A
- Adafruit GFX
- RTClib
- DHT sensor library
- NTPClient
- ESP8266WiFi
---
 
## Notes
 
The display turns off automatically after 60 seconds without movement and wakes up when the PIR sensor detects someone. A short button press switches the display mode. The time is set automatically from the Brick whenever a valid timestamp comes in over serial.
