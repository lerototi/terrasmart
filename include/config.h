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
#define TELEMETRY_INTERVAL 30000 // 30 segundos (otimizado para performance)

/* ================= NTP ================= */
#define NTP_SERVER "10.0.0.84"     // Servidor NTP local (Home Assistant)
#define NTP_TIMEZONE -3            // Timezone offset em horas (UTC-3 para Brasil)
#define NTP_UPDATE_INTERVAL 3600   // Atualizar a cada 1 hora (3600s)

