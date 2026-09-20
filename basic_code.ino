#include <WiFi.h>   // only used to switch the radio OFF (cuts DAC noise)

// ─────────────── PIN MAP (match your wiring) ───────────────
#define LEFT  0
#define RIGHT 1

const int DAC_LEFT  = 25;  // GPIO25/DAC1 -> Q4 base (sink on B). Pairs with HSA/Q1.
const int DAC_RIGHT = 26;  // GPIO26/DAC2 -> Q3 base (sink on A). Pairs with HSB/Q2.
const int HSA       = 32;  // -> Q5 -> high-side PNP Q1 (node A)
const int HSB       = 33;  // -> Q6 -> high-side PNP Q2 (node B)
const int SENSE     = 34;  // ADC input, wired to the TOP of Re (input-only pin)

// LEFT/RIGHT are just labels — confirm on a person which one leans which way,
// then rename if you like. LEFT drives HSA+DAC25, RIGHT drives HSB+DAC26.

// ─────────────── CALIBRATION FOR ~2.0 mA ───────────────
const int DAC_TARGET_LEFT   = 215;   // ~2.0 mA left target
const int DAC_TARGET_RIGHT  = 200;   // ~2.0 mA right target
const int DAC_HARD_MAX      = 225;   // clamp to avoid heavy base saturation

// ─────────────── SAFETY MONITOR ───────────────
const bool  USE_MONITOR     = true;
const float RE_OHMS         = 1000.0;
const float I_LIMIT_MA      = 2.50;   // 2.5 mA par abort threshold
const float I_LOW_MA        = 0.90;   // warn agar contact loose ho

// ─────────────── FAST WOBBLE TIMING (1 Hz Alternating) ───────────────
const int RAMP_UP_MS   = 100;   // 100ms me full attack (fast edge)
const int HOLD_MS      = 400;   // Sirf 400ms hold (ek side)
const int RAMP_DOWN_MS = 100;   // 100ms ramp down
const int DEADTIME_MS  = 50;    // 50ms safe shoot-through gap

// Total half-cycle = 100 + 400 + 100 + 50 = 650 ms (~0.8 Hz frequency)

// ──────────────────────────────────────────────────────────
int  clampDac(int v)        { return constrain(v, 0, DAC_HARD_MAX); }
int  dacPin(int dir)        { return (dir == LEFT) ? DAC_LEFT : DAC_RIGHT; }
int  targetDac(int dir)     { return (dir == LEFT) ? DAC_TARGET_LEFT : DAC_TARGET_RIGHT; }

void allOff() {
  dacWrite(DAC_LEFT, 0);
  dacWrite(DAC_RIGHT, 0);
  digitalWrite(HSA, LOW);
  digitalWrite(HSB, LOW);
}

float readCurrentmA() {
  if (!USE_MONITOR) return -1.0;
  uint32_t totalMv = 0;
  const int samples = 8;
  for (int i = 0; i < samples; i++) {
    totalMv += analogReadMilliVolts(SENSE);
    delayMicroseconds(50);
  }
  float avgMv = (float)totalMv / samples;
  return avgMv / RE_OHMS; // I(mA) = mV / Re(Ω)
}

void emergencyStop(float mA) {
  allOff();
  Serial.print("!! OVER-CURRENT ");
  Serial.print(mA, 2);
  Serial.println(" mA — HALTED. Press reset to restart.");
  while (true) { allOff(); delay(1000); }   // stay safe until reset
}

void guardCurrent() {
  if (!USE_MONITOR) return;
  float mA = readCurrentmA();
  if (mA > I_LIMIT_MA) emergencyStop(mA);
}

// Bring up ONE direction from a fully-off state (DAC still 0 while HS closes)
void enableDirection(int dir) {
  allOff();                        // both high-sides open, both DACs 0
  delay(DEADTIME_MS);              // guaranteed zero-current gap
  digitalWrite(dir == LEFT ? HSA : HSB, HIGH);
  delay(20);                       // let the high-side settle
}

// Smoothly move the ACTIVE direction's DAC from -> to
void ramp(int dir, int fromDac, int toDac, int ms) {
  int steps = abs(toDac - fromDac);
  if (steps == 0) { dacWrite(dacPin(dir), clampDac(toDac)); return; }
  int stepDelay = max(1, ms / steps);
  int inc = (toDac > fromDac) ? 1 : -1;
  for (int v = fromDac; v != toDac; v += inc) {
    dacWrite(dacPin(dir), clampDac(v));
    guardCurrent();                // check every step
    delay(stepDelay);
  }
  dacWrite(dacPin(dir), clampDac(toDac));
  guardCurrent();
}

// Hold at full while monitoring + reporting
void hold(int dir, int ms) {
  unsigned long t0 = millis(), lastPrint = 0;
  while (millis() - t0 < ms) {
    guardCurrent();
    if (millis() - lastPrint > 1000) {
      lastPrint = millis();
      float mA = readCurrentmA();
      Serial.print(dir == LEFT ? "LEFT  hold " : "RIGHT hold ");
      if (USE_MONITOR) {
        Serial.print(mA, 2); Serial.print(" mA");
        if (mA < I_LOW_MA) Serial.print("  <-- low: check electrode contact / impedance");
        Serial.println();
      } else Serial.println("(monitor off)");
    }
    delay(50);
  }
}

void cycleDirection(int dir) {
  Serial.println(dir == LEFT ? "== LEFT ==" : "== RIGHT ==");
  enableDirection(dir);                       // safe start, correct high-side on
  ramp(dir, 0, targetDac(dir), RAMP_UP_MS);   // 0 -> full, slowly
  hold(dir, HOLD_MS);                         // dwell at 1.5 mA
  ramp(dir, targetDac(dir), 0, RAMP_DOWN_MS); // full -> 0, slowly
  allOff();                                   // open the high-side
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);            // quiet the 3.3 V rail so the DAC is steady
  btStop();                      // (remove this line if it won't compile)
  pinMode(HSA, OUTPUT);
  pinMode(HSB, OUTPUT);
  allOff();                      // ALWAYS start in the safe state
  if (USE_MONITOR) analogSetPinAttenuation(SENSE, ADC_11db);  // read up to ~3 V
  delay(500);
  Serial.println("GVS bridge ready. Starting slow left/right cycle.");
}

void loop() {
  cycleDirection(LEFT);
  cycleDirection(RIGHT);
}