/**
 * @file tm1668.h
 * @brief Driver for TM1668 and compatible LED display/keypad driver ICs.
 *
 * This driver supports the Titan Micro TM1668, a 7-segment x 10-grid LED
 * display driver with keypad scanning. It communicates over a custom 3-wire
 * serial protocol (CLK, DIO, STB) and can optionally organize multiple
 * devices on a shared bus (CLK + DIO) when CONFIG_TM1668_WITH_BUS is enabled
 * in Kconfig.
 *
 * Key features:
 * - Auto-increment and fixed-address display modes
 * - Configurable display mode (grid × segment combinations)
 * - 8-level brightness (pulse width) control
 * - Keypad matrix scanning (up to 10 × 2 keys for TM1668)
 * - Optional shared bus for multiple daisy-chained devices
 *
 * @note The TM1638 is register-compatible with a subset of TM1668 features
 *       (fewer grids, no display-mode setting). Use tm1638.h for convenience
 *       wrappers.
 *
 * @section timing Serial Timing
 *
 * Communication timing is configured via Kconfig:
 * - CONFIG_TM1668_DELAY_US (default 1): Half-cycle clock delay. The datasheet
 *   minimum is 1 μs; increase for long wires or clone chips.
 * - CONFIG_TM1668_READ_KEY_DELAY_US (default 2): Settling delay after the
 *   READ_KEY command before the TM1668 drives DIO. Increase if key reads
 *   return inconsistent data.
 */

#pragma once

#include "esp_err.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_TM1668_WITH_BUS
/**
 * @brief Bus-level configuration for the shared CLK/DIO lines.
 *
 * When multiple TM1668/TM1638 devices share the same CLK and DIO pins
 * (each with its own STB pin), create a bus handle first and then add
 * individual devices to it.
 */
typedef struct {
    gpio_num_t clk_io_num; /**< GPIO number for the shared CLK line */
    gpio_num_t dio_io_num; /**< GPIO number for the shared DIO line */
    struct {
        uint32_t enable_internal_pullup
            : 1; /**< Enable internal pull-up on CLK and DIO */
    } flags;
} tm1668_bus_config_t;

/** Opaque handle for a shared TM1668 bus. */
typedef struct tm1668_bus_t *tm1668_bus_handle_t;

/**
 * @brief Per-device configuration for a device on a shared bus.
 *
 * Each device on the bus uses the shared CLK/DIO from the bus but
 * requires its own STB (strobe) pin for chip selection.
 */
typedef struct {
    gpio_num_t
        stb_io_num; /**< GPIO number for the STB (strobe) pin of this device */
    struct {
        uint32_t enable_internal_pullup
            : 1; /**< Enable internal pull-up on STB */
    } flags;
} tm1668_device_config_t;
#endif // CONFIG_TM1668_WITH_BUS

/** Opaque handle for a single TM1668 device. */
typedef struct tm1668_dev_t *tm1668_dev_handle_t;

/**
 * @brief Standalone device configuration (all three pins specified directly).
 *
 * Use this when you have a single TM1668 device, or when each device
 * has independent CLK/DIO/STB lines. If you need multiple devices on a
 * shared bus, use tm1668_bus_config_t + tm1668_device_config_t instead.
 */
typedef struct {
    gpio_num_t clk_io_num; /**< GPIO number for the CLK line */
    gpio_num_t dio_io_num; /**< GPIO number for the DIO line */
    gpio_num_t stb_io_num; /**< GPIO number for the STB (strobe) line */
    struct {
        uint32_t enable_internal_pullup
            : 1; /**< Enable internal pull-up on all three pins */
    } flags;
} tm1668_config_t;

#ifdef CONFIG_TM1668_WITH_BUS
/**
 * @brief Create a new shared bus with the given CLK/DIO pin configuration.
 *
 * Initializes the CLK (output) and DIO (open-drain) GPIOs and creates
 * a mutex for bus-level synchronization across devices.
 *
 * @param[in]  bus_config    Pointer to bus configuration structure.
 * @param[out] ret_bus_handle Pointer to receive the new bus handle.
 * @return
 *  - ESP_OK on success.
 *  - ESP_ERR_INVALID_ARG if bus_config is NULL or pin numbers are invalid.
 *  - ESP_ERR_NO_MEM if memory allocation fails.
 */
esp_err_t tm1668_new_bus(const tm1668_bus_config_t *bus_config,
                         tm1668_bus_handle_t *ret_bus_handle);

/**
 * @brief Add a device to an existing shared bus.
 *
 * Allocates and configures a new device handle on the given bus.
 * The STB pin must be unique among devices already on the bus.
 *
 * @param[in]  bus_handle  Bus handle created by tm1668_new_bus().
 * @param[in]  dev_config  Pointer to device configuration.
 * @param[out] ret_handle  Pointer to receive the new device handle.
 * @return
 *  - ESP_OK on success.
 *  - ESP_ERR_INVALID_ARG if arguments are invalid or STB pin already in use.
 *  - ESP_ERR_NO_MEM if memory allocation fails.
 */
esp_err_t tm1668_bus_add_device(tm1668_bus_handle_t bus_handle,
                                const tm1668_device_config_t *dev_config,
                                tm1668_dev_handle_t *ret_handle);

/**
 * @brief Delete a bus and free associated resources.
 *
 * The semaphore and bus memory are freed. 
 *
 * @param[in] bus_handle Bus handle to delete.
 * @return
 *  - ESP_OK on success.
 *  - ESP_ERR_INVALID_ARG if bus_handle is NULL.
 */
esp_err_t tm1668_del_bus(tm1668_bus_handle_t bus_handle);

/**
 * @brief Remove a device from its bus and free the device handle.
 *
 * Removes the device from the bus's device list and frees the device
 * memory. Does not affect other devices on the same bus.
 *
 * @param[in] handle Device handle to remove.
 * @return
 *  - ESP_OK on success.
 *  - ESP_ERR_INVALID_ARG if handle is NULL.
 */
esp_err_t tm1668_bus_rm_device(tm1668_dev_handle_t handle);

/**
 * @brief Get the bus handle associated with a device.
 *
 * @param[in]  handle         Device handle.
 * @param[out] ret_bus_handle Pointer to receive the bus handle.
 * @return
 *  - ESP_OK on success.
 *  - ESP_ERR_INVALID_ARG if handle is NULL.
 */
esp_err_t tm1668_get_bus(tm1668_dev_handle_t handle,
                         tm1668_bus_handle_t *ret_bus_handle);

/**
 * @brief Convenience macro to return early on ESP error.
 * @internal Used by the inline tm1668_new_device() and tm1668_del_device()
 *           wrappers when bus mode is enabled.
 */
#define _TM1668_CHECK_ESP_OK_(s)                                               \
    do {                                                                       \
        esp_err_t _r = (s);                                                    \
        if (_r != ESP_OK) {                                                    \
            return _r;                                                         \
        }                                                                      \
    } while (0)

/**
 * @brief Create a new TM1668 device using a standalone configuration.
 *
 * This is an inline convenience function that wraps tm1668_new_bus() and
 * tm1668_bus_add_device() into a single call. Use this for single-device
 * setups where bus management is not needed.
 *
 * @param[in]  config     Pointer to device configuration (all three pins).
 * @param[out] ret_handle Pointer to receive the new device handle.
 * @return ESP_OK on success, or an error code from bus/device creation.
 */
static inline esp_err_t tm1668_new_device(const tm1668_config_t *config,
                                          tm1668_dev_handle_t *ret_handle)
{
    esp_err_t ret;
    const tm1668_bus_config_t bus_config = {
        .clk_io_num = config->clk_io_num,
        .dio_io_num = config->dio_io_num,
        .flags.enable_internal_pullup = config->flags.enable_internal_pullup,
    };
    tm1668_bus_handle_t bus_handle;
    _TM1668_CHECK_ESP_OK_(tm1668_new_bus(&bus_config, &bus_handle));
    const tm1668_device_config_t dev_config = {
        .stb_io_num = config->stb_io_num,
        .flags.enable_internal_pullup = config->flags.enable_internal_pullup,
    };
    ret = tm1668_bus_add_device(bus_handle, &dev_config, ret_handle);
    if (ret != ESP_OK) {
        tm1668_del_bus(bus_handle);
        return ret;
    }
    return ESP_OK;
}

/**
 * @brief Delete a device created with tm1668_new_device().
 *
 * Removes the device from its auto-created bus and deletes the bus.
 *
 * @param[in] handle Device handle to delete.
 * @return ESP_OK on success, or an error code.
 */
static inline esp_err_t tm1668_del_device(tm1668_dev_handle_t handle)
{
    tm1668_bus_handle_t bus_handle;
    _TM1668_CHECK_ESP_OK_(tm1668_get_bus(handle, &bus_handle));
    _TM1668_CHECK_ESP_OK_(tm1668_bus_rm_device(handle));
    _TM1668_CHECK_ESP_OK_(tm1668_del_bus(bus_handle));
    return ESP_OK;
}
#undef _TM1668_CHECK_ESP_OK_
#else
/**
 * @brief Create a new TM1668 device (non-bus mode).
 *
 * Directly initializes CLK, DIO, and STB pins and allocates a device handle.
 * Each device has independent pins; no bus sharing.
 *
 * @param[in]  config     Pointer to device configuration.
 * @param[out] ret_handle Pointer to receive the new device handle.
 * @return ESP_OK on success, or error code.
 */
esp_err_t tm1668_new_device(const tm1668_config_t *config,
                            tm1668_dev_handle_t *ret_handle);

/**
 * @brief Delete a TM1668 device (non-bus mode).
 *
 * Frees the device handle. GPIO pins are not reset to their default state.
 *
 * @param[in] handle Device handle to delete.
 * @return ESP_OK on success, or error code.
 */
esp_err_t tm1668_del_device(tm1668_dev_handle_t handle);
#endif // CONFIG_TM1668_WITH_BUS

/**
 * @brief Reset the device to auto-increment address mode.
 *
 * Sends the ADDRESS_INCREMENT command, which resets the address pointer
 * and switches to auto-increment mode if it was in fixed-address mode.
 *
 * @param[in] handle Device handle.
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG.
 */
esp_err_t tm1668_reset(tm1668_dev_handle_t handle);

/**
 * @brief TM1668 display register map (14 bytes).
 *
 * Each row corresponds to one GRID. The address column shows the register
 * address in hex (L = low nibble, H = high nibble of the byte).
 *
 * @verbatim
 * SEG1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | SEG9 | 10 | X | SEG12 | 13 | 14
 * BIT0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | BIT0 | 1  | 2 | 3     | 4  | 5
 * 00HL             | 00HU          | 01HL                  | 01HU    | GRID1
 * 02HL             | 02HU          | 03HL                  | 03HU    | GRID2
 * 04HL             | 04HU          | 05HL                  | 05HU    | GRID3
 * 06HL             | 06HU          | 07HL                  | 07HU    | GRID4
 * 08HL             | 08HU          | 09HL                  | 09HU    | GRID5
 * 0AHL             | 0AHU          | 0BHL                  | 0BHU    | GRID6
 * 0CHL             | 0CHU          | 0DHL                  | 0DHU    | GRID7
 * @endverbatim
 */
#define TM1668_DISPLAY_SIZE 14

/**
 * @brief Write display data starting at the given address in auto-increment
 * mode.
 *
 * The address pointer advances automatically after each byte written.
 * If the device is currently in fixed-address mode, this function
 * switches it to auto-increment mode first.
 *
 * @param[in] handle  Device handle.
 * @param[in] address Starting display register address (0–15).
 * @param[in] data    Pointer to the data buffer to write.
 * @param[in] size    Number of bytes to write (address + size must not exceed
 *                    16).
 * @return ESP_OK on success, or error code.
 */
esp_err_t tm1668_display_auto(tm1668_dev_handle_t handle, uint8_t address,
                              const uint8_t *data, size_t size);

/**
 * @brief Write a single byte to a display register in fixed-address mode.
 *
 * The address pointer does not advance. Useful for updating individual
 * grid segments. If the device is currently in auto-increment mode,
 * this function switches it to fixed-address mode first.
 *
 * @param[in] handle  Device handle.
 * @param[in] address Display register address (0–15).
 * @param[in] data    Single byte to write.
 * @return ESP_OK on success, or error code.
 */
esp_err_t tm1668_display_fixed(tm1668_dev_handle_t handle, uint8_t address,
                               uint8_t data);

/**
 * @brief TM1668 keypad scan data layout (5 bytes).
 *
 * Each byte encodes two rows of the key matrix:
 *
 * @verbatim
 * BIT0 | 1 | 2 | 3  | 4
 * K1   | 2 | X | K1 | 2
 * KS1          | KS2    | BYTE1
 * KS3          | KS4    | BYTE2
 * KS5          | KS6    | BYTE3
 * KS7          | KS8    | BYTE4
 * KS9          | KS10   | BYTE5
 * @endverbatim
 */
#define TM1668_KEY_SIZE 5

/**
 * @brief Read the keypad scan data from the TM1668.
 *
 * Reads `size` bytes of key matrix state. The TM1668 returns 5 bytes
 * covering up to 10 keyscan inputs × 2 key return lines.
 *
 * @param[in]  handle Device handle.
 * @param[out] data   Buffer to receive key data (must be at least `size`
 * bytes).
 * @param[in]  size   Number of bytes to read (typically TM1668_KEY_SIZE).
 * @return ESP_OK on success, or error code.
 */
esp_err_t tm1668_read_key(tm1668_dev_handle_t handle, uint8_t *data,
                          size_t size);

/** Display mode: number of grids × number of segments per grid. */
enum {
    TM1668_MODE_4x13 = 0, /**< 4 grids, 13 segments each */
    TM1668_MODE_5x12,     /**< 5 grids, 12 segments each */
    TM1668_MODE_6x11,     /**< 6 grids, 11 segments each */
    TM1668_MODE_7x10,     /**< 7 grids, 10 segments each */
};

/** Default display mode (4 grids × 13 segments). */
#define TM1668_MODE_DEFAULT TM1668_MODE_4x13

/**
 * @brief Set the display mode (grid × segment configuration).
 *
 * TM1668 supports four modes with different grid/segment trade-offs
 * (see TM1668_MODE_* enum). TM1638 does NOT support this command;
 * it is fixed at 8 grids × 10 segments.
 *
 * @param[in] handle Device handle.
 * @param[in] value  Display mode (TM1668_MODE_4x13 through TM1668_MODE_7x10).
 * @return ESP_OK on success, or error code.
 */
esp_err_t tm1668_set_mode(tm1668_dev_handle_t handle, uint8_t value);

/** Pulse width (brightness) settings. Higher values = brighter display. */
enum {
    TM1668_PULSE_WIDTH_1 = 0, /**< 1/16 duty — dimmest */
    TM1668_PULSE_WIDTH_2,     /**< 2/16 duty */
    TM1668_PULSE_WIDTH_4,     /**< 4/16 duty */
    TM1668_PULSE_WIDTH_10,    /**< 10/16 duty */
    TM1668_PULSE_WIDTH_11,    /**< 11/16 duty */
    TM1668_PULSE_WIDTH_12,    /**< 12/16 duty */
    TM1668_PULSE_WIDTH_13,    /**< 13/16 duty */
    TM1668_PULSE_WIDTH_14,    /**< 14/16 duty — brightest */
};

/** Default pulse width (4/16 duty). */
#define TM1668_PULSE_WIDTH_DEFAULT TM1668_PULSE_WIDTH_4

/**
 * @brief Set the display brightness via pulse width modulation.
 *
 * The pulse width controls the LED duty cycle. The display must be
 * turned on separately with tm1668_display() for the setting to take
 * visible effect.
 *
 * @param[in] handle Device handle.
 * @param[in] value  Pulse width value (TM1668_PULSE_WIDTH_1 through
 *                   TM1668_PULSE_WIDTH_14).
 * @return ESP_OK on success, or error code.
 */
esp_err_t tm1668_set_pulse(tm1668_dev_handle_t handle, uint8_t value);

/**
 * @brief Turn the LED display on or off.
 *
 * This sets the display enable bit in the display control command.
 * Brightness (pulse width) is preserved across on/off toggles.
 *
 * @param[in] handle Device handle.
 * @param[in] value  true to turn display on, false to turn off.
 * @return ESP_OK on success, or error code.
 */
esp_err_t tm1668_display(tm1668_dev_handle_t handle, bool value);

#ifdef __cplusplus
}
#endif
