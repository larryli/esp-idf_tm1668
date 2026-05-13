# TM1668 / TM1638 Display Driver for ESP-IDF

Driver for Titan Micro TM1668 and TM1638 LED display/keypad controller ICs.

These chips drive common-cathode 7-segment LED grids over a custom 3-wire
serial protocol (CLK, DIO, STB) and include built-in keypad matrix scanning.

| Feature | TM1668 | TM1638 |
|---------|--------|--------|
| Max grids × segments | 7 × 10 (configurable) | 8 × 10 (fixed) |
| Display modes | 4×13, 5×12, 6×11, 7×10 | 8×10 only |
| Key matrix | 10 × 2 (5 bytes) | 3 × 8 (4 bytes) |
| Brightness levels | 8 (pulse width) | 8 (pulse width) |
| Display register size | 14 bytes | 16 bytes |

This driver supports both ICs through a shared core (`tm1668.c`) plus a
compatibility header (`tm1638.h`) that provides `tm1638_*` wrappers.

## Hardware

Each device uses three GPIO pins:

| Pin | Direction | Description |
|-----|-----------|-------------|
| CLK | Host → Device | Serial clock (push-pull output) |
| DIO | Bidirectional | Data line (open-drain; device drives it during key scan reads) |
| STB | Host → Device | Strobe / chip-select (push-pull output, active low) |

- All three lines idle **high**.
- Internal pull-ups are enabled by default via `flags.enable_internal_pullup`.
  Disable this if you have external pull-up resistors.
- When using **bus mode** (multiple devices), CLK and DIO are shared; each
  device needs its own STB pin.

## Installation

```bash
idf.py add-dependency "larryli/tm1668"
```

## Configuration

Run `idf.py menuconfig` → **Component config** → **TM1668/TM1638 Display Driver**.

| Option | Default | Description |
|--------|---------|-------------|
| `TM1668_WITH_BUS` | y | Enable shared-bus mode for multiple daisy-chained devices. Disable to reduce code size when using a single device. |
| `TM1668_DELAY_US` | 1 | Half-cycle clock delay in µs (~500 kHz). Increase to 2–5 µs for breadboard wiring or clone chips that need slower timing. |
| `TM1668_READ_KEY_DELAY_US` | 2 | Settling delay (µs) after the READ_KEY command. Increase if key reads return all zeros. |

## Quick Start — Single Device

```c
#include "tm1668.h"

const tm1668_config_t config = {
    .clk_io_num = 18,
    .dio_io_num = 19,
    .stb_io_num = 23,
    .flags.enable_internal_pullup = true,
};
tm1668_dev_handle_t handle;
ESP_ERROR_CHECK(tm1668_new_device(&config, &handle));

/* Write 14 bytes to the display in auto-increment mode, then turn it on. */
uint8_t buf[TM1668_DISPLAY_SIZE] = {0};      /* all segments off */
buf[0] = 0b00111111;                         /* digit "0" on grid 1 */
ESP_ERROR_CHECK(tm1668_display_auto(handle, 0, buf, sizeof(buf)));

ESP_ERROR_CHECK(tm1668_set_pulse(handle, TM1668_PULSE_WIDTH_4));
ESP_ERROR_CHECK(tm1668_display(handle, true));

/* Read the keypad matrix. */
uint8_t keys[TM1668_KEY_SIZE];
ESP_ERROR_CHECK(tm1668_read_key(handle, keys, sizeof(keys)));

/* Teardown */
ESP_ERROR_CHECK(tm1668_del_device(handle));
```

## Quick Start — Multiple Devices on a Shared Bus

```c
#include "tm1668.h"

/* Create a shared bus (CLK + DIO). */
const tm1668_bus_config_t bus_cfg = {
    .clk_io_num = 18,
    .dio_io_num = 19,
    .flags.enable_internal_pullup = true,
};
tm1668_bus_handle_t bus;
ESP_ERROR_CHECK(tm1668_new_bus(&bus_cfg, &bus));

/* Add a TM1668 device — its own STB pin. */
const tm1668_device_config_t dev1_cfg = {
    .stb_io_num = 5,
    .flags.enable_internal_pullup = true,
};
tm1668_dev_handle_t dev1;
ESP_ERROR_CHECK(tm1668_bus_add_device(bus, &dev1_cfg, &dev1));

/* Add a second device (TM1638 compatible). */
const tm1668_device_config_t dev2_cfg = {
    .stb_io_num = 23,
    .flags.enable_internal_pullup = true,
};
tm1668_dev_handle_t dev2;
ESP_ERROR_CHECK(tm1668_bus_add_device(bus, &dev2_cfg, &dev2));

/* Use dev1 and dev2 independently — they share CLK/DIO,
 * but STB selects which chip receives the command. */

/* Teardown: remove devices first, then delete the bus. */
ESP_ERROR_CHECK(tm1668_bus_rm_device(dev1));
ESP_ERROR_CHECK(tm1668_bus_rm_device(dev2));
ESP_ERROR_CHECK(tm1668_del_bus(bus));
```

## TM1638 Usage

Replace `#include "tm1668.h"` with `#include "tm1638.h"`. All function names
mirror their TM1668 equivalents (`tm1638_new_device`, `tm1638_display_auto`, …).

Key differences:

- **No `tm1638_set_mode()`** — the TM1638 has a fixed 8×10 display layout.
  Calling `tm1668_set_mode()` on a TM1638 is silently ignored by the hardware.
- Use `TM1638_DISPLAY_SIZE` (16) instead of `TM1668_DISPLAY_SIZE` (14).
- Use `TM1638_KEY_SIZE` (4) instead of `TM1668_KEY_SIZE` (5).

```c
#include "tm1638.h"

tm1638_dev_handle_t handle;
ESP_ERROR_CHECK(tm1638_new_device(&config, &handle));

/* TM1638 has a 16-byte display buffer (8 grids × 10 segments). */
uint8_t buf[TM1638_DISPLAY_SIZE] = {0};
ESP_ERROR_CHECK(tm1638_display_auto(handle, 0, buf, sizeof(buf)));

uint8_t keys[TM1638_KEY_SIZE];
ESP_ERROR_CHECK(tm1638_read_key(handle, keys, sizeof(keys)));
```

## API Reference

### Display

| Function | Description |
|----------|-------------|
| `tm1668_reset(handle)` | Reset to auto-increment address mode |
| `tm1668_set_mode(handle, mode)` | Set grid × segment mode (TM1668 only) |
| `tm1668_display_auto(handle, addr, data, size)` | Write multiple bytes; address auto-increments |
| `tm1668_display_fixed(handle, addr, data)` | Write a single byte to a fixed address |
| `tm1668_set_pulse(handle, width)` | Set brightness (1/16 … 14/16 duty) |
| `tm1668_display(handle, on_off)` | Turn display on or off |

### Keypad

| Function | Description |
|----------|-------------|
| `tm1668_read_key(handle, data, size)` | Read key matrix state (5 bytes TM1668, 4 bytes TM1638) |

### Lifecycle

| Function | Description |
|----------|-------------|
| `tm1668_new_device(cfg, &handle)` | Create a standalone device |
| `tm1668_del_device(handle)` | Delete a standalone device |
| `tm1668_new_bus(cfg, &bus)` | Create a shared bus |
| `tm1668_bus_add_device(bus, cfg, &handle)` | Add a device to a bus |
| `tm1668_bus_rm_device(handle)` | Remove a device from its bus |
| `tm1668_del_bus(bus)` | Delete a bus (auto-cleans residual devices with a warning) |

### Display Modes (TM1668 only)

| Constant | Grids × Segments |
|----------|-----------------|
| `TM1668_MODE_4x13` | 4 × 13 |
| `TM1668_MODE_5x12` | 5 × 12 |
| `TM1668_MODE_6x11` | 6 × 11 |
| `TM1668_MODE_7x10` | 7 × 10 |

### Pulse Width (Brightness)

Higher values = brighter. `TM1668_PULSE_WIDTH_1` is dimmest, `_14` is brightest.

## Examples

| Example | Description |
|---------|-------------|
| [get-started](examples/get-started/) | Single TM1638, 7-segment digits + key scanning |
| [multiple](examples/multiple/) | TM1668 + TM1638 on a shared bus |

## Troubleshooting

**Display stays dark after setup.**
Make sure you call `tm1668_set_pulse()` **before** `tm1668_display(handle, true)`.
Both functions send the display control command and `display(true)` may override
a zeroed pulse-width default if called first.

**Keypad reads return all zeros.**
Try increasing `TM1668_READ_KEY_DELAY_US` to 4–10 µs. The TM1668 needs time to
start driving DIO after receiving the READ_KEY command. This is especially
common with long wires.

**Display flickers or shows garbage.**
Increase `TM1668_DELAY_US` to 2–5 µs. Breadboard wiring and long traces add
capacitance that slows signal edges. Clone chips may also be slower than
genuine Titan Micro parts.

**TM1638 display shows wrong segments / fewer grids than expected.**
TM1638 has a fixed 8×10 layout; `tm1668_set_mode()` has no effect. Make sure
you are using `TM1638_DISPLAY_SIZE` (16) and `#include "tm1638.h"`.

**Build error: "CONFIG_TM1668_WITH_BUS not defined".**
Run `idf.py menuconfig` and enable TM1668 bus support, or switch to the
standalone API (`tm1668_new_device` / `tm1668_del_device`).

## License

MIT — see [LICENSE](LICENSE).