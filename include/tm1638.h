#pragma once

#include "tm1668.h"

#ifdef CONFIG_TM1668_WITH_BUS
typedef tm1668_bus_config_t tm1638_bus_config_t;
typedef tm1668_bus_handle_t tm1638_bus_handle_t;
typedef tm1668_device_config_t tm1638_device_config_t;
#endif // CONFIG_TM1668_WITH_BUS
typedef tm1668_dev_handle_t tm1638_dev_handle_t;
typedef tm1668_config_t tm1638_config_t;

#ifdef CONFIG_TM1668_WITH_BUS
static inline esp_err_t tm1638_new_bus(const tm1638_bus_config_t *bus_config,
                                       tm1638_bus_handle_t *ret_bus_handle)
{
    return tm1668_new_bus(bus_config, ret_bus_handle);
}

static inline esp_err_t
tm1638_bus_add_device(tm1638_bus_handle_t bus_handle,
                      const tm1638_device_config_t *dev_config,
                      tm1638_dev_handle_t *ret_handle)
{
    return tm1668_bus_add_device(bus_handle, dev_config, ret_handle);
}

static inline esp_err_t tm1638_del_bus(tm1638_bus_handle_t bus_handle)
{
    return tm1668_del_bus(bus_handle);
}

static inline esp_err_t tm1638_bus_rm_device(tm1638_dev_handle_t handle)
{
    return tm1668_bus_rm_device(handle);
}

static inline esp_err_t tm1638_get_bus(tm1668_dev_handle_t handle,
                                       tm1668_bus_handle_t *ret_bus_handle)
{
    return tm1668_get_bus(handle, ret_bus_handle);
}
#endif // CONFIG_TM1668_WITH_BUS

static inline esp_err_t tm1638_new_device(const tm1668_config_t *config,
                                          tm1668_dev_handle_t *ret_handle)
{
    return tm1668_new_device(config, ret_handle);
}

static inline esp_err_t tm1638_del_device(tm1668_dev_handle_t handle)
{
    return tm1668_del_device(handle);
}

static inline esp_err_t tm1638_reset(tm1638_dev_handle_t handle)
{
    return tm1668_reset(handle);
}

/**
Address of display register
SEG1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | SEG9 | 10
BIT0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | BIT0 | 1
00HL             | 00HU          | 01HL      | GRID1
02HL             | 02HU          | 03HL      | GRID2
04HL             | 04HU          | 05HL      | GRID3
06HL             | 06HU          | 07HL      | GRID4
08HL             | 08HU          | 09HL      | GRID5
0AHL             | 0AHU          | 0BHL      | GRID6
0CHL             | 0CHU          | 0DHL      | GRID7
0EHL             | 0EHU          | 0FHL      | GRID8
*/
#define TM1638_DISPLAY_SIZE 16

static inline esp_err_t tm1638_display_auto(tm1638_dev_handle_t handle,
                                            uint8_t address, uint8_t *data,
                                            size_t size)
{
    return tm1668_display_auto(handle, address, data, size);
}

static inline esp_err_t tm1638_display_fixed(tm1638_dev_handle_t handle,
                                             uint8_t address, uint8_t data)
{
    return tm1668_display_fixed(handle, address, data);
}

/**
Keypad scaning
BIT0 | 1 | 2 | 3 | 4  | 5 | 6
K3   | 2 | 1 | X | K3 | 2 | 1
KS1              | KS2        | BYTE1
KS3              | KS4        | BYTE2
KS5              | KS6        | BYTE3
KS7              | KS8        | BYTE4
*/
#define TM1638_KEY_SIZE 4

static inline esp_err_t tm1638_read_key(tm1638_dev_handle_t handle,
                                        uint8_t *data, size_t size)
{
    return tm1668_read_key(handle, data, size);
}

// NO Display Mode Setting

#define TM1638_PULSE_WIDTH_1 TM1668_PULSE_WIDTH_1
#define TM1638_PULSE_WIDTH_2 TM1668_PULSE_WIDTH_2
#define TM1638_PULSE_WIDTH_4 TM1668_PULSE_WIDTH_4
#define TM1638_PULSE_WIDTH_10 TM1668_PULSE_WIDTH_10
#define TM1638_PULSE_WIDTH_11 TM1668_PULSE_WIDTH_11
#define TM1638_PULSE_WIDTH_12 TM1668_PULSE_WIDTH_12
#define TM1638_PULSE_WIDTH_13 TM1668_PULSE_WIDTH_13
#define TM1638_PULSE_WIDTH_14 TM1668_PULSE_WIDTH_14

#define TM1638_PULSE_WIDTH_DEFAULT TM1668_PULSE_WIDTH_DEFAULT

static inline esp_err_t tm1638_set_pulse(tm1638_dev_handle_t handle,
                                         uint8_t value)
{
    return tm1668_set_pulse(handle, value);
}

static inline esp_err_t tm1638_display(tm1638_dev_handle_t handle, bool value)
{
    return tm1668_display(handle, value);
}
