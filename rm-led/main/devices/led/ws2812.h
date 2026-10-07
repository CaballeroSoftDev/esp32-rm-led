#pragma once

#include <stdint.h>

#include "esp_err.h"

/**
 * Initialize the onboard WS2812 and clear it.
 * @return ESP_ERR_INVALID_STATE if already initialized; otherwise the driver result.
 */
esp_err_t led_ws2812_init(void);

/**
 * Set the LED color.
 * @param red Red component, 0-255.
 * @param green Green component, 0-255.
 * @param blue Blue component, 0-255.
 * @return ESP_ERR_INVALID_STATE if not initialized; otherwise the driver result.
 */
esp_err_t led_ws2812_set_color(uint8_t red, uint8_t green, uint8_t blue);

/**
 * Release the LED strip resources.
 * @return ESP_ERR_INVALID_STATE if not initialized; otherwise the driver result.
 */
esp_err_t led_ws2812_deinit(void);
