#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ─────────────── WI-FI CREDENTIALS ───────────────
const char* SSID     = "YOUR WIFI NAME";
const char* PASSWORD = "YOUR WIFI PASS";
const int   UDP_PORT = 5005;

WiFiUDP udp;

// ─────────────── PIN MAP ───────────────
const int DAC_LEFT  = 25;  // GPIO25 -> Q4 base
const int DAC_RIGHT = 26;  // GPIO26 -> Q3 base
const int HSA       = 32;  // GPIO32 -> High-Side PNP Q1
const int HSB       = 33;  // GPIO33 -> High-Side PNP Q2
const int SENSE     = 34;  // ADC input across Re

enum GVSState { STATE_OFF, STATE_LEFT, STATE_RIGHT };
GVSState currentState = STATE_OFF;

// ─────────────── CALIBRATION & LIMITS ───────────────
// Chubhan kam karne aur smooth feel ke liye ~1.8 mA calibration:
const int DAC_TARGET_LEFT   = 200;  // ~1.8 mA
const int DAC_TARGET_RIGHT  = 185;  // ~1.8 mA
const int DAC_HARD_MAX      = 220;

const bool  USE_MONITOR = true;
const float RE_OHMS     = 1000.0;
const float I_LIMIT_MA  = 2.40;

const int DEADTIME_MS   = 20;

// ─────────────── SMOOTH MICRO-RAMP (Eliminates Pinprick Pain) ───────────────
const int MICRO_RAMP_STEPS = 6;
const int STEP_DELAY_MS    = 10; // Total ramp time = 60 ms

int clampDac(int v) { return constrain(v, 0, DAC_HARD_MAX); }

void allOff() {
  dacWrite(DAC_LEFT, 0);
  dacWrite(DAC_RIGHT, 0);
  digitalWrite(HSA, LOW);
  digitalWrite(HSB, LOW);
  currentState = STATE_OFF;
}

float readCurrentmA() {
  if (!USE_MONITOR) return -1.0;
  uint32_t totalMv = 0;
  for (int i = 0; i < 16; i++) {
    totalMv += analogReadMilliVolts(SENSE);
    delayMicroseconds(30);
  }
  return ((float)totalMv / 16.0) / RE_OHMS;
}

void emergencyStop(float mA) {
  allOff();
  Serial.print("!! OVER-CURRENT: ");
  Serial.print(mA, 2);
  Serial.println(" mA — HALTED.");
  while (true) { allOff(); delay(1000); }
}

int faultCount = 0;
void guardCurrent() {
  if (!USE_MONITOR) return;
  float mA = readCurrentmA();
  if (mA > I_LIMIT_MA) {
    faultCount++;
    if (faultCount >= 4) {
      emergencyStop(mA);
    }
  } else {
    faultCount = 0;
  }
}

// YAHAN PURANA transitionTo REPLACE HUA HAI:
void transitionTo(GVSState newState) {
  if (currentState == newState) return;

  allOff();
  delay(DEADTIME_MS);

  if (newState == STATE_LEFT) {
    digitalWrite(HSA, HIGH);
    delayMicroseconds(500);
    int target = clampDac(DAC_TARGET_LEFT);
    for (int i = 1; i <= MICRO_RAMP_STEPS; i++) {
      dacWrite(DAC_LEFT, (target * i) / MICRO_RAMP_STEPS);
      delay(STEP_DELAY_MS);
    }
    currentState = STATE_LEFT;
  } 
  else if (newState == STATE_RIGHT) {
    digitalWrite(HSB, HIGH);
    delayMicroseconds(500);
    int target = clampDac(DAC_TARGET_RIGHT);
    for (int i = 1; i <= MICRO_RAMP_STEPS; i++) {
      dacWrite(DAC_RIGHT, (target * i) / MICRO_RAMP_STEPS);
      delay(STEP_DELAY_MS);
    }
    currentState = STATE_RIGHT;
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  pinMode(HSA, OUTPUT);
  pinMode(HSB, OUTPUT);
  allOff();

  if (USE_MONITOR) analogSetPinAttenuation(SENSE, ADC_11db);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Connected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  udp.begin(UDP_PORT);
}

unsigned long lastPrint = 0;

void loop() {
  guardCurrent();

  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    char cmd = udp.read();
    if (cmd == 'L' || cmd == 'l') {
      transitionTo(STATE_LEFT);
    } else if (cmd == 'R' || cmd == 'r') {
      transitionTo(STATE_RIGHT);
    } else if (cmd == '0') {
      transitionTo(STATE_OFF);
    }
  }

  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    if (currentState != STATE_OFF) {
      Serial.print("Current: ");
      Serial.print(readCurrentmA(), 2);
      Serial.println(" mA");
    }
  }
}
