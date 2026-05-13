| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

# TM1638 Single-Device Example

Demonstrates basic TM1638 usage: 7-segment display output and keypad scanning.

## Hardware

Connect a TM1638 module (e.g. the common "LED & Key" board) to your ESP:

| TM1638 Pin | ESP32 GPIO | ESP32-S2/S3 GPIO |
|-----------|------------|-------------------|
| VCC | 3.3V or 5V | 3.3V or 5V |
| GND | GND | GND |
| STB | 23 | 13 |
| CLK | 18 | 11 |
| DIO | 19 | 12 |

## Build & Flash

```bash
idf.py set-target esp32        # or esp32s3, esp32c3, …
idf.py build
idf.py flash monitor
```

## Expected Behavior

- Digits 1–8 appear on the 7-segment display.
- Press any key on the keypad — the corresponding LED indicator lights up.
- Each key press toggles one of the 8 individual LEDs on the board.
