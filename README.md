# MosKill — 智能电蚊拍

ESP32-C3 驱动的智能电蚊拍，带蓝牙连接、击杀计数与统计分析。

```
  ┌─────────────────────────────┐
  │  ⚡ 2000V Grid (3-layer)    │
  │         │                   │
  │  Current Sense ──▶ ADC      │
  │         │                   │
  │    ESP32-C3-MINI-1          │
  │    ├── BLE 5.0 ──▶ 📱 App  │
  │    ├── Kill Detection       │
  │    ├── Stats Engine (NVS)   │
  │    ├── AHT20 Temp/Humidity  │
  │    ├── WS2812B RGB LED      │
  │    └── Piezo Buzzer         │
  │         │                   │
  │  18650 Li-ion + TP4056 USB-C│
  └─────────────────────────────┘
```

## Features

- **击杀检测** — 电流脉冲分析，4 级分类 (S 小飞虫 / M 蚊子 / L 苍蝇 / XL 飞蛾)
- **统计引擎** — 击杀总数、连杀记录、时段热力图、虫体大小分布、效率评分
- **环境关联** — 温湿度采集，蚊虫活跃度与环境数据交叉分析
- **BLE 5.0** — 实时击杀通知、统计读取、设备配置、OTA 固件升级
- **UI 反馈** — WS2812B 按击杀类型变色，连杀彩虹特效，蜂鸣器音效
- **电源管理** — 18650 锂电 USB-C 充电，5 分钟无操作自动深度睡眠 (5μA)

## Project Structure

```
moskill/
├── docs/DESIGN.md                  # 系统设计文档 (硬件架构、算法、数据结构)
├── protocol/ble_protocol.md        # BLE GATT 协议规范
├── hardware/
│   ├── kicad/                      # KiCad 7 原理图 + PCB (35×60mm 2层板)
│   └── bom/BOM.csv                 # 物料清单 (~¥21)
├── firmware/                       # ESP-IDF 固件
│   ├── main/                       # 入口、UI 驱动、配置、类型定义
│   └── components/
│       ├── kill_detector/          # 击杀检测状态机
│       ├── stats_engine/           # 统计引擎 + NVS 持久化
│       ├── ble_service/            # BLE GATT Server + OTA
│       ├── env_sensor/             # AHT20 I2C 驱动
│       ├── power_mgmt/             # 电池监控 + HV 控制 + 休眠
│       └── hv_driver/              # WS2812B RMT 驱动
└── tools/ble_debug/                # Python BLE 调试工具
```

## Hardware

| Component | Part | Function |
|-----------|------|----------|
| MCU | ESP32-C3-MINI-1 | RISC-V 160MHz, BLE 5.0, 4MB Flash |
| HV Boost | NE555 + SI2302 + EE10 Transformer | 3.7V → ~2000V DC |
| HV Multiplier | 3-stage Cockcroft-Walton | 6× 1N4007 + 6× 1nF/2kV |
| Current Sense | 0.47Ω + BAT54S + LMV358 (G=10) | 击杀波形采集 |
| Env Sensor | AHT20 (I2C) | 温度 ±0.3°C / 湿度 ±2%RH |
| Power | 18650 + TP4056 + ME6211 LDO | USB-C 充电, 3.3V 稳压 |
| Battery Protect | DW01A + FS8205A | 过充/过放/过流保护 |
| LED | WS2812B | RGB 状态指示 + 击杀特效 |
| Buzzer | Passive Piezo | 击杀音效 + 连杀上升音 |

Full schematic: [`hardware/kicad/moskill.kicad_sch`](hardware/kicad/moskill.kicad_sch)

## Build Firmware

```bash
# 安装 ESP-IDF v5.x
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/

cd firmware
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## BLE Debug Tool

```bash
cd tools/ble_debug
pip install -r requirements.txt

# 扫描设备
python moskill_ble.py scan

# 读取统计 (含时段热力图)
python moskill_ble.py stats <BLE_ADDRESS>

# 实时监控击杀
python moskill_ble.py monitor <BLE_ADDRESS>

# 查看击杀日志
python moskill_ble.py log <BLE_ADDRESS>

# 修改配置 (灵敏度 0-2, 蜂鸣器 0-3)
python moskill_ble.py config <BLE_ADDRESS> -s 2 -b 3

# BLE OTA 升级
python moskill_ble.py ota <BLE_ADDRESS> build/moskill.bin
```

## Kill Detection

```
ADC 1kHz sampling → Threshold trigger → Sustain check (≥5ms)
    → Peak capture → Envelope match → Classification → Stats update
    → BLE notify → LED + Buzzer feedback

Classification by peak ADC:
  S  (< 800)  → 小飞虫 / 果蝇
  M  (800-2000) → 蚊子
  L  (2000-3200) → 苍蝇 / 蛾子
  XL (> 3200) → 大型飞蛾 / 甲虫
```

## BLE Services

| Service | UUID | Description |
|---------|------|-------------|
| MosKill | `0x1B00` | Kill count (notify), session/lifetime stats, kill log, environment |
| Battery | `0x180F` | Standard Battery Service |
| Device Info | `0x180A` | Manufacturer, model, firmware version |
| Config | `0x1B10` | Sensitivity, LED, buzzer settings + time sync |
| OTA | `0x1B20` | Firmware update (start/write/verify/abort) |

Full protocol spec: [`protocol/ble_protocol.md`](protocol/ble_protocol.md)

## License

MIT
