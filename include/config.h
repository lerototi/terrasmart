#pragma once

/* ================= DEVICE ================= */
#define DEVICE_ID "esp_sensor_01"

/* ================= MQTT ================= */
#define MQTT_HOST "10.0.0.84"
#define MQTT_PORT 1883
#define MQTT_USER "mqtt_addon"
#define MQTT_PASS "1234"

/* ================= TOPICS ================= */
#define MQTT_BASE_TOPIC "addon/esp/"

#define MQTT_TELEMETRY_TOPIC MQTT_BASE_TOPIC DEVICE_ID "/telemetry"
#define MQTT_STATUS_TOPIC    MQTT_BASE_TOPIC DEVICE_ID "/status"
#define MQTT_CMD_TOPIC       MQTT_BASE_TOPIC DEVICE_ID "/cmd"
#define MQTT_RESP_TOPIC      MQTT_BASE_TOPIC DEVICE_ID "/resp"

/* ================= TIMERS ================= */
#define TELEMETRY_INTERVAL 5000
