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
// Intervalo mínimo entre leituras do sensor (DHT22 precisa de 2s)
#define SENSOR_READ_INTERVAL 3000 // 3 segundos

// Intervalo de telemetria forçada (heartbeat - mesmo sem mudança)
#define TELEMETRY_FORCE_INTERVAL 300000 // 5 minutos

// Thresholds para detectar mudança significativa
#define TEMP_CHANGE_THRESHOLD 0.3     // 0.3°C de diferença
#define HUMIDITY_CHANGE_THRESHOLD 1.0 // 1% de diferença

/* ================= NTP ================= */
#define NTP_SERVER "10.0.0.84"   // Servidor NTP local (Home Assistant)
#define NTP_TIMEZONE -3          // Timezone offset em horas (UTC-3 para Brasil)
#define NTP_UPDATE_INTERVAL 3600 // Atualizar a cada 1 hora (3600s)
