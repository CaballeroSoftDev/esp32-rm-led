#pragma once

/* Keep this device identifier aligned with the MQTT client ID. */
#define MQTT_DEVICE_ID "rm-led-esp32s3"

#define MQTT_TOPIC_ROOT MQTT_DEVICE_ID

/* Commands published by the backend and subscribed to by this device. */
/* Payload format: "R,G,B", with each RGB component in the range 0-255. */
#define MQTT_TOPIC_LED_WS2812_SET MQTT_TOPIC_ROOT "/cmd/led/ws2812/set"
/* Payload is '1' to turn on or '0' to turn off. */
#define MQTT_TOPIC_LED_SIMPLE_SET MQTT_TOPIC_ROOT "/cmd/led/simple/set"

/* Retained online/offline availability published by the device or broker LWT. */
#define MQTT_TOPIC_AVAILABILITY MQTT_TOPIC_ROOT "/availability"
