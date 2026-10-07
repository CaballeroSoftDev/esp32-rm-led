#include "gpio_led.h"

#include "driver/gpio.h"
#include "esp_log.h"

/* Connect GPIO10 through a resistor to the LED anode; connect its cathode to GND. */
#define GPIO_LED_PIN GPIO_NUM_10

static const char *TAG = "gpio_led";
/* Prevent control calls before GPIO configuration succeeds. */
static bool s_gpio_led_initialized;

esp_err_t gpio_led_init(void)
{
    if (s_gpio_led_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << GPIO_LED_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }

    err = gpio_set_level(GPIO_LED_PIN, 0);
    if (err != ESP_OK) {
        return err;
    }

    s_gpio_led_initialized = true;
    ESP_LOGI(TAG, "Two-pin LED initialized on GPIO%d, initially off", GPIO_LED_PIN);
    return ESP_OK;
}

esp_err_t gpio_led_set(bool on)
{
    if (!s_gpio_led_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return gpio_set_level(GPIO_LED_PIN, on ? 1 : 0);
}
