// SPDX-License-Identifier: MIT
// Copyright (c) 2026 [sweet.famine, LuckiestLuc, Lu2aDev80]

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "DHT.h"


#define DHT_PIN  14
#define DHTTYPE  DHT22


const char* ssid     = "DEIN_WLAN_NAME";
const char* password = "DEIN_WLAN_PASSWORT";


const long utcOffsetSeconds        = 7200;
const unsigned long SEND_INTERVAL_MS       = 3000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 15000;
const uint8_t SENSOR_DECIMALS             = 1;


DHT dht(DHT_PIN, DHTTYPE);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetSeconds, 60000);

unsigned long lastSendTime      = 0;
unsigned long lastWiFiRetryTime = 0;

bool wifiWasConnected = false;
bool timeSynced       = false;


void setup() {

  Serial.begin(9600);

  dht.begin();

  Serial.println(F("=== BRICK START ==="));

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  lastWiFiRetryTime = millis();

  timeClient.begin();

  Serial.println(F("WLAN-Verbindung laeuft im Hintergrund."));

}


void loop() {

  maintainWiFiAndTime();

  if (millis() - lastSendTime < SEND_INTERVAL_MS) {
    yield();
    return;
  }

  lastSendTime = millis();

  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  if (!isnan(temp) && !isnan(hum)) {
    sendSensorPacket(temp, hum);
  } else {
    Serial.println(F("Fehler beim Lesen des DHT22!"));
  }

}


void maintainWiFiAndTime() {

  bool wifiConnected = WiFi.status() == WL_CONNECTED;

  if (wifiConnected) {

    if (!wifiWasConnected) {
      Serial.println(F("WLAN verbunden!"));
      wifiWasConnected = true;
    }

    if (timeClient.update()) {
      timeSynced = true;
    }

    return;

  }

  if (wifiWasConnected) {
    Serial.println(F("WLAN getrennt, Sensordaten laufen weiter."));
    wifiWasConnected = false;
  }

  if (millis() - lastWiFiRetryTime >= WIFI_RETRY_INTERVAL_MS) {
    lastWiFiRetryTime = millis();
    WiFi.begin(ssid, password);
  }

}


void sendSensorPacket(float temp, float hum) {

  char tempText[12];
  char humText[12];

  dtostrf(temp, 0, SENSOR_DECIMALS, tempText);
  dtostrf(hum,  0, SENSOR_DECIMALS, humText);

  if (timeSynced) {

    time_t epoch    = timeClient.getEpochTime();
    struct tm* t    = localtime(&epoch);

    Serial.print(F("DATA,"));
    Serial.print(t->tm_year + 1900);
    Serial.print(',');
    Serial.print(t->tm_mon + 1);
    Serial.print(',');
    Serial.print(t->tm_mday);
    Serial.print(',');
    Serial.print(t->tm_hour);
    Serial.print(',');
    Serial.print(t->tm_min);
    Serial.print(',');
    Serial.print(t->tm_sec);
    Serial.print(',');
    Serial.print(tempText);
    Serial.print(',');
    Serial.println(humText);

    return;

  }

  Serial.print(F("SENSOR,"));
  Serial.print(tempText);
  Serial.print(',');
  Serial.println(humText);

}
