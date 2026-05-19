#include "Adafruit_GC9A01A.h"
#include <Adafruit_GFX.h>
#include <SPI.h>
#include "RTClib.h"
#include <stdlib.h>
#include <string.h>


// Pin-Definitionen
#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TFT_BL     7
#define BUTTON_PIN 2
#define PIR_PIN    4


Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);
RTC_Millis rtc;


int currentMode = 0;
bool displayOn = true;

int lastSec = -1;
int lastMinute = -1;

bool redrawRequested = false;
bool fullRedrawRequested = false;

String lastDisplayKey = "";


const int16_t SCREEN_CENTER_X = 120;
const int16_t SCREEN_CENTER_Y = 120;
const int16_t RING_OUTER = 118;
const int16_t RING_INNER = 117;


float temp = 0;
float hum = 0;

unsigned long lastBrickDataTime = 0;
bool brickDataReceived = false;

const unsigned long BRICK_TIMEOUT = 30000;
const unsigned long SLEEP_TIMEOUT = 60000;
const unsigned long LONG_PRESS = 800;


unsigned long lastMovementTime = 0;

unsigned long buttonPressedTime = 0;
bool buttonActive = false;
bool buttonWokeDisplay = false;


char serialLine[96];
uint8_t serialLineLen = 0;


void setup() {
  // Initialisierung
  Serial.begin(9600);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);

  // Display
  tft.begin();
  tft.cp437(true);
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);

  // RTC mit aktuellem Datum
  rtc.begin(DateTime(F(__DATE__), F(__TIME__)));

  lastMovementTime = millis();

  showIntro();

}


void loop() {
  // Sensoren und Display
  readBrickData();
  checkBrickDataTimeout();

  DateTime now = rtc.now();

  // Bewegungssensor aktiviert Display
  if (digitalRead(PIR_PIN) == HIGH) {
    if (!displayOn) {
      wakeDisplayToStart();
    }
    lastMovementTime = millis();
  }

  // Display nach Inaktivität ausschalten
  if (displayOn && millis() - lastMovementTime > SLEEP_TIMEOUT) {
    displayOn = false;
    digitalWrite(TFT_BL, LOW);
  }

  checkButton();

  bool timedUpdate = false;

  if (currentMode == 0 || currentMode == 2) {
    timedUpdate = now.second() != lastSec;
  } else if (currentMode == 1) {
    timedUpdate = now.minute() != lastMinute;
  }

  if (displayOn && (redrawRequested || timedUpdate)) {
    updateDisplay(now, fullRedrawRequested);
    lastSec = now.second();
    lastMinute = now.minute();
    redrawRequested = false;
    fullRedrawRequested = false;
  }

}


void readBrickData() {
  // Serielle Daten lesen und parsen
  while (Serial.available()) {

    char c = Serial.read();

    if (c == '\n') {
      serialLine[serialLineLen] = '\0';
      parseBrickLine(serialLine);
      serialLineLen = 0;

    } else if (c != '\r') {

      if (serialLineLen < sizeof(serialLine) - 1) {
        serialLine[serialLineLen++] = c;
      } else {
        serialLineLen = 0;
      }

    }
  }

}


void parseBrickLine(const char* line) {
  if (strncmp(line, "DATA,", 5) == 0) {
    parseTimedSensorLine(line + 5);

  } else if (strncmp(line, "SENSOR,", 7) == 0) {
    parseSensorOnlyLine(line + 7);
  }

}


void parseTimedSensorLine(const char* payload) {

  const char* cursor = payload;

  long year, month, day;
  long hour, minute, second;
  float newTemp, newHum;

  if (!parseLongField(cursor, year))   return;
  if (!parseLongField(cursor, month))  return;
  if (!parseLongField(cursor, day))    return;
  if (!parseLongField(cursor, hour))   return;
  if (!parseLongField(cursor, minute)) return;
  if (!parseLongField(cursor, second)) return;
  if (!parseFloatField(cursor, newTemp)) return;
  if (!parseLastFloat(cursor, newHum))   return;

  if (!isValidDateTime(year, month, day, hour, minute, second)) return;

  rtc.adjust(DateTime(
    (uint16_t)year,
    (uint8_t)month,
    (uint8_t)day,
    (uint8_t)hour,
    (uint8_t)minute,
    (uint8_t)second
  ));

  acceptSensorValues(newTemp, newHum);

}


void parseSensorOnlyLine(const char* payload) {

  const char* cursor = payload;
  float newTemp, newHum;

  if (!parseFloatField(cursor, newTemp)) return;
  if (!parseLastFloat(cursor, newHum))   return;

  acceptSensorValues(newTemp, newHum);

}


bool parseLongField(const char*& cursor, long& value) {

  char* end;
  value = strtol(cursor, &end, 10);

  if (end == cursor || *end != ',') return false;

  cursor = end + 1;
  return true;

}


bool parseFloatField(const char*& cursor, float& value) {

  char* end;
  value = strtod(cursor, &end);

  if (end == cursor || *end != ',') return false;

  cursor = end + 1;
  return true;

}


bool parseLastFloat(const char*& cursor, float& value) {

  char* end;
  value = strtod(cursor, &end);

  return end != cursor;

}


bool isValidDateTime(long year, long month, long day, long hour, long minute, long second) {

  if (year < 2024 || year > 2099)   return false;
  if (month < 1   || month > 12)    return false;
  if (day < 1     || day > 31)      return false;
  if (hour < 0    || hour > 23)     return false;
  if (minute < 0  || minute > 59)   return false;
  if (second < 0  || second > 59)   return false;

  return true;

}


void acceptSensorValues(float newTemp, float newHum) {
  // speichern und aktualisieren
  temp = newTemp;
  hum  = newHum;

  lastBrickDataTime = millis();
  brickDataReceived = true;

  requestSensorDisplayUpdate();

}


void checkBrickDataTimeout() {

  if (brickDataReceived && millis() - lastBrickDataTime > BRICK_TIMEOUT) {
    brickDataReceived = false;
    requestSensorDisplayUpdate();
  }

}


void checkButton() {
  bool btnDown = !digitalRead(BUTTON_PIN);

  if (btnDown && !buttonActive) {

    buttonPressedTime  = millis();
    buttonActive       = true;
    buttonWokeDisplay  = false;
    lastMovementTime   = millis();

    if (!displayOn) {
      wakeDisplayToStart();
      buttonWokeDisplay = true;
    }

  } else if (!btnDown && buttonActive) {

    unsigned long duration = millis() - buttonPressedTime;

    if (!buttonWokeDisplay && duration < LONG_PRESS) {
      currentMode = (currentMode + 1) % 5;
      forceRedraw();
    }

    buttonActive      = false;
    buttonWokeDisplay = false;
    lastMovementTime  = millis();

  }

}


void wakeDisplayToStart() {

  currentMode = 0;
  forceRedraw();

  DateTime now = rtc.now();
  updateDisplay(now, true);

  lastSec    = now.second();
  lastMinute = now.minute();

  redrawRequested     = false;
  fullRedrawRequested = false;

  displayOn = true;
  digitalWrite(TFT_BL, HIGH);

}


void forceRedraw() {

  tft.fillScreen(GC9A01A_BLACK);

  lastSec        = -1;
  lastMinute     = -1;
  lastDisplayKey = "";

  redrawRequested     = true;
  fullRedrawRequested = true;

}


void requestDisplayUpdate() {
  redrawRequested = true;
}


void requestSensorDisplayUpdate() {

  if ((currentMode == 1 || currentMode == 3) && displayedValuesChanged(rtc.now())) {
    requestDisplayUpdate();
  }

}


void updateDisplay(DateTime now, bool fullRedraw) {
  switch (currentMode) {
    case 0: drawAstroFace(now, fullRedraw); break;      // Astro-Gesicht
    case 1: drawDigitalLifeSupport(now);    break;      // Digital Uhr+Sensor
    case 2: drawAnalogClock(now);           break;      // Analoge Uhr
    case 3: drawFullWeather();              break;      // Umgebungssensor
    case 4: drawNasaMission();              break;      // NASA-Mission
  }

  rememberDisplayedValues(now);

}


String buildDisplayKey(DateTime now) {

  if (currentMode == 1) {
    return String("digital|")
      + formatTimeLabel(now) + "|"
      + formatDateLabel(now) + "|"
      + formatTemperatureValue(temp, 1) + "|"
      + formatHumidityValue(hum, 1);
  }

  if (currentMode == 3) {
    return String("weather|")
      + formatTemperatureValue(temp, 1) + "|"
      + formatHumidityValue(hum, 1) + "|"
      + (brickDataReceived ? "online" : "offline");
  }

  return "";

}


bool displayedValuesChanged(DateTime now) {

  String key = buildDisplayKey(now);
  return key.length() > 0 && key != lastDisplayKey;

}


void rememberDisplayedValues(DateTime now) {

  String key = buildDisplayKey(now);

  if (key.length() > 0) {
    lastDisplayKey = key;
  }

}


void drawAstroFace(DateTime now, bool fullRedraw) {

  int lookOffset = (now.second() % 5 - 2) * 4;
  bool blink     = now.second() % 6 == 0;

  if (fullRedraw) {
    tft.fillScreen(0xEF5D);
    tft.drawCircle(120, 120, 118, 0xD69A);
    tft.drawCircle(120, 120, 117, 0xD69A);
  }

  int eyeY = 104;

  tft.fillRoundRect( 30, 62, 80, 88, 36, GC9A01A_WHITE);
  tft.fillRoundRect(130, 62, 80, 88, 36, GC9A01A_WHITE);

  if (blink) {

    tft.fillRoundRect( 44, eyeY - 6, 52, 14, 7, 0x39E7);
    tft.fillRoundRect(144, eyeY - 6, 52, 14, 7, 0x39E7);
    tft.drawFastHLine( 50, eyeY, 40, GC9A01A_CYAN);
    tft.drawFastHLine(150, eyeY, 40, GC9A01A_CYAN);

  } else {

    tft.fillCircle( 70 + lookOffset, eyeY, 27, 0x18E3);
    tft.fillCircle(170 + lookOffset, eyeY, 27, 0x18E3);
    tft.fillCircle( 70 + lookOffset, eyeY, 16, GC9A01A_CYAN);
    tft.fillCircle(170 + lookOffset, eyeY, 16, GC9A01A_CYAN);

    tft.fillCircle( 60 + lookOffset, eyeY - 13, 5, GC9A01A_WHITE);
    tft.fillCircle(160 + lookOffset, eyeY - 13, 5, GC9A01A_WHITE);

  }

  tft.drawFastHLine(84, 178, 72, 0x39E7);
  tft.drawFastHLine(92, 182, 56, 0x39E7);

}


void drawAnalogClock(DateTime now) {

  tft.fillCircle(120, 120, 95, GC9A01A_BLACK);
  tft.drawCircle(120, 120, 118, GC9A01A_WHITE);

  for (int i = 0; i < 12; i++) {
    float a  = (i * 30 - 90) * PI / 180;
    int x1 = 120 + cos(a) * 82;
    int y1 = 120 + sin(a) * 82;
    int x2 = 120 + cos(a) * 91;
    int y2 = 120 + sin(a) * 91;
    tft.drawLine(x1, y1, x2, y2, GC9A01A_WHITE);
  }

  float sA = (now.second() * 6 - 90) * PI / 180;
  float mA = (now.minute() * 6 - 90) * PI / 180;
  float hA = ((now.hour() % 12 * 30 + now.minute() * 0.5) - 90) * PI / 180;

  tft.drawLine(120, 120, 120 + cos(sA) * 90, 120 + sin(sA) * 90, GC9A01A_RED);
  tft.drawLine(120, 120, 120 + cos(mA) * 80, 120 + sin(mA) * 80, GC9A01A_WHITE);
  tft.drawLine(120, 120, 120 + cos(hA) * 50, 120 + sin(hA) * 50, GC9A01A_WHITE);

  tft.fillCircle(120, 120, 4, GC9A01A_WHITE);

}


void drawDigitalLifeSupport(DateTime now) {

  clearCenterBand(40, 20);
  drawCenteredText("DIGITAL", 46, 1, GC9A01A_CYAN);

  clearCenterBand(74, 36);
  drawCenteredText(formatTimeLabel(now), 78, 4, GC9A01A_WHITE);

  clearCenterBand(120, 22);
  drawCenteredText(formatDateLabel(now), 124, 2, GC9A01A_WHITE);

  clearCenterBand(156, 22);
  drawCenteredText(formatTemperatureValue(temp, 1), 158, 2, GC9A01A_CYAN);

  clearCenterBand(181, 22);
  drawCenteredText(formatHumidityValue(hum, 1), 183, 2, GC9A01A_MAGENTA);

  drawScreenRing(GC9A01A_CYAN);

}


void drawFullWeather() {

  clearCenterBand(46, 22);
  drawCenteredText("ENV-SCAN", 50, 2, GC9A01A_CYAN);

  clearCenterBand(88, 30);
  drawCenteredText(formatTemperatureValue(temp, 1), 92, 3, GC9A01A_WHITE);

  clearCenterBand(140, 30);
  drawCenteredText(formatHumidityValue(hum, 1), 145, 3, GC9A01A_MAGENTA);

  clearCenterBand(196, 14);

  if (brickDataReceived) {
    drawCenteredText("online",  200, 1, GC9A01A_GREEN);
  } else {
    drawCenteredText("offline", 200, 1, GC9A01A_RED);
  }

  drawScreenRing(GC9A01A_BLUE);

}


void drawNasaMission() {

  tft.drawCircle(120, 120, 115, GC9A01A_ORANGE);

  tft.setTextColor(GC9A01A_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(65, 45);
  tft.println("NASA APOD");

  tft.setTextSize(1);
  tft.setTextColor(GC9A01A_WHITE);

  tft.setCursor(50, 80);
  tft.println("ID: BKM-MES-2026");

  tft.setCursor(50, 95);
  tft.println("UPLINK: ACTIVE");

  tft.fillRect(60, 115, 120, 60, 0x9102);
  tft.fillCircle(150, 130, 10, GC9A01A_ORANGE);
  tft.drawFastHLine(60, 165, 120, GC9A01A_BLACK);

  tft.drawRect(60, 185, 120, 10, GC9A01A_WHITE);
  tft.fillRect(60, 185,  85, 10, GC9A01A_GREEN);

  tft.setCursor(75, 205);
  tft.setTextColor(GC9A01A_GREEN);
  tft.print("RECV: 74.2%");

}


void showIntro() {

  tft.fillScreen(GC9A01A_BLACK);

  tft.setTextColor(GC9A01A_CYAN);
  tft.setTextSize(2);

  tft.setCursor(50, 100);
  tft.println("ASTRO-BUDDY");

  tft.setCursor(70, 130);
  tft.println("ONLINE");

  delay(1500);

  forceRedraw();

}


String twoDigits(int value) {

  if (value < 10) {
    return "0" + String(value);
  }

  return String(value);

}


String formatTimeLabel(DateTime now) {
  return twoDigits(now.hour()) + ":" + twoDigits(now.minute());
}


String formatDateLabel(DateTime now) {

  const char* weekdays[] = {"SO", "MO", "DI", "MI", "DO", "FR", "SA"};

  String label = weekdays[now.dayOfTheWeek()];
  label += " ";
  label += twoDigits(now.day());
  label += ".";
  label += twoDigits(now.month());
  label += ".";
  label += twoDigits(now.year() % 100);

  return label;

}


String formatTemperatureValue(float value, int decimals) {

  if (!brickDataReceived) return "N/A";

  String text = String(value, decimals);
  text += (char)248;
  text += "C";

  return text;

}


String formatHumidityValue(float value, int decimals) {

  if (!brickDataReceived) return "N/A";

  String text = String(value, decimals);
  text += "%";

  return text;

}


void drawScreenRing(uint16_t color) {
  tft.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, RING_OUTER, color);
  tft.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, RING_INNER, color);
}


void clearCenterBand(int y, int height) {
  tft.fillRect(18, y, 204, height, GC9A01A_BLACK);
}


void drawCenteredText(const String& text, int y, uint8_t textSize, uint16_t color) {

  int16_t  x1, y1;
  uint16_t w, h;

  tft.setTextSize(textSize);
  tft.setTextColor(color, GC9A01A_BLACK);
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, y);
  tft.print(text);

}