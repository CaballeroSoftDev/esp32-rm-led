#pragma once

#include <stdbool.h>

#include "esp_err.h"

/**
 * Configure the external LED output and leave it off.
 * @return ESP_ERR_INVALID_STATE if already initialized; otherwise the GPIO result.
 */
esp_err_t gpio_led_init(void);

/**
 * Set the LED state.
 * @param on true to turn on; false to turn off.
 * @return ESP_ERR_INVALID_STATE if not initialized; otherwise the GPIO result.
 */
esp_err_t gpio_led_set(bool on);
