#pragma once

#include "esp_err.h"
#include "soc/gpio_num.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t clk_io_num;
    gpio_num_t dio_io_num;
    struct {
        uint32_t enable_internal_pullup : 1;
    } flags;
} tm1668_bus_config_t;

typedef struct tm1668_bus_t *tm1668_bus_handle_t;

typedef struct {
    gpio_num_t stb_io_num;
    struct {
        uint32_t enable_internal_pullup : 1;
    } flags;
} tm1668_device_config_t;

typedef struct tm1668_dev_t *tm1668_dev_handle_t;

esp_err_t tm1668_new_bus(const tm1668_bus_config_t *bus_config,
                         tm1668_bus_handle_t *ret_bus_handle);
esp_err_t tm1668_bus_add_device(tm1668_bus_handle_t bus_handle,
                                const tm1668_device_config_t *dev_config,
                                tm1668_dev_handle_t *ret_handle);
esp_err_t tm1668_del_bus(tm1668_bus_handle_t bus_handle);
esp_err_t tm1668_bus_rm_device(tm1668_dev_handle_t handle);
esp_err_t tm1668_reset(tm1668_dev_handle_t handle);

/**
Address of display register
SEG1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | SEG9 | 10 | X | SEG12 | 13 | 14
BIT0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | BIT0 | 1  | 2 | 3     | 4  | 5
00HL             | 00HU          | 01HL                  | 01HU    | GRID1
02HL             | 02HU          | 03HL                  | 03HU    | GRID2
04HL             | 04HU          | 05HL                  | 05HU    | GRID3
06HL             | 06HU          | 07HL                  | 07HU    | GRID4
08HL             | 08HU          | 09HL                  | 09HU    | GRID5
0AHL             | 0AHU          | 0BHL                  | 0BHU    | GRID6
0CHL             | 0CHU          | 0DHL                  | 0DHU    | GRID7
*/
#define TM1668_DISPLAY_SIZE 14

esp_err_t tm1668_display_auto(tm1668_dev_handle_t handle, uint8_t address,
                              uint8_t *data, size_t size);
esp_err_t tm1668_display_fixed(tm1668_dev_handle_t handle, uint8_t address,
                               uint8_t data);

/**
Keypad scaning
BIT0 | 1 | 2 | 3  | 4
K1   | 2 | X | K1 | 2
KS1          | KS2    | BYTE1
KS3          | KS4    | BYTE2
KS5          | KS6    | BYTE3
KS7          | KS8    | BYTE4
KS9          | KS10   | BYTE5
*/
#define TM1668_KEY_SIZE 5
esp_err_t tm1668_read_key(tm1668_dev_handle_t handle, uint8_t *data,
                          size_t size);

enum {
    TM1668_MODE_4x13 = 0,
    TM1668_MODE_5x12,
    TM1668_MODE_6x11,
    TM1668_MODE_7x10,
};

#define TM1668_MODE_DEFAULT TM1668_MODE_4x13

esp_err_t tm1668_set_mode(tm1668_dev_handle_t handle, uint8_t value);

enum {
    TM1668_PULSE_WIDTH_1 = 0,
    TM1668_PULSE_WIDTH_2,
    TM1668_PULSE_WIDTH_4,
    TM1668_PULSE_WIDTH_10,
    TM1668_PULSE_WIDTH_11,
    TM1668_PULSE_WIDTH_12,
    TM1668_PULSE_WIDTH_13,
    TM1668_PULSE_WIDTH_14,
};

#define TM1668_PULSE_WIDTH_DEFAULT TM1668_PULSE_WIDTH_4

esp_err_t tm1668_set_pulse(tm1668_dev_handle_t handle, uint8_t value);
esp_err_t tm1668_display(tm1668_dev_handle_t handle, bool value);
#ifdef __cplusplus
}
#endif
