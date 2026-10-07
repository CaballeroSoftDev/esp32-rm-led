#include "ws2812.h"

#include "esp_err.h"
#include "esp_log.h"
#include "led_strip.h"

#define LED_GPIO 48 /* Onboard WS2812 data pin. */

static const char *TAG = "led_ws2812";
static led_strip_handle_t s_led_strip;

esp_err_t led_ws2812_init(void)
{
    if (s_led_strip != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };

    /* WS2812 timing uses a 10 MHz RMT clock; DMA is disabled for one pixel. */
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = false,
        },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (err != ESP_OK) {
        s_led_strip = NULL;
        return err;
    }

    /* Make the hardware's safe initial state explicit. */
    err = led_strip_clear(s_led_strip);
    if (err != ESP_OK) {
        (void)led_strip_del(s_led_strip);
        s_led_strip = NULL;
        return err;
    }

    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return ESP_OK;
}

esp_err_t led_ws2812_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (s_led_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = led_strip_set_pixel(s_led_strip, 0, red, green, blue);
    if (err != ESP_OK) {
        return err;
    }

    return led_strip_refresh(s_led_strip);
}

esp_err_t led_ws2812_deinit(void)
{
    if (s_led_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = led_strip_del(s_led_strip);
    if (err == ESP_OK) {
        s_led_strip = NULL;
    }
    return err;
}
