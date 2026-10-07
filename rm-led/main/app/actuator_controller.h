#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "mqtt_topics.h"

/**
 * Initialize both LED drivers; both LEDs start off.
 * @return ESP_OK on success, or the first driver initialization error.
 */
esp_err_t actuator_controller_init(void);

/**
 * Route a complete MQTT command to the actuator selected by its topic.
 * @param topic Topic bytes; need not be null-terminated.
 * @param topic_len Number of bytes in topic.
 * @param payload Command payload bytes; need not be null-terminated.
 * @param payload_len Number of bytes in payload.
 */
void actuator_controller_handle_message(const char *topic,
                                        size_t topic_len,
                                        const char *payload,
                                        size_t payload_len);
