#include "esp_err.h"
#include "esp_log.h"
#include "app/actuator_controller.h"
#include "services/connectivity/mqtt_manager.h"
#include "services/connectivity/wifi_manager.h"

static const char *TAG = "rm_led";

/** Initialize actuators, connect to Wi-Fi, then start MQTT; stop on startup errors. */
void app_main(void)
{
    esp_err_t actuator_err = actuator_controller_init();
    if (actuator_err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize actuators: %s", esp_err_to_name(actuator_err));
        return;
    }

    esp_err_t wifi_err = wifi_manager_start();
    if (wifi_err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wi-Fi manager: %s", esp_err_to_name(wifi_err));
        return;
    }

    if (!wifi_manager_wait_for_connection()) {
        ESP_LOGE(TAG, "Could not connect to Wi-Fi after the retry limit");
        return;
    }

    esp_err_t mqtt_err = mqtt_manager_start();
    if (mqtt_err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start MQTT manager: %s", esp_err_to_name(mqtt_err));
    }
}
