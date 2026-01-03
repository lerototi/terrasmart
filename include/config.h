#pragma once

/* ================= DEVICE ================= */
#define DEVICE_ID "esp_sensor_01"

/* ================= TOPICS ================= */
#define MQTT_BASE_TOPIC "addon/esp/"

#define MQTT_TELEMETRY_TOPIC MQTT_BASE_TOPIC DEVICE_ID "/telemetry"
#define MQTT_STATUS_TOPIC MQTT_BASE_TOPIC DEVICE_ID "/status"
#define MQTT_CMD_TOPIC MQTT_BASE_TOPIC DEVICE_ID "/cmd"
#define MQTT_RESP_TOPIC MQTT_BASE_TOPIC DEVICE_ID "/resp"

/* ================= TIMERS ================= */
#define TELEMETRY_INTERVAL 5000
