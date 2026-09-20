# Galvanic Vestibular Stimulation (GVS) System

A compact **ESP32-based Galvanic Vestibular Stimulation (GVS)** prototype designed to generate controlled bidirectional DC current through mastoid electrodes.

The system combines a **discrete BJT H-bridge**, DAC-controlled current sinks, and ADC-based current monitoring to control both the **magnitude and direction of stimulation**.

> ⚠️ **Prototype / Research System:** This project involves electrical stimulation of the human body and is intended for controlled experimental development. It should not be used on humans without appropriate electrical, medical, and safety validation.

---

## System Overview

```text
                 +11.1 V
                    │
              ┌─────┴─────┐
              │ PNP H-Bridge│
              └─────┬─────┘
                    │
               ┌────┴────┐
               │  LOAD   │
               │ (Head)  │
               └────┬────┘
                    │
            NPN Current Sinks
                    │
                  Re 1kΩ
                    │
                   GND
                    │
               ADC Feedback
                    │
                  ESP32
```

---

## Key Features

* **ESP32-based control**
* Bidirectional current through the load
* Dual DAC-based current control
* Discrete **PNP/NPN BJT H-bridge**
* NPN level shifting for high-side control
* `1 kΩ` shared current-sense resistor
* ADC-based current monitoring
* Configurable current ramping
* Deadtime between polarity transitions
* Software fault detection and shutdown

---

## Hardware

| Component        | Specification               |
| ---------------- | --------------------------- |
| Controller       | ESP32                       |
| Supply           | 11.1 V 3S LiPo              |
| High-Side        | 2 × PNP BJT                 |
| Low-Side         | 2 × NPN BJT                 |
| Level Shifting   | 2 × NPN BJT                 |
| Current Sense    | 1 kΩ resistor               |
| Current Range    | ~1.5–2.2 mA prototype range |
| Current Feedback | ESP32 ADC                   |

### ESP32 Pin Mapping

| GPIO    | Function          |
| ------- | ----------------- |
| GPIO 25 | DAC → Q4          |
| GPIO 26 | DAC → Q3          |
| GPIO 32 | High-Side A       |
| GPIO 33 | High-Side B       |
| GPIO 34 | Current Sense ADC |

---

## Operating Modes

### LEFT

```text
+11.1 V → Q1 → Node A → LOAD → Node B → Q4 → Re → GND
```

Current flows from **Node A → Node B**.

### RIGHT

```text
+11.1 V → Q2 → Node B → LOAD → Node A → Q3 → Re → GND
```

Current flows from **Node B → Node A**.

### NEUTRAL

All bridge devices are disabled and the intended load current is approximately **0 mA**.

---

## Current Control

The low-side NPN transistors operate as voltage-controlled current sinks.

With `Re = 1 kΩ`:

```text
V_Re ≈ V_DAC - V_BE

I_sink ≈ (V_DAC - 0.7 V) / 1000 Ω
```

The ESP32 also monitors the voltage across `Re` through **GPIO 34** to estimate the actual current.

---

## Safety Features

The firmware implements:

* **Deadtime** during polarity switching
* **Controlled current ramping**
* Continuous current monitoring
* Fault threshold detection
* Immediate `allOff()` shutdown on detected overcurrent

Hardware-level current limiting and independent safety mechanisms are recommended for any human-connected implementation.

---

## Project Status

**Prototype — Under Development**

Current work focuses on:

* Hardware validation
* Current regulation accuracy
* Bidirectional switching
* ADC feedback and fault detection
* Firmware refinement
* System-level testing

---

## License

This project is intended for **research and educational purp**
