#include "mqtt_manager.h"

#include "app/actuator_controller.h"
#include "app/mqtt_topics.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "sdkconfig.h"
#include <string.h>

#define MQTT_STATUS_TOPIC MQTT_TOPIC_AVAILABILITY

static const char *TAG = "mqtt_manager";
static esp_mqtt_client_handle_t s_mqtt_client;

/**
 * Subscribe to a command topic at QoS 0.
 * @param client MQTT client handle.
 * @param topic Null-terminated topic name.
 */
static void subscribe_to_topic(esp_mqtt_client_handle_t client, const char *topic)
{
    int message_id = esp_mqtt_client_subscribe(client, topic, 0);
    if (message_id < 0) {
        ESP_LOGE(TAG, "Could not subscribe to topic: %s", topic);
    } else {
        ESP_LOGI(TAG, "Subscribe request sent for '%s' (message id=%d)", topic, message_id);
    }
}

/**
 * Dispatch MQTT connection, subscription, data, and error events.
 * @param handler_args Unused callback context.
 * @param base Event category.
 * @param event_id MQTT event identifier.
 * @param event_data MQTT event details.
 */
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to the MQTT broker");

        int status_message_id = esp_mqtt_client_publish(
            event->client, MQTT_STATUS_TOPIC, "online", 0, 1, 1);
        if (status_message_id < 0) {
            ESP_LOGE(TAG, "Could not publish online status to '%s'", MQTT_STATUS_TOPIC);
        } else {
            ESP_LOGI(TAG, "Published online status to '%s' (message id=%d)",
                     MQTT_STATUS_TOPIC, status_message_id);
        }

        subscribe_to_topic(event->client, MQTT_TOPIC_LED_WS2812_SET);
        subscribe_to_topic(event->client, MQTT_TOPIC_LED_SIMPLE_SET);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscription confirmed (message id=%d)", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        /* Commands are small; fragmented payloads are not reassembled here. */
        if (event->current_data_offset != 0 || event->data_len != event->total_data_len) {
            ESP_LOGW(TAG, "Ignoring incomplete MQTT message");
            break;
        }

        actuator_controller_handle_message(event->topic,
                                           (size_t)event->topic_len,
                                           event->data,
                                           (size_t)event->data_len);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from the MQTT broker");
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT client reported an error");
        break;

    default:
        break;
    }
}

esp_err_t mqtt_manager_start(void)
{
    if (s_mqtt_client != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = CONFIG_RM_LED_MQTT_BROKER_URI,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.username = CONFIG_RM_LED_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_RM_LED_MQTT_PASSWORD,
        .credentials.client_id = MQTT_DEVICE_ID,
        .session.last_will = {
            .topic = MQTT_STATUS_TOPIC,
            .msg = "offline",
            .msg_len = sizeof("offline") - 1,
            .qos = 1,
            .retain = 1,
        },
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_config);
    if (s_mqtt_client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_mqtt_client_register_event(
        s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return err;
    }

    err = esp_mqtt_client_start(s_mqtt_client);
    if (err != ESP_OK) {
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return err;
    }

    ESP_LOGI(TAG, "MQTT client started; waiting for broker connection");
    return ESP_OK;
}
