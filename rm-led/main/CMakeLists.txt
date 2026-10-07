idf_component_register(
    SRCS
        "main.c"
        "app/actuator_controller.c"
        "services/connectivity/wifi_manager.c"
        "services/connectivity/mqtt_manager.c"
        "devices/led/ws2812.c"
        "devices/led/gpio_led.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_event esp_netif esp_wifi esp-tls esp_driver_gpio mqtt nvs_flash
)
