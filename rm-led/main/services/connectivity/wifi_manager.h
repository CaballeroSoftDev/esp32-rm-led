#pragma once

#include <stdbool.h>

#include "esp_err.h"

/**
 * Initialize NVS, networking, and Wi-Fi station mode.
 * Starts connection attempts asynchronously; does not wait for an IP address.
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
esp_err_t wifi_manager_start(void);

/**
 * Block until an IP address is acquired or the retry limit is reached.
 * @return true if connected; false if connection failed or start was not called.
 */
bool wifi_manager_wait_for_connection(void);
