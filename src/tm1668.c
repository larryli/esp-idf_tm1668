#include "tm1668.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <sys/queue.h>

static const char TAG[] = "tm1668";

typedef struct tm1668_bus_device_list {
    tm1668_dev_handle_t device;
    SLIST_ENTRY(tm1668_bus_device_list) next;
} tm1668_bus_device_list_t;

struct tm1668_bus_t {
    gpio_num_t clk_num;
    gpio_num_t dio_num;
    SemaphoreHandle_t bus_lock_mux;
    SLIST_HEAD(tm1668_bus_device_list_head, tm1668_bus_device_list) device_list;
};

struct tm1668_dev_t {
    gpio_num_t stb_num;
    tm1668_bus_handle_t bus_handle;
    bool address_fixed;
    bool display_on;
    uint8_t pulse_width;
};

static portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;

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

    const gpio_config_t clk_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = false,
        .pull_up_en = bus_config->flags.enable_internal_pullup
                          ? GPIO_PULLUP_ENABLE
                          : GPIO_PULLUP_DISABLE,
        .pin_bit_mask = 1ULL << bus_handle->clk_num,
    };
    ESP_GOTO_ON_ERROR(gpio_set_level(bus_handle->clk_num, 1), err, TAG,
                      "CLK pin set level failed");
    ESP_GOTO_ON_ERROR(gpio_config(&clk_conf), err, TAG, "config GPIO failed");
    const gpio_config_t dio_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_down_en = false,
        .pull_up_en = bus_config->flags.enable_internal_pullup
                          ? GPIO_PULLUP_ENABLE
                          : GPIO_PULLUP_DISABLE,
        .pin_bit_mask = 1ULL << bus_handle->dio_num,
    };
    ESP_GOTO_ON_ERROR(gpio_set_level(bus_handle->dio_num, 1), err, TAG,
                      "DIO pin set level failed");
    ESP_GOTO_ON_ERROR(gpio_config(&dio_conf), err, TAG, "config GPIO failed");

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

    device_item =
        (tm1668_bus_device_list_t *)calloc(1, sizeof(tm1668_bus_device_list_t));
    ESP_GOTO_ON_FALSE((device_item != NULL), ESP_ERR_NO_MEM, err, TAG,
                      "no memory for tm1668 device item`");
    device_item->device = dev_handle;
    xSemaphoreTake(bus_handle->bus_lock_mux, portMAX_DELAY);
    SLIST_INSERT_HEAD(&bus_handle->device_list, device_item, next);
    xSemaphoreGive(bus_handle->bus_lock_mux);

    const gpio_config_t stb_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = false,
        .pull_up_en = dev_config->flags.enable_internal_pullup
                          ? GPIO_PULLUP_ENABLE
                          : GPIO_PULLUP_DISABLE,
        .pin_bit_mask = 1ULL << dev_handle->stb_num,
    };
    ESP_GOTO_ON_ERROR(gpio_set_level(dev_handle->stb_num, 1), err, TAG,
                      "STB pin set level failed");
    ESP_GOTO_ON_ERROR(gpio_config(&stb_conf), err, TAG, "config GPIO failed");

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

#define DELAY_US 1
#define READ_KEY_DELAY_US 2
#define MODE 0x00
#define ADDRESS_INCREMENT 0x40
#define ADDRESS_FIXED 0x44
#define DISPLAY_ADDRESS 0xC0
#define ADDRESS_MASK 0xF
#define READ_KEY 0x42
#define MODE_MASK 0x3
#define DISPLAY_CONTROL 0x80
#define PULSE_WIDTH_MASK 0x7
#define DISPLAY_BIT 3

static inline void _set_clk(tm1668_bus_handle_t handle)
{
    gpio_set_level(handle->clk_num, 1);
    esp_rom_delay_us(DELAY_US);
    gpio_set_level(handle->clk_num, 0);
    esp_rom_delay_us(DELAY_US);
}

static inline void _send_data(tm1668_bus_handle_t handle, uint8_t value)
{
    for (int b = 0; b < 8; b++) {
        gpio_set_level(handle->dio_num, (value >> b) & 1);
        _set_clk(handle);
    }
}

static inline void _send_command(tm1668_dev_handle_t handle, uint8_t command)
{
    portENTER_CRITICAL(&g_lock);
    gpio_set_level(handle->stb_num, 0);
    _send_data(handle->bus_handle, command);
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
                              uint8_t *data, size_t size)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");
    ESP_RETURN_ON_FALSE(address < 0x10, ESP_ERR_INVALID_ARG, TAG,
                        "invalid address");
    ESP_RETURN_ON_FALSE(address + size <= 0x10, ESP_ERR_INVALID_ARG, TAG,
                        "invalid size");

    if (handle->address_fixed) {
        _send_command(handle, ADDRESS_INCREMENT);
        handle->address_fixed = false;
    }
    portENTER_CRITICAL(&g_lock);
    gpio_set_level(handle->stb_num, 0);
    _send_data(handle->bus_handle, DISPLAY_ADDRESS | (ADDRESS_MASK & address));
    for (int n = 0; n < size; n++)
        _send_data(handle->bus_handle, data[n]);
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

    if (!handle->address_fixed) {
        _send_command(handle, ADDRESS_FIXED);
        handle->address_fixed = true;
    }
    portENTER_CRITICAL(&g_lock);
    gpio_set_level(handle->stb_num, 0);
    _send_data(handle->bus_handle, DISPLAY_ADDRESS | (ADDRESS_MASK & address));
    _send_data(handle->bus_handle, data);
    gpio_set_level(handle->stb_num, 1);
    portEXIT_CRITICAL(&g_lock);

    return ESP_OK;
}

esp_err_t tm1668_read_key(tm1668_dev_handle_t handle, uint8_t *data,
                          size_t size)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");
    ESP_RETURN_ON_FALSE(size <= 0x10, ESP_ERR_INVALID_ARG, TAG, "invalid size");

    portENTER_CRITICAL(&g_lock);
    gpio_set_level(handle->stb_num, 0);
    _send_data(handle->bus_handle, READ_KEY);
    gpio_set_level(handle->bus_handle->dio_num, 1);
    esp_rom_delay_us(READ_KEY_DELAY_US);
    for (int n = 0; n < size; n++) {
        data[n] = 0;
        for (int b = 0; b < 8; b++) {
            data[n] |= gpio_get_level(handle->bus_handle->dio_num) << b;
            _set_clk(handle->bus_handle);
        }
    }
    gpio_set_level(handle->stb_num, 1);
    portEXIT_CRITICAL(&g_lock);

    return ESP_OK;
}

esp_err_t tm1668_set_mode(tm1668_dev_handle_t handle, uint8_t value)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");

    _send_command(handle, MODE | (MODE_MASK & value));

    return ESP_OK;
}

esp_err_t tm1668_set_pulse(tm1668_dev_handle_t handle, uint8_t value)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG,
                        "invalid device handle");

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

    handle->display_on = value;
    _send_command(handle, DISPLAY_CONTROL |
                              (handle->display_on << DISPLAY_BIT) |
                              (PULSE_WIDTH_MASK & handle->pulse_width));

    return ESP_OK;
}
