#pragma once

#include "esp_err.h"

/**
 * Initialize and start the MQTT client after Wi-Fi has obtained an IP address.
 * Broker connection and event handling continue asynchronously.
 * @return ESP_OK if the client started, or an ESP-IDF error code.
 */
esp_err_t mqtt_manager_start(void);
