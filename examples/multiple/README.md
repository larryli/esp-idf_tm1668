| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

# TM1668 + TM1638 Multi-Device Example

Demonstrates two display/keypad ICs sharing a single CLK/DIO bus, each with its
own STB (chip-select) pin.

> Requires `CONFIG_TM1668_WITH_BUS=y` (enabled by default in `menuconfig`).

## Hardware

| Signal | ESP32 GPIO | ESP32-S2/S3 GPIO |
|--------|-----------|-------------------|
| CLK (shared) | 18 | 11 |
| DIO (shared) | 19 | 12 |
| STB — TM1668 | 5 | 14 |
| STB — TM1638 | 23 | 13 |

## Build & Flash

```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

## Expected Behavior

- **TM1668**: Digits 1–3 on the 7-segment grids. Key presses light the
  individual LEDs at positions 6 and 8.
- **TM1638**: Digits 1–8 on the 7-segment grids. Key presses light the
  corresponding LED indicators.
- Both devices operate independently — pressing keys on one does not affect
  the other's display.
