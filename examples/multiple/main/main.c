#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "tm1638.h"

#define CLK_IO_PIN GPIO_NUM_18
#define DIO_IO_PIN GPIO_NUM_19
#define TM1668_STB_IO_PIN GPIO_NUM_5
#define TM1638_STB_IO_PIN GPIO_NUM_23

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
    ESP_ERROR_CHECK(tm1668_new_bus(&bus_config, &bus_handle));

    const tm1668_device_config_t tm1668_config = {
        .stb_io_num = TM1668_STB_IO_PIN,
        .flags.enable_internal_pullup = true,
    };
    tm1668_dev_handle_t tm1668_handle;
    ESP_ERROR_CHECK(
        tm1638_bus_add_device(bus_handle, &tm1668_config, &tm1668_handle));

    const tm1638_device_config_t tm1638_config = {
        .stb_io_num = TM1638_STB_IO_PIN,
        .flags.enable_internal_pullup = true,
    };
    tm1638_dev_handle_t tm1638_handle;
    ESP_ERROR_CHECK(
        tm1638_bus_add_device(bus_handle, &tm1638_config, &tm1638_handle));

    ESP_ERROR_CHECK(tm1668_reset(tm1668_handle));
    ESP_ERROR_CHECK(tm1668_set_mode(tm1668_handle, TM1668_MODE_7x10));
    ESP_ERROR_CHECK(tm1638_reset(tm1638_handle));

    uint8_t buf1[TM1668_DISPLAY_SIZE] = {
        num7seg[1], 0, num7seg[2], 0, num7seg[3], 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ESP_ERROR_CHECK(tm1638_display_auto(tm1668_handle, 0, buf1, sizeof(buf1)));
    ESP_ERROR_CHECK(
        tm1638_set_pulse(tm1668_handle, TM1668_PULSE_WIDTH_DEFAULT));

    uint8_t buf2[TM1638_DISPLAY_SIZE] = {
        num7seg[1], 0, num7seg[2], 0, num7seg[3], 0, num7seg[4], 0,
        num7seg[5], 0, num7seg[6], 0, num7seg[7], 0, num7seg[8], 0};
    ESP_ERROR_CHECK(tm1638_display_auto(tm1638_handle, 0, buf2, sizeof(buf2)));
    ESP_ERROR_CHECK(
        tm1638_set_pulse(tm1638_handle, TM1638_PULSE_WIDTH_DEFAULT));

    ESP_ERROR_CHECK(tm1668_display(tm1668_handle, true));
    ESP_ERROR_CHECK(tm1638_display(tm1638_handle, true));

    uint8_t key1[TM1668_KEY_SIZE];
    uint8_t key2[TM1638_KEY_SIZE];
    while (1) {
        ESP_ERROR_CHECK(tm1668_read_key(tm1668_handle, key1, sizeof(key1)));
        uint8_t buf = 0;
        for (int i = 0; i < TM1668_KEY_SIZE; i++) {
            buf |= (key1[i] & 1) << i;
        }
        ESP_ERROR_CHECK(tm1668_display_fixed(tm1668_handle, 6, buf));
        buf = 0;
        for (int i = 0; i < TM1668_KEY_SIZE; i++) {
            buf |= ((key1[i] >> 3) & 1) << i;
        }
        ESP_ERROR_CHECK(tm1668_display_fixed(tm1668_handle, 8, buf));
        ESP_ERROR_CHECK(tm1638_read_key(tm1638_handle, key2, sizeof(key2)));
        for (int i = 0; i < TM1638_KEY_SIZE * 2; i++) {
            ESP_ERROR_CHECK(tm1638_display_fixed(
                tm1638_handle, i * 2 + 1,
                i < TM1638_KEY_SIZE ? key2[i % TM1638_KEY_SIZE]
                                    : key2[i % TM1638_KEY_SIZE] >> 4));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
