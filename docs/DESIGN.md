# MosKill - Smart Electric Mosquito Swatter

## System Overview

ESP32-C3 based smart mosquito swatter with BLE connectivity, real-time kill detection via current pulse analysis, and comprehensive kill statistics including waveform classification, environmental correlation, and efficiency scoring.

```
┌─────────────────────────────────────────────────────┐
│                   MosKill System                    │
│                                                     │
│  ┌──────────┐   ┌──────────┐   ┌───────────────┐   │
│  │ HV Grid  │──▶│ Current  │──▶│   ESP32-C3    │   │
│  │ 3-layer  │   │ Sensing  │   │               │   │
│  └──────────┘   └──────────┘   │  ┌─────────┐  │   │
│                                │  │Kill Det. │  │   │
│  ┌──────────┐                  │  ├─────────┤  │   │
│  │ Boost    │◀─────────────────│  │Stats Eng│  │   │
│  │ Converter│   Control        │  ├─────────┤  │   │
│  └──────────┘                  │  │BLE Stack│  │   │
│                                │  ├─────────┤  │   │
│  ┌──────────┐                  │  │Power Mgr│  │   │
│  │ AHT20    │─────────────────▶│  ├─────────┤  │   │
│  │Temp/Humi │   I2C            │  │Env Sense│  │   │
│  └──────────┘                  │  └─────────┘  │   │
│                                └───────────────┘   │
│  ┌──────────┐   ┌──────────┐                       │
│  │ Li-ion   │──▶│ LDO 3.3V │──▶ System Power       │
│  │ 18650    │   │ ME6211   │                       │
│  └──────────┘   └──────────┘                       │
└─────────────────────────────────────────────────────┘
```

## 1. Hardware Architecture

### 1.1 MCU - ESP32-C3-MINI-1

| Parameter | Value |
|-----------|-------|
| Core | RISC-V 32-bit, 160MHz |
| Flash | 4MB |
| SRAM | 400KB |
| BLE | 5.0 |
| ADC | 12-bit SAR, 6 channels |
| GPIO | 22 pins |
| Deep Sleep | 5μA |
| Package | ESP32-C3-MINI-1 module |

#### Pin Assignment

| Pin | Function | Description |
|-----|----------|-------------|
| GPIO0 | ADC_CH0 | Current sense input (via analog frontend) |
| GPIO1 | ADC_CH1 | Battery voltage divider |
| GPIO2 | BOOT | Boot mode (active low) |
| GPIO3 | ADC_CH3 | Env sensor aux (optional) |
| GPIO4 | I2C_SDA | AHT20 data |
| GPIO5 | I2C_SCL | AHT20 clock |
| GPIO6 | HV_EN | High voltage boost enable |
| GPIO7 | LED_STATUS | Status LED (RGB WS2812B) |
| GPIO8 | BUZZER | Piezo buzzer PWM |
| GPIO9 | BUTTON | Main button (active low, internal pull-up) |
| GPIO10 | CHG_STAT | TP4056 charge status |
| GPIO18 | USB_D- | USB-C data |
| GPIO19 | USB_D+ | USB-C data |
| GPIO20 | UART_RX | Debug UART |
| GPIO21 | UART_TX | Debug UART |

### 1.2 High Voltage System

#### Boost Converter (Battery → HV)

```
Li-ion 3.7V ──▶ Oscillator + Transformer ──▶ Voltage Multiplier ──▶ ~2000V DC
                     │                              │
                  GPIO6 EN                    Current Sense
                                                    │
                                              To Analog Frontend
```

**Oscillator**: NE555 astable driving MOSFET gate
- R_A = 4.7kΩ, R_B = 10kΩ, C_t = 10nF
- f = 1.44 / ((R_A + 2×R_B) × C_t) = 1.44 / ((4.7k + 2×10k) × 10nF) = **5.83kHz**
- Duty cycle = (R_A + R_B) / (R_A + 2×R_B) = 14.7k / 24.7k = **59.5%**

**MOSFET**: AO3400 (N-ch, Vds=30V, Rds_on=40mΩ, Id=5.7A, SOT-23)
- RC snubber across drain-source: **100Ω + 1nF** (suppresses ringing)
- TVS across drain-source: **SMBJ24A** (24V standoff, 38.9V clamp, flyback protection)

**Transformer**: EE13 or EE16 core, **10:200 winding ratio (1:20 effective)**
- 10 primary turns: inductance ~200μH
- Peak primary current: I = V × t_on / L = 3.7V × 100μs / 200μH = **1.85A** (safe for AO3400)
- 200 secondary turns: V_sec_peak = V_in × N_s/N_p = 3.7 × 20 = **~740V peak** at 3.7V input

**Voltage Multiplier**: Cockcroft-Walton 3-stage
- C: 1nF 2kV rated (C0G dielectric)
- D: **UF4007** (fast recovery, trr=75ns, 1kV, 1A) — standard 1N4007 too slow for 5.8kHz switching
- No-load output: 6 × 740V = 4440V theoretical × 0.7 efficiency ≈ **3100V**
- Loaded output (grid + insect): **~2000V DC** target achieved
- Design margin: Vout adjustable via duty cycle / frequency trim

**Grid**: 3-layer structure
- Outer layers: Ground reference
- Inner layer: High voltage
- Spacing: 3-4mm

#### Current Sensing Analog Frontend

```
HV Return ──┤R_sense├──▶ GND
                │
                ▼
         ┌──────────────┐
         │  R_sense      │  0.1Ω 2W (gives 100mV at 1A, manageable range)
         │  voltage drop │
         └──────┬───────┘
                │
         ┌──────▼───────┐
         │ Series Limit  │  1kΩ resistor BEFORE clamp (limits inrush to clamp diodes)
         │ Resistor      │
         └──────┬───────┘
                │
         ┌──────▼───────┐
         │ Clamping      │  BAT54S Schottky clamp to 0-3.3V
         │ Protection    │
         └──────┬───────┘
                │
         ┌──────▼───────┐
         │ Backup Zener  │  BZX84C3V3 (3.3V Zener, SOT-23) — secondary hard clamp
         └──────┬───────┘
                │
         ┌──────▼───────┐
         │ Op-Amp Stage  │  LMV358 (rail-to-rail, 3.3V)
         │ Gain = 20     │  Non-inverting: Rf=19kΩ, Rg=1kΩ
         │ BW filter     │  RC LPF at 10kHz (anti-alias)
         └──────┬───────┘
                │
         ┌──────▼───────┐
         │ Peak Detector │  BAT43 Schottky (Vf≈0.2V, preserves small signals)
         │ + Envelope    │  + RC hold: 1μF × 100kΩ = τ = 100ms
         └──────┬───────┘
                │
                ▼
           GPIO0 (ADC)     12-bit, sampled at 1kHz during active mode
```

**Design Notes:**
- R_sense = 0.1Ω 2W: produces 10-100mV at typical kill currents (100mA-1A peak)
- 1kΩ series resistor before BAT54S clamp limits fault current into protection diodes
- BZX84C3V3 Zener backup provides hard 3.3V clamp if Schottky clamp fails
- LMV358 gain of 20x (Rf=19kΩ, Rg=1kΩ) maps 0-165mV to 0-3.3V, linear up to 1.65A
- RC filter (R=1.5kΩ, C=10nF → fc ≈ 10.6kHz) for anti-aliasing
- Peak detector uses BAT43 Schottky (low Vf ≈ 0.2V) to preserve small signals
- Peak detector with 1μF + 100kΩ gives τ = 100ms — fast enough for consecutive kills

### 1.3 Power System

#### Battery
- Cell: 18650 Li-ion, 2600mAh
- Nominal: 3.7V (range 3.0V-4.2V)
- Protection: DW01A + FS8205A (overcurrent, overdischarge, overcharge)

#### Charging
- IC: TP4056 with DW01A
- Input: USB-C (5V) with **SS34 Schottky** in series for VBUS reverse protection
- USB ESD protection: **USBLC6-2SC6** on D+/D- lines
- Input capacitor: **4.7μF** (per TP4056 datasheet, 100nF insufficient for USB cable inductance)
- TEMP pin: connected through **10kΩ to VCC** (NOT to GND — GND disables charging)
- Charge current: 500mA (R_prog = 2kΩ)
- Status: CHRG pin → GPIO10

#### LDO Regulator
- IC: ME6211C33 (3.3V, 500mA, low Iq)
- Decoupling: 10μF + 100nF at output
- Enable: Always on (tied to battery via switch)

#### Battery Monitoring
- Voltage divider: **10kΩ / 10kΩ** → GPIO1 (ADC), with **100nF** filter cap at midpoint
  - Lower impedance reduces ADC noise and leakage current errors
  - Quiescent draw: ~0.2mA (acceptable vs. 100kΩ divider's noise issues)
- Reading: V_bat = ADC_reading × 2 × 3.3 / 4095
- Calibrated curve for SoC estimation

### 1.4 Environmental Sensor

- IC: AHT20 (I2C, 0x38)
- Temperature: -40°C to +85°C, ±0.3°C
- Humidity: 0-100% RH, ±2%
- Power: 2.2-5.5V, typ 23μA
- Sample rate: Every 30 seconds (low duty)

### 1.5 User Interface

#### Status LED (WS2812B)
| State | Color | Pattern |
|-------|-------|---------|
| Idle | Off | - |
| Armed (HV on) | Red | Steady |
| Kill detected | Green | Flash 200ms |
| Multi-kill streak | Rainbow | Chase 500ms |
| BLE connected | Blue | Breathe |
| Charging | Orange | Pulse |
| Low battery | Red | Blink slow |

#### Buzzer (Passive piezo)
- Kill confirm: 2kHz, 50ms
- Streak (3+): Rising tone
- Low battery: 1kHz, 3× beep
- BLE connect: 2-tone chime

#### Button
- Short press: Toggle HV on/off
- Long press (3s): BLE advertising toggle
- Double press: Read stats via buzzer pattern

### 1.6 PCB Specifications

| Parameter | Value |
|-----------|-------|
| Dimensions | 35mm × 60mm (fits in swatter handle) |
| Layers | 2 |
| Copper | 1oz outer |
| Finish | HASL lead-free |
| Min trace | 0.2mm (8mil) |
| Min drill | 0.3mm |
| HV clearance | ≥ 4mm between HV traces and LV |
| Substrate | FR-4 1.6mm |

**Layout Zones:**
```
┌─────────────────────────────┐
│  USB-C    │   ESP32-C3      │
│  TP4056   │   Module        │
│  Charging │                 │
├───────────┤   BLE Antenna   │
│  Battery  │   Keep-out      │
│  Mgmt     │   Zone          │
├───────────┼─────────────────┤
│  Analog Frontend            │
│  (Ground guarded)           │
├─────────────────────────────┤
│  HV Section                 │
│  (Isolated, 4mm clearance)  │
│  NE555 + MOSFET + Xformer   │
│  Voltage Multiplier         │
│  Routing slots between zones│
│  Conformal coating required │
└─────────────────────────────┘
```

## 2. Firmware Architecture

### 2.1 RTOS Task Structure (ESP-IDF / FreeRTOS)

```
┌─────────────────────────────────────────────┐
│              FreeRTOS Tasks                  │
│                                              │
│  ┌────────────┐  Priority: 5 (Highest)       │
│  │ kill_detect │  Core 0, 4KB stack           │
│  │ _task       │  1kHz ADC sampling           │
│  └─────┬──────┘  → kill event queue           │
│        │                                      │
│  ┌─────▼──────┐  Priority: 4                  │
│  │ stats_task  │  Core 0, 8KB stack            │
│  │             │  Process kill events          │
│  └─────┬──────┘  → update stats, flash write  │
│        │                                      │
│  ┌─────▼──────┐  Priority: 3                  │
│  │ ble_task    │  Core 0, 6KB stack            │
│  │             │  GATT server, notifications   │
│  └────────────┘                               │
│                                               │
│  ┌────────────┐  Priority: 2                  │
│  │ env_task    │  Core 0, 2KB stack            │
│  │             │  AHT20 polling every 30s      │
│  └────────────┘                               │
│                                               │
│  ┌────────────┐  Priority: 1                  │
│  │ ui_task     │  Core 0, 2KB stack            │
│  │             │  LED + buzzer + button        │
│  └────────────┘                               │
│                                               │
│  ┌────────────┐  Priority: 0 (Lowest)         │
│  │ power_task  │  Core 0, 2KB stack            │
│  │             │  Battery monitoring, sleep    │
│  └────────────┘                               │
└─────────────────────────────────────────────┘
```

### 2.2 Kill Detection Algorithm

```
State Machine:
                    ┌──────────┐
            ┌──────▶│  IDLE    │◀─────────────┐
            │       └────┬─────┘              │
            │            │ ADC > threshold_low │
            │            ▼                     │
            │       ┌──────────┐              │
            │       │ TRIGGERED│              │
            │       └────┬─────┘              │
            │            │ sustain > 5ms       │
            │            ▼                     │
            │       ┌──────────┐   timeout    │
            │       │CONFIRMING├──────────────┘
            │       └────┬─────┘
            │            │ peak captured +
            │            │ envelope matches
            │            ▼
            │       ┌──────────┐
            └───────┤KILL_EVENT│
                    │ → queue  │
                    └──────────┘

Parameters:
- threshold_low:  200 (ADC counts, ~160mV → ~16mV at sense resistor)
- threshold_high: 3800 (ADC counts, clipping protection)
- min_sustain:    5ms (debounce, reject static/EMI)
- max_duration:   500ms (reject continuous shorts)
- cooldown:       200ms (between consecutive kills)
- envelope_match: rise < 2ms, hold 5-50ms, decay < 100ms
```

### 2.3 Kill Classification

Based on discharge waveform characteristics:

| Class | Peak ADC | Duration | Energy Proxy | Likely Target |
|-------|----------|----------|-------------- |---------------|
| S (Small) | 200-800 | 5-15ms | Low | Fruit fly, gnat |
| M (Medium) | 800-2000 | 10-30ms | Medium | Mosquito |
| L (Large) | 2000-3200 | 20-80ms | High | House fly, moth |
| XL (Extra) | 3200+ | 50-200ms | Very High | Large moth, beetle |

Energy proxy = Σ(ADC_sample²) over event duration (proportional to I²t)

### 2.4 Statistics Engine

#### Data Structures (NVS Storage)

```c
// Per-session stats (RAM, flushed to NVS on session end)
typedef struct {
    uint32_t session_id;
    uint32_t start_time;        // epoch seconds
    uint32_t duration_sec;
    uint16_t kills_total;
    uint16_t kills_by_class[4]; // S, M, L, XL
    uint16_t max_streak;
    uint16_t current_streak;
    float    avg_temp;
    float    avg_humidity;
    uint32_t energy_total;      // Σ energy proxy
    uint16_t hv_on_time_sec;
    uint16_t swings_estimated;  // from accelerometer-free proxy (HV on/off cycles)
} session_stats_t;

// Lifetime aggregates (NVS, updated on session end)
typedef struct {
    uint32_t total_kills;
    uint32_t kills_by_class[4];
    uint32_t total_sessions;
    uint32_t total_active_sec;
    uint32_t best_streak;
    uint32_t best_session_kills;
    float    kill_rate_per_hour; // rolling average
    uint8_t  hourly_histogram[24];  // kills per hour-of-day
    uint8_t  daily_histogram[7];    // kills per day-of-week
} lifetime_stats_t;

// Kill event log (circular buffer in NVS, last 256 kills)
typedef struct {
    uint32_t timestamp;
    uint8_t  kill_class;        // S=0, M=1, L=2, XL=3
    uint16_t peak_adc;
    uint16_t duration_ms;
    uint32_t energy_proxy;
    int8_t   temperature;       // °C, rounded
    uint8_t  humidity;          // %RH, rounded
} kill_event_t;
```

#### Derived Metrics (computed on read, not stored)

- **Kill Rate**: kills / HV-on-hours
- **Efficiency Score**: kills / (HV-on-time × energy) — higher = more efficient swatter
- **Peak Hour**: hour-of-day with most kills historically
- **Mosquito Pressure Index**: kills/hour normalized by temp/humidity (higher temp+humidity = more mosquitoes expected, so fewer kills ≠ fewer mosquitoes)
- **Streak Record**: longest consecutive kills within 10s window
- **Size Distribution**: pie chart data of S/M/L/XL

## 3. BLE Protocol

See [protocol/ble_protocol.md](../protocol/ble_protocol.md) for full specification.

### GATT Service Summary

| Service | UUID | Description |
|---------|------|-------------|
| MosKill Service | `0x1B00` | Primary service |
| Kill Count | `0x1B01` | Total kills (notify on change) |
| Session Stats | `0x1B02` | Current session (read) |
| Lifetime Stats | `0x1B03` | All-time stats (read) |
| Kill Log | `0x1B04` | Recent kills (read, paginated) |
| Environment | `0x1B05` | Temp + humidity (notify) |
| Battery | `0x180F` | Standard BAS |
| Device Info | `0x180A` | Standard DIS |
| Config | `0x1B10` | Sensitivity, LED, buzzer settings |
| Time Sync | `0x1B11` | Set RTC from phone |
| OTA | `0x1B20` | Firmware update |

## 4. Bill of Materials (Key Components)

| # | Component | Part | Qty | Est. Cost (¥) |
|---|-----------|------|-----|---------------|
| 1 | MCU Module | ESP32-C3-MINI-1 | 1 | 9.00 |
| 2 | Temp/Humidity | AHT20 | 1 | 2.50 |
| 3 | LDO | ME6211C33M5G | 1 | 0.30 |
| 4 | Charge IC | TP4056 | 1 | 0.40 |
| 5 | Battery Protect | DW01A + FS8205A | 1+1 | 0.50 |
| 6 | Op-Amp | LMV358 | 1 | 0.80 |
| 7 | HV MOSFET | AO3400 (N-ch, 30V, 40mΩ) | 1 | 0.35 |
| 8 | Timer | NE555 | 1 | 0.30 |
| 9 | HV Transformer | EE13/EE16 10:200 | 1 | 2.00 |
| 10 | HV Diodes | UF4007 (fast recovery) | 6 | 0.60 |
| 11 | HV Caps | 1nF 2kV | 6 | 0.60 |
| 12 | Sense Resistor | 0.1Ω 2W | 1 | 0.15 |
| 13 | Clamp Diode | BAT54S | 1 | 0.20 |
| 14 | Peak Det. Diode | BAT43 (Schottky) | 1 | 0.15 |
| 15 | Backup Zener | BZX84C3V3 | 1 | 0.10 |
| 16 | TVS Diode | SMBJ24A | 1 | 0.50 |
| 17 | RGB LED | WS2812B | 1 | 0.30 |
| 18 | Buzzer | Passive piezo | 1 | 0.50 |
| 19 | USB-C Connector | 16-pin | 1 | 0.50 |
| 20 | USB Reverse Prot. | SS34 Schottky | 1 | 0.30 |
| 21 | USB ESD Prot. | USBLC6-2SC6 | 1 | 0.50 |
| 22 | Discharge MOSFET | 2N7002 (N-ch) | 1 | 0.10 |
| 23 | Button | 6×6mm tactile | 1 | 0.10 |
| 24 | HV Bleed Resistors | 1MΩ 0805 200V | 10 | 0.20 |
| 25 | Snubber R | 100Ω 0402 | 1 | 0.01 |
| 26 | Snubber C | 1nF 0402 | 1 | 0.01 |
| 27 | Discharge R | 100Ω 0805 | 1 | 0.01 |
| 28 | Passives | R, C assorted | ~30 | 1.00 |
| 29 | PCB | 2-layer 35×60mm | 1 | 3.00 |
| | **Total** | | | **~¥24.48** |

(Battery 18650 and grid/handle not included — mechanical parts)

## 5. Safety Considerations

- HV section isolated with **4mm clearance** on PCB
- **PCB routing slots** between HV and LV zones to increase creepage distance
- **Conformal coating** required on HV section (silicone or acrylic, IPC-CC-830)
- Grid auto-discharge via bleed resistor chain (see below)
- Overcurrent protection via sense resistor + software cutoff
- **USB ESD protection**: USBLC6-2SC6 on D+/D- lines
- **USB reverse protection**: SS34 Schottky in series with VBUS
- Battery protection IC prevents overcurrent/undervoltage/overvoltage
- Transformer limited by oscillator duty cycle — cannot exceed ~3100V no-load
- Ground layer pour on LV section; separate ground pour for analog frontend

### 5.1 Safety Circuits

#### Hardware Watchdog on HV_EN

External RC timeout circuit prevents indefinite HV operation if MCU crashes:

```
GPIO6 (HV_EN) ──── 100kΩ ────┬──── HV Enable (gate of enable MOSFET)
                              │
                            10μF
                              │
                             GND
```

- MCU must output **PWM** (not static HIGH) to keep HV armed
- RC time constant: τ = 100kΩ × 10μF = **1 second**
- If MCU crashes (GPIO goes static or Hi-Z), cap discharges in ~1s and HV turns off automatically
- PWM frequency: ~100Hz minimum to maintain cap charge above threshold

#### Grid Rapid Discharge

When HV is disabled, residual grid charge must be drained quickly:

```
HV Grid ──── 100Ω ────┬──── Drain of 2N7002
                       │
                      GND (via 2N7002 source)

2N7002 Gate ──── Inverted HV_EN (via NPN inverter or spare NE555 section)
```

- **2N7002** N-MOSFET (Vds=60V, Id=115mA, SOT-23) shorts grid through 100Ω when HV_EN is LOW
- Discharge time constant depends on grid capacitance (~10-50pF): effectively instant
- 100Ω limits peak discharge current to protect MOSFET and grid conductors

#### Bleed Resistor Chain

- **10× 1MΩ** resistors in series (each 0805 package, 200V rated per resistor)
- Total: 10MΩ at 2kV — each resistor sees only 200V (within 0805 rating)
- Provides passive discharge path: τ = 10MΩ × ~30pF (grid) ≈ 0.3ms
- Serves as backup to active discharge circuit

#### HV Clearance Requirements

| Parameter | Requirement |
|-----------|-------------|
| PCB trace clearance (HV to LV) | ≥ 4mm |
| Routing slots | Required between HV and LV zones |
| Conformal coating | Silicone or acrylic per IPC-CC-830 |
| Component placement | No LV components within 4mm of HV traces |
| Grid wire insulation | Silicone sleeving on HV connections to PCB |
