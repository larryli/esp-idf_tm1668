#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "tm1638.h"

#if CONFIG_IDF_TARGET_ESP32
#define CLK_IO_PIN GPIO_NUM_26
#define DIO_IO_PIN GPIO_NUM_25
#define STB_IO_PIN GPIO_NUM_27
#else
#define CLK_IO_PIN GPIO_NUM_11
#define DIO_IO_PIN GPIO_NUM_12
#define STB_IO_PIN GPIO_NUM_13
#endif

static const char TAG[] = "app_main";

static const uint8_t num7seg[] = {
    0b00111111, /* 0 */
    0b00000110, /* 1 */
    0b01011011, /* 2 */
    0b01001111, /* 3 */
    0b01100110, /* 4 */
    0b01101101, /* 5 */
    0b01111101, /* 6 */
    0b00000111, /* 7 */
    0b01111111, /* 8 */
    0b01101111, /* 9 */
};

void app_main(void)
{
    ESP_LOGI(TAG, "start");
    const tm1638_bus_config_t bus_config = {
        .clk_io_num = CLK_IO_PIN,
        .dio_io_num = DIO_IO_PIN,
        .flags.enable_internal_pullup = true,
    };
    tm1638_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(tm1638_new_bus(&bus_config, &bus_handle));
    const tm1638_device_config_t dev_config = {
        .stb_io_num = STB_IO_PIN,
        .flags.enable_internal_pullup = true,
    };
    tm1638_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(
        tm1638_bus_add_device(bus_handle, &dev_config, &dev_handle));
    ESP_ERROR_CHECK(tm1638_reset(dev_handle));
    uint8_t buf[TM1638_DISPLAY_SIZE] = {
        num7seg[1], 0, num7seg[2], 0, num7seg[3], 0, num7seg[4], 0,
        num7seg[5], 0, num7seg[6], 0, num7seg[7], 0, num7seg[8], 0};
    ESP_ERROR_CHECK(tm1638_display_auto(dev_handle, 0, buf, sizeof(buf)));
    ESP_ERROR_CHECK(tm1638_set_pulse(dev_handle, TM1638_PULSE_WIDTH_DEFAULT));
    ESP_ERROR_CHECK(tm1638_display(dev_handle, true));

    uint8_t key[TM1638_KEY_SIZE];
    while (1) {
        ESP_ERROR_CHECK(tm1638_read_key(dev_handle, key, sizeof(key)));
        for (int i = 0; i < TM1638_KEY_SIZE * 2; i++) {
            ESP_ERROR_CHECK(tm1638_display_fixed(
                dev_handle, i * 2 + 1,
                i < TM1638_KEY_SIZE ? key[i % TM1638_KEY_SIZE]
                                    : key[i % TM1638_KEY_SIZE] >> 4));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
