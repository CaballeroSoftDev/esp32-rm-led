#include "actuator_controller.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "devices/led/gpio_led.h"
#include "devices/led/ws2812.h"

static const char *TAG = "actuator_controller";

/**
 * Parse an RGB payload and apply it to the WS2812.
 * @param payload Payload bytes; need not be null-terminated.
 * @param payload_len Number of bytes in payload.
 */
static void handle_led_message(const char *payload, size_t payload_len)
{
    /* Fit the longest RGB text and its terminating null byte. */
    char text[sizeof("255,255,255")];
    if (payload == NULL || payload_len == 0 || payload_len >= sizeof(text)) {
        ESP_LOGW(TAG, "RGB message does not fit the expected payload buffer");
        return;
    }

    memcpy(text, payload, payload_len);
    text[payload_len] = '\0';

    /* Assumes the backend validates RGB syntax and 0-255 ranges. */
    unsigned red = 0;
    unsigned green = 0;
    unsigned blue = 0;
    (void)sscanf(text, "%u,%u,%u", &red, &green, &blue);

    esp_err_t err = led_ws2812_set_color((uint8_t)red, (uint8_t)green, (uint8_t)blue);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not set WS2812 color: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGD(TAG, "WS2812 set to R=%u G=%u B=%u", red, green, blue);
}

/**
 * Apply the first payload byte as the GPIO LED command.
 * @param payload Payload bytes; need not be null-terminated.
 * @param payload_len Number of bytes in payload.
 */
static void handle_gpio_led_message(const char *payload, size_t payload_len)
{
    /* Assumes the payload is '1' (on) or '0' (off); only its first byte is read. */
    if (payload == NULL || payload_len == 0) {
        ESP_LOGW(TAG, "GPIO LED command has no payload");
        return;
    }

    const bool on = payload[0] == '1';
    esp_err_t err = gpio_led_set(on);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not set GPIO LED: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGD(TAG, "Two-pin LED turned %s", on ? "on" : "off");
}

esp_err_t actuator_controller_init(void)
{
    esp_err_t err = led_ws2812_init();
    if (err != ESP_OK) {
        return err;
    }

    err = gpio_led_init();
    if (err != ESP_OK) {
        (void)led_ws2812_deinit();
        return err;
    }

    return ESP_OK;
}

void actuator_controller_handle_message(const char *topic,
                                        size_t topic_len,
                                        const char *payload,
                                        size_t payload_len)
{
    if (topic == NULL) {
        ESP_LOGW(TAG, "Ignoring MQTT message without a topic");
        return;
    }

    /* MQTT supplies a byte length, not a null-terminated topic string. */
    if (topic_len == sizeof(MQTT_TOPIC_LED_WS2812_SET) - 1 &&
        memcmp(topic, MQTT_TOPIC_LED_WS2812_SET, topic_len) == 0) {
        handle_led_message(payload, payload_len);
    } else if (topic_len == sizeof(MQTT_TOPIC_LED_SIMPLE_SET) - 1 &&
               memcmp(topic, MQTT_TOPIC_LED_SIMPLE_SET, topic_len) == 0) {
        handle_gpio_led_message(payload, payload_len);
    } else {
        ESP_LOGW(TAG, "No actuator is registered for the received topic");
    }
}
