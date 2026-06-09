# MosKill BLE Protocol Specification v1.0

## Overview

BLE 5.0 GATT-based protocol for MosKill smart mosquito swatter.
Device acts as GATT Server (Peripheral). Phone app acts as GATT Client (Central).

## Advertising

| Parameter | Value |
|-----------|-------|
| Name | `MosKill-XXXX` (last 4 of MAC) |
| Interval | 200ms (fast) / 1000ms (slow, after 30s) |
| TX Power | 0 dBm |
| Flags | LE General Discoverable + BR/EDR Not Supported |
| Service UUID | `0x1B00` in Complete List of 16-bit UUIDs |
| Manufacturer Data | `[0xFF, 0xFF, kill_count_lo, kill_count_hi, battery_pct, flags]` |

**Flags byte:**
| Bit | Meaning |
|-----|---------|
| 0 | HV armed |
| 1 | Charging |
| 2 | Low battery |
| 3-7 | Reserved |

## GATT Services & Characteristics

### 1. MosKill Service (`0x1B00`)

#### 1.1 Kill Count (`0x1B01`)
| Property | Value |
|----------|-------|
| Properties | Read, Notify |
| Format | uint32 (little-endian) |
| Description | Total kills in current session |
| CCCD | `0x2902` (enable notifications) |

Notification sent on each kill event. Client subscribes to get real-time kill feed.

#### 1.2 Session Stats (`0x1B02`)
| Property | Value |
|----------|-------|
| Properties | Read |
| Format | Custom struct (see below) |
| Max length | 40 bytes |

```
Offset  Size  Field
0       4     session_id (uint32)
4       4     start_time (uint32, epoch)
8       4     duration_sec (uint32)
12      2     kills_total (uint16)
14      2     kills_S (uint16)
16      2     kills_M (uint16)
18      2     kills_L (uint16)
20      2     kills_XL (uint16)
22      2     max_streak (uint16)
24      4     energy_total (uint32)
28      2     avg_temp_x10 (int16, °C×10)
30      2     avg_humidity_x10 (uint16, %×10)
32      2     hv_on_time_sec (uint16)
34      2     efficiency_x100 (uint16, score×100)
36      4     reserved
```

#### 1.3 Lifetime Stats (`0x1B03`)
| Property | Value |
|----------|-------|
| Properties | Read |
| Format | Custom struct |
| Max length | 80 bytes |

```
Offset  Size  Field
0       4     total_kills (uint32)
4       4     kills_S (uint32)
8       4     kills_M (uint32)
12      4     kills_L (uint32)
16      4     kills_XL (uint32)
20      4     total_sessions (uint32)
24      4     total_active_sec (uint32)
28      4     best_streak (uint32)
32      4     best_session_kills (uint32)
36      4     kill_rate_x100 (uint32, per-hour×100)
40      24    hourly_histogram[24] (uint8×24)
64      7     daily_histogram[7] (uint8×7, Mon=0)
71      1     padding
72      4     first_use_time (uint32, epoch)
76      4     reserved
```

#### 1.4 Kill Log (`0x1B04`)
| Property | Value |
|----------|-------|
| Properties | Read |
| Format | Custom struct, paginated |
| Max length | 200 bytes per read |

**Read with offset**: Client writes page number (uint8) to trigger read.
Each page = 10 kill events, newest first.

```
Per-event (16 bytes):
Offset  Size  Field
0       4     timestamp (uint32, epoch)
4       1     kill_class (uint8: 0=S, 1=M, 2=L, 3=XL)
5       2     peak_adc (uint16)
7       2     duration_ms (uint16)
9       4     energy_proxy (uint32)
13      1     temperature (int8, °C)
14      1     humidity (uint8, %)
15      1     reserved

Page header (4 bytes):
0       1     page_number (uint8)
1       1     total_pages (uint8)
2       2     events_in_page (uint16)

Total per page: 4 + 10×16 = 164 bytes
```

#### 1.5 Environment (`0x1B05`)
| Property | Value |
|----------|-------|
| Properties | Read, Notify |
| Format | 4 bytes |
| Notify interval | 30 seconds |

```
Offset  Size  Field
0       2     temperature_x10 (int16, °C×10)
2       2     humidity_x10 (uint16, %RH×10)
```

### 2. Battery Service (`0x180F`) — Standard BAS

#### 2.1 Battery Level (`0x2A19`)
| Property | Value |
|----------|-------|
| Properties | Read, Notify |
| Format | uint8 (0-100%) |
| Notify | On 5% change |

### 3. Device Information Service (`0x180A`) — Standard DIS

| Characteristic | UUID | Value |
|---------------|------|-------|
| Manufacturer | `0x2A29` | `"MosKill"` |
| Model Number | `0x2A24` | `"MK-01"` |
| Firmware Rev | `0x2A26` | `"1.0.0"` |
| Hardware Rev | `0x2A27` | `"1.0"` |
| Serial Number| `0x2A25` | MAC-derived |

### 4. Configuration Service (`0x1B10`)

#### 4.1 Device Config (`0x1B11`)
| Property | Value |
|----------|-------|
| Properties | Read, Write |
| Format | 8 bytes |

```
Offset  Size  Field
0       1     kill_sensitivity (uint8: 0=low, 1=med, 2=high)
1       1     led_brightness (uint8: 0-255)
2       1     buzzer_volume (uint8: 0=off, 1=low, 2=med, 3=high)
3       1     buzzer_on_kill (uint8: 0=off, 1=on)
4       1     led_on_kill (uint8: 0=off, 1=on)
5       1     streak_effects (uint8: 0=off, 1=on)
6       2     reserved
```

#### 4.2 Time Sync (`0x1B12`)
| Property | Value |
|----------|-------|
| Properties | Write |
| Format | 4 bytes |

Write uint32 epoch seconds to sync internal RTC.

### 5. OTA Service (`0x1B20`)

#### 5.1 OTA Control (`0x1B21`)
| Property | Value |
|----------|-------|
| Properties | Write, Notify |
| Format | Variable |

| Command (byte 0) | Payload | Description |
|-------------------|---------|-------------|
| `0x01` | `[total_size: uint32]` | Start OTA, send firmware size |
| `0x02` | `[offset: uint32, data: bytes]` | Write chunk (max 240 bytes) |
| `0x03` | `[crc32: uint32]` | Verify and apply |
| `0x04` | - | Abort OTA |

Notification responses:
| Status | Meaning |
|--------|---------|
| `0x00` | OK / ACK |
| `0x01` | CRC error |
| `0x02` | Flash error |
| `0x03` | Size mismatch |
| `0xFF` | Abort |

## Connection Parameters

| Parameter | Value |
|-----------|-------|
| Min Interval | 15ms |
| Max Interval | 30ms (active) / 500ms (idle) |
| Slave Latency | 0 (active) / 4 (idle) |
| Timeout | 4000ms |
| MTU | Request 247 (for kill log pages) |

## Security

| Parameter | Value |
|-----------|-------|
| Pairing | Just Works (no MITM needed for a swatter) |
| Bonding | Yes (remember paired devices) |
| Encryption | AES-CCM (BLE 4.2+ standard) |
| Max bonds | 4 devices |

## Data Flow

```
Kill Event:
  Grid discharge → ADC → kill_detect_task → kill_queue
       → stats_task (update session/lifetime, write NVS)
       → ble_task (notify kill count char 0x1B01)
       → ui_task (LED flash + buzzer beep)

Phone Connection:
  Phone → scan → connect → discover services
       → subscribe to 0x1B01 (kill count)
       → subscribe to 0x1B05 (environment)
       → subscribe to 0x2A19 (battery)
       → read 0x1B02 (session stats)
       → read 0x1B03 (lifetime stats)
       → paginate 0x1B04 (kill log)
       → write 0x1B12 (time sync)
       → write 0x1B11 (config)
```
