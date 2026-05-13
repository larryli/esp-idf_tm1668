/**
 * @file tm1638.h
 * @brief Compatibility wrappers for TM1638 LED display/keypad driver IC.
 *
 * The TM1638 is register-compatible with a subset of the TM1668 feature set
 * (8 grids × 10 segments, 3×8 key matrix, no display-mode setting). This
 * header provides convenience typedefs and inline wrapper functions so that
 * the same tm1668 driver core can be used with TM1638 hardware without code
 * changes — simply use the tm1638_* symbols instead of tm1668_*.
 *
 * All functions in this header are inline wrappers that directly call their
 * tm1668_* counterparts. There is no separate TM1638 driver source file.
 *
 * @note The TM1638 has a fixed display mode of 8 grids × 10 segments.
 *       tm1638_set_mode() does NOT exist. Use TM1638_DISPLAY_SIZE (16)
 *       instead of TM1668_DISPLAY_SIZE (14) for display buffer sizing.
 */

#pragma once

#include "tm1668.h"

#ifdef CONFIG_TM1668_WITH_BUS
/** Alias for tm1668_bus_config_t — shared bus configuration. */
typedef tm1668_bus_config_t tm1638_bus_config_t;

/** Alias for tm1668_bus_handle_t — opaque bus handle. */
typedef tm1668_bus_handle_t tm1638_bus_handle_t;

/** Alias for tm1668_device_config_t — per-device configuration on a bus. */
typedef tm1668_device_config_t tm1638_device_config_t;
#endif // CONFIG_TM1668_WITH_BUS

/** Alias for tm1668_dev_handle_t — opaque device handle. */
typedef tm1668_dev_handle_t tm1638_dev_handle_t;

/** Alias for tm1668_config_t — standalone device configuration. */
typedef tm1668_config_t tm1638_config_t;

#ifdef CONFIG_TM1668_WITH_BUS
/**
 * @brief Create a new shared bus for TM1638 devices.
 *
 * Equivalent to tm1668_new_bus(). The bus protocol is identical for both ICs.
 */
static inline esp_err_t tm1638_new_bus(const tm1638_bus_config_t *bus_config,
                                       tm1638_bus_handle_t *ret_bus_handle)
{
    return tm1668_new_bus(bus_config, ret_bus_handle);
}

/**
 * @brief Add a TM1638 device to a shared bus.
 *
 * Equivalent to tm1668_bus_add_device().
 */
static inline esp_err_t
tm1638_bus_add_device(tm1638_bus_handle_t bus_handle,
                      const tm1638_device_config_t *dev_config,
                      tm1638_dev_handle_t *ret_handle)
{
    return tm1668_bus_add_device(bus_handle, dev_config, ret_handle);
}

/**
 * @brief Delete a TM1638 shared bus.
 *
 * Equivalent to tm1668_del_bus().
 */
static inline esp_err_t tm1638_del_bus(tm1638_bus_handle_t bus_handle)
{
    return tm1668_del_bus(bus_handle);
}

/**
 * @brief Remove a TM1638 device from its bus.
 *
 * Equivalent to tm1668_bus_rm_device().
 */
static inline esp_err_t tm1638_bus_rm_device(tm1638_dev_handle_t handle)
{
    return tm1668_bus_rm_device(handle);
}

/**
 * @brief Get the bus handle for a TM1638 device.
 *
 * Equivalent to tm1668_get_bus().
 */
static inline esp_err_t tm1638_get_bus(tm1668_dev_handle_t handle,
                                       tm1668_bus_handle_t *ret_bus_handle)
{
    return tm1668_get_bus(handle, ret_bus_handle);
}
#endif // CONFIG_TM1668_WITH_BUS

/**
 * @brief Create a new TM1638 device (standalone mode).
 *
 * Equivalent to tm1668_new_device().
 */
static inline esp_err_t tm1638_new_device(const tm1668_config_t *config,
                                          tm1668_dev_handle_t *ret_handle)
{
    return tm1668_new_device(config, ret_handle);
}

/**
 * @brief Delete a TM1638 device.
 *
 * Equivalent to tm1668_del_device().
 */
static inline esp_err_t tm1638_del_device(tm1668_dev_handle_t handle)
{
    return tm1668_del_device(handle);
}

/**
 * @brief Reset a TM1638 device to auto-increment address mode.
 *
 * Equivalent to tm1668_reset().
 */
static inline esp_err_t tm1638_reset(tm1638_dev_handle_t handle)
{
    return tm1668_reset(handle);
}

/**
 * @brief TM1638 display register map (16 bytes).
 *
 * The TM1638 has a fixed 8 grids × 10 segments display layout.
 * Each row corresponds to one GRID.
 *
 * @verbatim
 * SEG1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | SEG9 | 10
 * BIT0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | BIT0 | 1
 * 00HL             | 00HU          | 01HL      | GRID1
 * 02HL             | 02HU          | 03HL      | GRID2
 * 04HL             | 04HU          | 05HL      | GRID3
 * 06HL             | 06HU          | 07HL      | GRID4
 * 08HL             | 08HU          | 09HL      | GRID5
 * 0AHL             | 0AHU          | 0BHL      | GRID6
 * 0CHL             | 0CHU          | 0DHL      | GRID7
 * 0EHL             | 0EHU          | 0FHL      | GRID8
 * @endverbatim
 */
#define TM1638_DISPLAY_SIZE 16

/**
 * @brief Write display data in auto-increment mode (TM1638).
 *
 * Equivalent to tm1668_display_auto().
 */
static inline esp_err_t tm1638_display_auto(tm1638_dev_handle_t handle,
                                            uint8_t address,
                                            const uint8_t *data, size_t size)
{
    return tm1668_display_auto(handle, address, data, size);
}

/**
 * @brief Write a single byte in fixed-address mode (TM1638).
 *
 * Equivalent to tm1668_display_fixed().
 */
static inline esp_err_t tm1638_display_fixed(tm1638_dev_handle_t handle,
                                             uint8_t address, uint8_t data)
{
    return tm1668_display_fixed(handle, address, data);
}

/**
 * @brief TM1638 keypad scan data layout (4 bytes).
 *
 * Each byte encodes two keyscan inputs, covering up to 3 × 8 keys.
 *
 * @verbatim
 * BIT0 | 1 | 2 | 3 | 4  | 5 | 6
 * K3   | 2 | 1 | X | K3 | 2 | 1
 * KS1              | KS2        | BYTE1
 * KS3              | KS4        | BYTE2
 * KS5              | KS6        | BYTE3
 * KS7              | KS8        | BYTE4
 * @endverbatim
 */
#define TM1638_KEY_SIZE 4

/**
 * @brief Read the keypad scan data (TM1638).
 *
 * Equivalent to tm1668_read_key().
 */
static inline esp_err_t tm1638_read_key(tm1638_dev_handle_t handle,
                                        uint8_t *data, size_t size)
{
    return tm1668_read_key(handle, data, size);
}

/**
 * @note The TM1638 does NOT support tm1638_set_mode(). The display mode
 *       is fixed at 8 grids × 10 segments. tm1668_set_mode() is not
 *       wrapped; calling it directly on a TM1638 has no effect.
 */

/** Alias — see TM1668_PULSE_WIDTH_1. */
#define TM1638_PULSE_WIDTH_1 TM1668_PULSE_WIDTH_1
/** Alias — see TM1668_PULSE_WIDTH_2. */
#define TM1638_PULSE_WIDTH_2 TM1668_PULSE_WIDTH_2
/** Alias — see TM1668_PULSE_WIDTH_4. */
#define TM1638_PULSE_WIDTH_4 TM1668_PULSE_WIDTH_4
/** Alias — see TM1668_PULSE_WIDTH_10. */
#define TM1638_PULSE_WIDTH_10 TM1668_PULSE_WIDTH_10
/** Alias — see TM1668_PULSE_WIDTH_11. */
#define TM1638_PULSE_WIDTH_11 TM1668_PULSE_WIDTH_11
/** Alias — see TM1668_PULSE_WIDTH_12. */
#define TM1638_PULSE_WIDTH_12 TM1668_PULSE_WIDTH_12
/** Alias — see TM1668_PULSE_WIDTH_13. */
#define TM1638_PULSE_WIDTH_13 TM1668_PULSE_WIDTH_13
/** Alias — see TM1668_PULSE_WIDTH_14. */
#define TM1638_PULSE_WIDTH_14 TM1668_PULSE_WIDTH_14

/** Default pulse width (4/16 duty) — alias for TM1668_PULSE_WIDTH_DEFAULT. */
#define TM1638_PULSE_WIDTH_DEFAULT TM1668_PULSE_WIDTH_DEFAULT

/**
 * @brief Set the TM1638 display brightness.
 *
 * Equivalent to tm1668_set_pulse().
 */
static inline esp_err_t tm1638_set_pulse(tm1638_dev_handle_t handle,
                                         uint8_t value)
{
    return tm1668_set_pulse(handle, value);
}

/**
 * @brief Turn the TM1638 display on or off.
 *
 * Equivalent to tm1668_display().
 */
static inline esp_err_t tm1638_display(tm1638_dev_handle_t handle, bool value)
{
    return tm1668_display(handle, value);
}
