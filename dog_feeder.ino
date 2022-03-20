#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "Button2.h";
#include "ESPRotary.h";
#include "RTClib.h"
#include "env.h";

// Nokia5110 LCD
int8_t LCD_RST_PIN = 27;
int8_t LCD_CE_PIN  = 25;
int8_t LCD_DC_PIN  = 32;
int8_t LCD_DIN_PIN = 17;
int8_t LCD_CLK_PIN = 16;
int8_t LCD_BL_PIN  = 0;

// Rotary Encoder
int8_t ROTARY_CLK_PIN  = 26;
int8_t ROTARY_DT_PIN   = 18;
int8_t ROTARY_SW_PIN   = 19; // Button
int8_t ROTARY_Step_PIN = 2;  // This number depends on your rotary encoder

// RTC - Real Time Clock
// int8_t RTC_SCL_PIN = 22;
// int8_t RTC_SDA_PIN = 12;

// Buttons
int8_t BTN_1_PIN = 2;
bool g_btn1Pressed = false;

// Strings
String WIFI_IP="";

unsigned long g_lastUpdateClock = millis();
unsigned long g_lastUpdateOTA   = millis();
unsigned long g_lastUpdateBtn1  = millis();

int16_t UPDATE_BTN   = 500;    // 0.5 sec
int16_t UPDATE_CLOCK = 1000;   // 1 sec
int32_t UPDATE_OTA   = 600000; // 10 min

int8_t g_hour, g_minute, g_second, g_day, g_month, g_year;

ESPRotary r = ESPRotary(ROTARY_DT_PIN, ROTARY_CLK_PIN, ROTARY_Step_PIN);
Button2 b = Button2(ROTARY_SW_PIN);

Adafruit_PCD8544 display = Adafruit_PCD8544(LCD_CLK_PIN, LCD_DIN_PIN, LCD_DC_PIN, LCD_CE_PIN, LCD_RST_PIN);
RTC_DS3231 rtc;

void IRAM_ATTR interruptBtn1() {
  g_btn1Pressed = true;
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);

  int8_t tries = 1;
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    if (tries <= 10) {
      Serial.print("Tying to connect to the internet... ");
      Serial.println(tries);
      delay(2000);
      tries++;
    } else {
      Serial.print("Connection Failed!");
      break;
    }
  }

  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    Serial.print("Connected to ");
    Serial.println(WIFI_NAME);
    Serial.print("IP address: ");
    WIFI_IP = WiFi.localIP().toString().c_str();
    Serial.println(WIFI_IP);
  }

  // Hostname
  ArduinoOTA.setHostname(HOST);

  // No authentication by default
  // Remove next line if you don't want to add an authentication password
  ArduinoOTA.setPassword(AUTH_PASSWORD);

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // WIFI_PASSWORD can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setWIFI_PASSWORDHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else
        type = "filesystem";

      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  pinMode(LCD_BL_PIN, OUTPUT);
  backlightOn();
  display.begin();
  display.clearDisplay();
  setContrast();

  r.setChangedHandler(rotate);
  r.setLeftRotationHandler(showDirection);
  r.setRightRotationHandler(showDirection);
  b.setTapHandler(click);
  b.setLongClickHandler(resetPosition);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // January 1, 2021 at HH, MM, SS:
    rtc.adjust(DateTime(2021, 1, 30, 21, 10, 0));
  }

  // January 1, 2021 at HH, MM, SS:
  // rtc.adjust(DateTime(2021, 1, 30, 21, 10, 0));

  pinMode(BTN_1_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN_1_PIN), interruptBtn1, RISING);
}

void loop() {
  checkOTA();
  checkBtn1();
  r.loop();
  b.loop();
  updateClock();
  printHeader();
  printEncoder();
  display.display();
}

void checkOTA() {
  if (millis() - g_lastUpdateOTA <= UPDATE_OTA) {
    ArduinoOTA.handle();
  }
}

void checkBtn1() {
  if (g_btn1Pressed) {
    if (millis() - g_lastUpdateBtn1 > UPDATE_BTN) {
      Serial.println("Button was pressed");
      g_lastUpdateBtn1 = millis();
    }
    g_btn1Pressed = false;
  }
}

void resetDefaults() {
  backlightOff();
}

void setContrast() {
  display.setContrast(60);
  display.display();
}

void backlightOff() {
  digitalWrite(LCD_BL_PIN, LOW);
}

void backlightOn() {
  digitalWrite(LCD_BL_PIN, HIGH);
}

void rotate(ESPRotary& r) { Serial.println(r.getPosition()); }

// + On left or right rotation
void showDirection(ESPRotary& r) { Serial.println(r.directionToString(r.getDirection())); }

// + Single click+
void click(Button2& btn) {
  if (digitalRead(LCD_BL_PIN)) {
    backlightOff();
  } else {
    backlightOn();
  }
}

// + Long press click
void resetPosition(Button2& btn) {
  r.resetPosition();
  resetDefaults();
  Serial.println("Reset!");
}

void printHeader() {
  display.setTextSize(1);
  display.clearDisplay();
  display.setTextColor(BLACK, WHITE);
  display.setCursor(0, 0);
  display.setCursor(18, 0);
  if (g_hour < 10) {
    display.print(0);
  }
  display.print(g_hour);
  display.print(":");
  if (g_minute < 10) {
    display.print(0);
  }
  display.print(g_minute);
  display.print(":");
  if (g_second < 10) {
    display.print(0);
  }
  display.print(g_second);
  display.drawFastHLine(0, 10, 83, BLACK);
}

void printEncoder() {
  display.setTextSize(3);
  if (r.getPosition() >= 0 && r.getPosition() < 10) {
    display.setCursor(35, 15);
  } else if (r.getPosition() >= 0 && r.getPosition() < 100) {
    display.setCursor(25, 15);
  } else if (r.getPosition() <= -100) {
    display.setCursor(5, 15);
  } else {
    display.setCursor(15, 15);
  }
  display.print(r.getPosition());
}

void updateClock() {
  if (millis() - g_lastUpdateClock > UPDATE_CLOCK) {
    DateTime now = rtc.now();
    g_hour = now.hour();
    g_minute = now.minute();
    g_second = now.second();
    g_day = now.day();
    g_month = now.month();
    g_year = now.year();
    g_lastUpdateClock = millis();
  }
}
