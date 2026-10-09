#include "wifi_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

/* Event-group signals consumed by wifi_manager_wait_for_connection(). */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT BIT1
/* Reconnection attempts after the initial connection attempt. */
#define WIFI_MAX_RETRIES 10

static const char *TAG = "wifi_manager";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count = 0;

/**
 * Initialize NVS, erasing it only when its stored data is incompatible.
 * @return ESP_OK on success, or the NVS error.
 */
static esp_err_t initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Reinitializing NVS after initialization error: %s", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err != ESP_OK)
        {
            return err;
        }
        err = nvs_flash_init();
    }

    return err;
}

/**
 * Handle station and IP events, including connection retries.
 * @param arg Unused callback context.
 * @param event_base Event category.
 * @param event_id Event identifier within the category.
 * @param event_data Event-specific data.
 */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Wi-Fi station started; connecting to the access point");
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Could not start Wi-Fi connection: %s", esp_err_to_name(err));
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAILED_BIT);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (s_retry_count < WIFI_MAX_RETRIES)
        {
            s_retry_count++;
            ESP_LOGW(TAG, "Disconnected; retry %u/%d", s_retry_count, WIFI_MAX_RETRIES);

            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not start reconnect attempt: %s", esp_err_to_name(err));
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAILED_BIT);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Connection failed after the initial attempt and %d retries",
                     WIFI_MAX_RETRIES);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAILED_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        s_retry_count = 0;
        ESP_LOGI(TAG, "Connected; IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * Initialize the Wi-Fi station and register its asynchronous event handlers.
 * @return ESP_OK on success, or an ESP-IDF initialization error.
 */
esp_err_t wifi_manager_start(void)
{
    esp_err_t err = initialize_nvs();
    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK)
    {
        return err;
    }

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (sta_netif == NULL)
    {
        return ESP_FAIL;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL)
    {
        esp_netif_destroy_default_wifi(sta_netif);
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK)
    {
        goto cleanup;
    }

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    if (err != ESP_OK)
    {
        goto cleanup_wifi;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    if (err != ESP_OK)
    {
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
        goto cleanup_wifi;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_RM_LED_WIFI_SSID,
            .password = CONFIG_RM_LED_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        goto cleanup_handlers;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
    {
        goto cleanup_handlers;
    }

    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        goto cleanup_handlers;
    }

    ESP_LOGI(TAG, "Wi-Fi initialized in station mode");
    return ESP_OK;

/* Release acquired resources in reverse order, according to the failure stage. */
cleanup_handlers:
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
cleanup_wifi:
    esp_wifi_deinit();
cleanup:
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;
    esp_netif_destroy_default_wifi(sta_netif);
    return err;
}

bool wifi_manager_wait_for_connection(void)
{
    if (s_wifi_event_group == NULL)
    {
        return false;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    return (bits & WIFI_CONNECTED_BIT) != 0;
}
