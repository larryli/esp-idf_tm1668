# TM1668/1638 display driver component for esp-idf

## Installation

    idf.py add-dependency "larryli/tm1668"

## Getting Started

### New tm1668 bus

```c
#include "tm1668.h"

tm1668_bus_config_t tm1668_bus_config = {
    .clk_io_num = CLK_IO_PIN,
    .dio_io_num = DIO_IO_PIN,
    .flags.enable_internal_pullup = true,
};
tm1668_bus_handle_t bus_handle;

tm1668_master_bus(&tm1668_bus_config, &bus_handle);
```

### New tm1668 device

```c
#include "tm1668.h"

tm1668_device_config_t dev_cfg = {
    .stb_io_num = STB_IO_PIN,
    .flags.enable_internal_pullup = true,
};

tm1668_dev_handle_t tm1668_handle;
tm1668_bus_add_device(bus_handle, &dev_cfg, &tm1668_handle);
```

### Display mode setting

```c
tm1668_set_mode(tm1668_handle, TM1668_MODE_7x10);
```

### Set the display brightness and on/off

```c
tm1668_set_pulse(tm1668_handle, TM1668_PULSE_WIDTH_14);
tm1668_display(tm1668_handle, true);
```

### Display on address auto increase mode

```c
uint8_t buf[TM1668_DISPLAY_SIZE] = {0xFF};
tm1668_display_auto(tm1668_handle, 0, buf, sizeof(buf));
```

### Display on fixed address mode

```c
tm1668_display_fixed(tm1668_handle, 0, 0xFF);
```

### Read time sequence of the key

```c
uint8_t key[TM1668_KEY_SIZE];
tm1668_read_key(tm1668_handle, key, sizeof(key));
```
