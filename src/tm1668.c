/**
 * @file tm1668.c
 * @brief TM1668/TM1638 display driver implementation.
 *
 * Implements the 3-wire serial protocol (CLK, DIO, STB) for Titan Micro
 * TM1668 and TM1638 LED display/keypad driver ICs. Supports two operating
 * configurations selected via Kconfig:
 *
 * - **Bus mode** (CONFIG_TM1668_WITH_BUS): Multiple devices share CLK + DIO,
 *   each with its own STB. Synchronized with a FreeRTOS binary semaphore.
 * - **Standalone mode**: Each device gets its own CLK, DIO, STB pins.
 *
 * @section protocol Serial Protocol
 *
 * The protocol is synchronous, LSB-first:
 * - Host sets STB low to begin a transaction.
 * - For each bit: CLK low → set DIO → CLK high (data latched on rising edge).
 * - After 8 bits, STB returns high to end the command/data byte.
 * - For reads (key scan), host releases DIO after sending the command byte
 *   and the TM1668 drives DIO on subsequent clock cycles.
 *
 * Timing is controlled by esp_rom_delay_us() with configurable delays
 * (CONFIG_TM1668_DELAY_US and CONFIG_TM1668_READ_KEY_DELAY_US in Kconfig).
 * The TM1668 datasheet specifies a minimum 1 us clock half-cycle; the default
 * 1 us value yields ~500 kHz. Increase CONFIG_TM1668_DELAY_US for long wires
 * or clone chips that require slower timing.
 * CONFIG_TM1668_READ_KEY_DELAY_US is the settling time after the READ_KEY
 * command before the TM1668 begins driving DIO (default 2 us).
 */

#include "tm1668.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#ifdef CONFIG_TM1668_WITH_BUS
#include <sys/queue.h>
#endif

static const char TAG[] = "tm1668";

#ifdef CONFIG_TM1668_WITH_BUS
/** Singly-linked list entry for a device on a shared bus. */
typedef struct tm1668_bus_device_list {
    tm1668_dev_handle_t device;
    SLIST_ENTRY(tm1668_bus_device_list) next;
} tm1668_bus_device_list_t;

/**
 * @brief Internal bus structure.
 */
struct tm1668_bus_t {
    gpio_num_t clk_num;             /**< Shared CLK GPIO pin */
    gpio_num_t dio_num;             /**< Shared DIO GPIO pin (open-drain) */
    SemaphoreHandle_t bus_lock_mux; /**< Mutex for device list access */
    SLIST_HEAD(tm1668_bus_device_list_head, tm1668_bus_device_list)
    device_list; /**< List of devices on this bus */
};

/** Dereference the bus handle from a device handle. */
#define BUS_HANDLE(p) ((p)->bus_handle)

#else
/** In non-bus mode, the bus handle IS the device handle. */
typedef tm1668_dev_handle_t tm1668_bus_handle_t;

#define BUS_HANDLE(p) (p)
#endif

/**
 * @brief Internal device structure.
 */
struct tm1668_dev_t {
#ifdef CONFIG_TM1668_WITH_BUS
    tm1668_bus_handle_t bus_handle; /**< Owning bus handle (bus mode) */
#else
    gpio_num_t clk_num; /**< CLK GPIO pin (standalone mode) */
    gpio_num_t dio_num; /**< DIO GPIO pin (standalone mode) */
#endif
    gpio_num_t stb_num;  /**< STB (strobe) GPIO pin */
    bool address_fixed;  /**< true if device is in fixed-address mode */
    bool display_on;     /**< Display on/off state (cached) */
    uint8_t pulse_width; /**< Current pulse width setting (cached) */
};

/**
 * @brief Global spinlock for protocol-level mutual exclusion.
 *
 * While the bus semaphore protects the device list, this spinlock
 * ensures that the STB → command/data → STB transaction sequence is
 * not interrupted by another task or ISR.
 */
static portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;

/**
 * @brief Initialize a single GPIO pin for TM1668 communication.
 *
 * Sets the pin to output high (idle), configures direction and pull-up.
 * Extracted to avoid repeating the set_level → gpio_config pattern
 * across bus-mode and standalone-mode init functions.
 *
 * @param pin          GPIO pin number.
 * @param mode         GPIO mode (OUTPUT for STB/CLK, INPUT_OUTPUT_OD for DIO).
 * @param enable_pullup Whether to enable the internal pull-up resistor.
 * @return ESP_OK on success, or the underlying GPIO error code.
 */
static esp_err_t _init_gpio(gpio_num_t pin, gpio_mode_t mode,
                            bool enable_pullup)
{
    const gpio_config_t conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = mode,
        .pull_down_en = false,
        .pull_up_en = enable_pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pin_bit_mask = 1ULL << pin,
    };
    esp_err_t ret = gpio_set_level(pin, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    return gpio_config(&conf);
}

#ifdef CONFIG_TM1668_WITH_BUS
esp_err_t tm1668_new_bus(const tm1668_bus_config_t *bus_config,
                         tm1668_bus_handle_t *ret_bus_handle)
{
    ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_INVALID_ARG, TAG,
                        "invalid bus config");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(bus_config->clk_io_num),
                        ESP_ERR_INVALID_ARG, TAG, "invalid CLK pin number");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(bus_config->dio_io_num),
                        ESP_ERR_INVALID_ARG, TAG, "invalid DIO pin number");

    esp_err_t ret = ESP_OK;
    tm1668_bus_handle_t bus_handle =
        (tm1668_bus_handle_t)calloc(1, sizeof(struct tm1668_bus_t));
    ESP_GOTO_ON_FALSE(bus_handle, ESP_ERR_NO_MEM, err, TAG,
                      "no memory for bus");
    bus_handle->clk_num = bus_config->clk_io_num;
    bus_handle->dio_num = bus_config->dio_io_num;
    bus_handle->bus_lock_mux = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(bus_handle->bus_lock_mux, ESP_ERR_NO_MEM, err, TAG,
                      "No memory for binary semaphore");
    xSemaphoreGive(bus_handle->bus_lock_mux);

    /* CLK: push-pull output (host always drives this line). */
    ESP_GOTO_ON_ERROR(_init_gpio(bus_handle->clk_num, GPIO_MODE_OUTPUT,
                                 bus_config->flags.enable_internal_pullup),
                      err, TAG, "init CLK GPIO failed");

    /* DIO: open-drain so the TM1668 can pull it low during key scan reads. */
    ESP_GOTO_ON_ERROR(_init_gpio(bus_handle->dio_num, GPIO_MODE_INPUT_OUTPUT_OD,
                                 bus_config->flags.enable_internal_pullup),
                      err, TAG, "init DIO GPIO failed");

    xSemaphoreTake(bus_handle->bus_lock_mux, portMAX_DELAY);
    SLIST_INIT(&bus_handle->device_list);
    xSemaphoreGive(bus_handle->bus_lock_mux);

    *ret_bus_handle = bus_handle;
    return ESP_OK;

err:
    free(bus_handle);
    return ret;
}

esp_err_t tm1668_del_bus(tm1668_bus_handle_t bus_handle)
{
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid bus handle");

    /* Clean up any devices still attached to this bus.
     * This is a safety net — callers should remove devices explicitly
     * with tm1668_bus_rm_device() before deleting the bus. */
    if (!SLIST_EMPTY(&bus_handle->device_list)) {
        int count = 0;
        tm1668_bus_device_list_t *item, *tmp;
        xSemaphoreTake(bus_handle->bus_lock_mux, portMAX_DELAY);
        SLIST_FOREACH_SAFE(item, &bus_handle->device_list, next, tmp)
        {
            free(item->device);
            free(item);
            count++;
        }
        xSemaphoreGive(bus_handle->bus_lock_mux);
        ESP_LOGW(TAG,
                 "bus deleted with %d device(s) still attached — "
                 "use tm1668_bus_rm_device() before tm1668_del_bus()",
                 count);
    }

    if (bus_handle->bus_lock_mux) {
        vSemaphoreDelete(bus_handle->bus_lock_mux);
    }
    free(bus_handle);
    return ESP_OK;
}

esp_err_t tm1668_bus_add_device(tm1668_bus_handle_t bus_handle,
                                const tm1668_device_config_t *dev_config,
                                tm1668_dev_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(bus_handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid bus handle");
    ESP_RETURN_ON_FALSE(dev_config, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device config");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(dev_config->stb_io_num),
                        ESP_ERR_INVALID_ARG, TAG, "invalid STB pin number");

    /* Check that the STB pin is not already in use by another device. */
    tm1668_bus_device_list_t *device_item;
    bool stb_not_exist = true;
    xSemaphoreTake(bus_handle->bus_lock_mux, portMAX_DELAY);
    SLIST_FOREACH(device_item, &bus_handle->device_list, next)
    {
        if (device_item->device->stb_num == dev_config->stb_io_num) {
            stb_not_exist = false;
            break;
        }
    }
    xSemaphoreGive(bus_handle->bus_lock_mux);
    ESP_RETURN_ON_FALSE(stb_not_exist, ESP_ERR_INVALID_ARG, TAG,
                        "STB pin is exist");

    esp_err_t ret = ESP_OK;
    tm1668_dev_handle_t dev_handle =
        (tm1668_dev_handle_t)calloc(1, sizeof(struct tm1668_dev_t));
    ESP_GOTO_ON_FALSE(dev_handle, ESP_ERR_NO_MEM, err, TAG,
                      "no memory for device");
    dev_handle->bus_handle = bus_handle;
    dev_handle->stb_num = dev_config->stb_io_num;
    dev_handle->address_fixed = false;
    dev_handle->display_on = false;
    dev_handle->pulse_width = TM1668_PULSE_WIDTH_DEFAULT;

    /* Insert into bus device list (mutex-protected). */
    device_item =
        (tm1668_bus_device_list_t *)calloc(1, sizeof(tm1668_bus_device_list_t));
    ESP_GOTO_ON_FALSE((device_item != NULL), ESP_ERR_NO_MEM, err, TAG,
                      "no memory for tm1668 device item");
    device_item->device = dev_handle;
    xSemaphoreTake(bus_handle->bus_lock_mux, portMAX_DELAY);
    SLIST_INSERT_HEAD(&bus_handle->device_list, device_item, next);
    xSemaphoreGive(bus_handle->bus_lock_mux);

    /* STB: push-pull output (chip select, host always drives). */
    ESP_GOTO_ON_ERROR(_init_gpio(dev_handle->stb_num, GPIO_MODE_OUTPUT,
                                 dev_config->flags.enable_internal_pullup),
                      err, TAG, "init STB GPIO failed");

    *ret_handle = dev_handle;
    return ESP_OK;

err:
    if (dev_handle) {
        tm1668_bus_rm_device(dev_handle);
    }
    return ret;
}

esp_err_t tm1668_bus_rm_device(tm1668_dev_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");

    tm1668_bus_handle_t tm1668_bus = handle->bus_handle;
    tm1668_bus_device_list_t *device_item;
    xSemaphoreTake(tm1668_bus->bus_lock_mux, portMAX_DELAY);
    SLIST_FOREACH(device_item, &tm1668_bus->device_list, next)
    {
        if (handle == device_item->device) {
            SLIST_REMOVE(&tm1668_bus->device_list, device_item,
                         tm1668_bus_device_list, next);
            free(device_item);
            break;
        }
    }
    xSemaphoreGive(tm1668_bus->bus_lock_mux);
    free(handle);
    return ESP_OK;
}

esp_err_t tm1668_get_bus(tm1668_dev_handle_t handle,
                         tm1668_bus_handle_t *ret_bus_handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");
    *ret_bus_handle = handle->bus_handle;
    return ESP_OK;
}
#else  // CONFIG_TM1668_WITH_BUS
esp_err_t tm1668_new_device(const tm1668_config_t *config,
                            tm1668_dev_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "invalid config");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(config->clk_io_num),
                        ESP_ERR_INVALID_ARG, TAG, "invalid CLK pin number");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(config->dio_io_num),
                        ESP_ERR_INVALID_ARG, TAG, "invalid DIO pin number");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(config->stb_io_num),
                        ESP_ERR_INVALID_ARG, TAG, "invalid STB pin number");

    esp_err_t ret = ESP_OK;
    tm1668_dev_handle_t handle =
        (tm1668_dev_handle_t)calloc(1, sizeof(struct tm1668_dev_t));
    ESP_GOTO_ON_FALSE(handle, ESP_ERR_NO_MEM, err, TAG, "no memory for bus");
    handle->clk_num = config->clk_io_num;
    handle->dio_num = config->dio_io_num;
    handle->stb_num = config->stb_io_num;

    /* CLK: push-pull output. */
    ESP_GOTO_ON_ERROR(_init_gpio(handle->clk_num, GPIO_MODE_OUTPUT,
                                 config->flags.enable_internal_pullup),
                      err, TAG, "init CLK GPIO failed");

    /* DIO: open-drain for bidirectional communication. */
    ESP_GOTO_ON_ERROR(_init_gpio(handle->dio_num, GPIO_MODE_INPUT_OUTPUT_OD,
                                 config->flags.enable_internal_pullup),
                      err, TAG, "init DIO GPIO failed");

    /* STB: push-pull output (chip select). */
    ESP_GOTO_ON_ERROR(_init_gpio(handle->stb_num, GPIO_MODE_OUTPUT,
                                 config->flags.enable_internal_pullup),
                      err, TAG, "init STB GPIO failed");

    *ret_handle = handle;
    return ESP_OK;

err:
    free(handle);
    return ret;
}

esp_err_t tm1668_del_device(tm1668_dev_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    free(handle);
    return ESP_OK;
}
#endif // CONFIG_TM1668_WITH_BUS

/* Timing: half-clock-cycle delay in microseconds.
 * Datasheet minimum is 1 us; increase for long traces or clone chips. */
#define DELAY_US CONFIG_TM1668_DELAY_US

/* Settling delay after READ_KEY command before TM1668 drives DIO. */
#define READ_KEY_DELAY_US CONFIG_TM1668_READ_KEY_DELAY_US

/*
 * Command byte encoding (TM1668 / TM1638 datasheet).
 *
 *   B7 B6 B5 B4 B3 B2 B1 B0
 *   ─────────────────────────
 *   0  0  —  —  —  —  —  —   Display mode setting command
 *   0  1  —  —  —  —  —  —   Data command (0: fixed, 1: auto-increment)
 *   1  0  —  —  —  —  —  —   Display control (on/off + pulse width)
 *   1  1  —  —  —  —  —  —   Address setting (upper 4 bits = 0xC0)
 */

/** Display mode command prefix: 0b00xxxxxx. */
#define MODE 0x00
/** Data write mode: auto-increment address after each byte. */
#define ADDRESS_INCREMENT 0x40
/** Data write mode: write to fixed address (no auto-increment). */
#define ADDRESS_FIXED 0x44
/** Display register address command prefix: 0b11xxxxxx. */
#define DISPLAY_ADDRESS 0xC0
/** Mask for the 4-bit address field. */
#define ADDRESS_MASK 0xF
/** Read key scan data command. */
#define READ_KEY 0x42
/** Mask for the 2-bit mode field. */
#define MODE_MASK 0x3
/** Display control command prefix: 0b1000xxxx. */
#define DISPLAY_CONTROL 0x80
/** Mask for the 3-bit pulse width field. */
#define PULSE_WIDTH_MASK 0x7
/** Bit 3 = display on/off in the display control command. */
#define DISPLAY_BIT 3

/**
 * @brief Bit-bang one byte to the TM1668 (LSB first, MSB last).
 *
 * For each of the 8 bits:
 *  1. Drive CLK low.
 *  2. Set DIO to the current bit value (0 or 1).
 *  3. Wait half a clock cycle.
 *  4. Drive CLK high — TM1668 latches DIO on this rising edge.
 *  5. Wait half a clock cycle.
 * After all 8 bits, DIO is released back to high (idle state).
 *
 * @param[in] handle Bus handle (or device handle in non-bus mode).
 * @param[in] value  Byte to send.
 */
static inline void _send_data(tm1668_bus_handle_t handle, uint8_t value)
{
    for (int b = 0; b < 8; b++) {
        gpio_set_level(handle->clk_num, 0);
        gpio_set_level(handle->dio_num, (value >> b) & 1);
        esp_rom_delay_us(DELAY_US);
        gpio_set_level(handle->clk_num, 1);
        esp_rom_delay_us(DELAY_US);
    }
    gpio_set_level(handle->dio_num, 1);
}

/**
 * @brief Send a command byte to a device (STB-low framing).
 *
 * Enters a critical section to prevent interleaved transactions.
 *
 * @param[in] handle  Device handle.
 * @param[in] command Command byte to send.
 */
static inline void _send_command(tm1668_dev_handle_t handle, uint8_t command)
{
    portENTER_CRITICAL(&g_lock);
    gpio_set_level(handle->stb_num, 0);
    _send_data(BUS_HANDLE(handle), command);
    gpio_set_level(handle->stb_num, 1);
    portEXIT_CRITICAL(&g_lock);
}

esp_err_t tm1668_reset(tm1668_dev_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");

    _send_command(handle, ADDRESS_INCREMENT);
    handle->address_fixed = false;

    return ESP_OK;
}

esp_err_t tm1668_display_auto(tm1668_dev_handle_t handle, uint8_t address,
                              const uint8_t *data, size_t size)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");
    ESP_RETURN_ON_FALSE(address < 0x10, ESP_ERR_INVALID_ARG, TAG,
                        "invalid address");
    ESP_RETURN_ON_FALSE(address + size <= 0x10, ESP_ERR_INVALID_ARG, TAG,
                        "invalid size");

    /* Switch to auto-increment mode if needed (cached). */
    if (handle->address_fixed) {
        _send_command(handle, ADDRESS_INCREMENT);
        handle->address_fixed = false;
    }

    /* One continuous transaction: STB low → address + data bytes → STB high. */
    portENTER_CRITICAL(&g_lock);
    gpio_set_level(handle->stb_num, 0);
    _send_data(BUS_HANDLE(handle), DISPLAY_ADDRESS | (ADDRESS_MASK & address));
    for (int n = 0; n < size; n++) {
        _send_data(BUS_HANDLE(handle), data[n]);
    }
    gpio_set_level(handle->stb_num, 1);
    portEXIT_CRITICAL(&g_lock);

    return ESP_OK;
}

esp_err_t tm1668_display_fixed(tm1668_dev_handle_t handle, uint8_t address,
                               uint8_t data)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");
    ESP_RETURN_ON_FALSE(address < 0x10, ESP_ERR_INVALID_ARG, TAG,
                        "invalid address");

    /* Switch to fixed-address mode if needed (cached). */
    if (!handle->address_fixed) {
        _send_command(handle, ADDRESS_FIXED);
        handle->address_fixed = true;
    }

    portENTER_CRITICAL(&g_lock);
    gpio_set_level(handle->stb_num, 0);
    _send_data(BUS_HANDLE(handle), DISPLAY_ADDRESS | (ADDRESS_MASK & address));
    _send_data(BUS_HANDLE(handle), data);
    gpio_set_level(handle->stb_num, 1);
    portEXIT_CRITICAL(&g_lock);

    return ESP_OK;
}

esp_err_t tm1668_read_key(tm1668_dev_handle_t handle, uint8_t *data,
                          size_t size)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "invalid data pointer");
    ESP_RETURN_ON_FALSE(size <= 0x10, ESP_ERR_INVALID_ARG, TAG, "invalid size");

    /* Key scan read sequence:
     * 1. STB low → send READ_KEY command (host drives DIO).
     * 2. Release DIO, wait READ_KEY_DELAY_US for TM1668 to start driving.
     * 3. Clock in `size` bytes LSB-first.
     * 4. STB high. */
    portENTER_CRITICAL(&g_lock);
    gpio_set_level(handle->stb_num, 0);
    _send_data(BUS_HANDLE(handle), READ_KEY);
    gpio_set_level(BUS_HANDLE(handle)->dio_num, 1);
    esp_rom_delay_us(READ_KEY_DELAY_US);
    for (int n = 0; n < size; n++) {
        data[n] = 0;
        for (int b = 0; b < 8; b++) {
            gpio_set_level(BUS_HANDLE(handle)->clk_num, 0);
            esp_rom_delay_us(DELAY_US);
            gpio_set_level(BUS_HANDLE(handle)->clk_num, 1);
            esp_rom_delay_us(DELAY_US);
            data[n] |= gpio_get_level(BUS_HANDLE(handle)->dio_num) << b;
        }
    }
    gpio_set_level(BUS_HANDLE(handle)->dio_num, 1);
    gpio_set_level(handle->stb_num, 1);
    portEXIT_CRITICAL(&g_lock);

    return ESP_OK;
}

esp_err_t tm1668_set_mode(tm1668_dev_handle_t handle, uint8_t value)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");

    /* Display mode command: 0b00MMxxxx.
     * Only valid on TM1668 (TM1638 ignores this command). */
    _send_command(handle, MODE | (MODE_MASK & value));

    return ESP_OK;
}

esp_err_t tm1668_set_pulse(tm1668_dev_handle_t handle, uint8_t value)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");

    /* Display control byte: 0b1000DPPP  (D=display on/off, PPP=pulse width). */
    handle->pulse_width = value;
    _send_command(handle, DISPLAY_CONTROL |
                              (handle->display_on << DISPLAY_BIT) |
                              (PULSE_WIDTH_MASK & handle->pulse_width));

    return ESP_OK;
}

esp_err_t tm1668_display(tm1668_dev_handle_t handle, bool value)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");

    /* Pulse width is preserved from the last tm1668_set_pulse() call. */
    handle->display_on = value;
    _send_command(handle, DISPLAY_CONTROL |
                              (handle->display_on << DISPLAY_BIT) |
                              (PULSE_WIDTH_MASK & handle->pulse_width));

    return ESP_OK;
}
