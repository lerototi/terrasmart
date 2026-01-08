#include <ESP8266WiFi.h>
#include <time.h>
#include "telemetry.h"
#include "../mqtt/mqtt_manager.h"
#include "config.h"
#include "../sensors/sensor_base.h"
#include "../include/device_config.h"

// Instância global do sensor
static BaseSensor *g_sensor = nullptr;

// Função para obter timestamp ISO8601
static String getISOTimestamp()
{
    time_t now = time(nullptr);

    // Se não sincronizou ainda, retornar uptime
    if (now < 100000)
    {
        return "uptime:" + String(millis() / 1000) + "s";
    }

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);

    // Adicionar timezone offset (definido em config.h)
    sprintf(buffer + strlen(buffer), "%+03d:00", NTP_TIMEZONE);

    return String(buffer);
}

bool initSensor()
{
    if (g_sensor)
    {
        return true; // Já inicializado
    }

    Serial.println("[TELEMETRY] Inicializando sensor...");

    g_sensor = SensorFactory::createSensor((SensorType)SENSOR_TYPE);
    if (!g_sensor)
    {
        Serial.println("[TELEMETRY] ✗ Falha ao criar sensor");
        return false;
    }

    if (!g_sensor->begin())
    {
        Serial.println("[TELEMETRY] ✗ Falha ao inicializar sensor");
        SensorFactory::destroySensor(g_sensor);
        g_sensor = nullptr;
        return false;
    }

    Serial.printf("[TELEMETRY] ✓ Sensor inicializado: %s\n", g_sensor->getName());
    return true;
}

void sendTelemetry()
{
    // Verificar se MQTT está conectado
    if (!mqttIsConnected())
    {
        Serial.println("[TELEMETRY] ✗ Não enviado - MQTT desconectado");
        return;
    }

    // Inicializar sensor se necessário
    if (!g_sensor && !initSensor())
    {
        Serial.println("[TELEMETRY] ✗ Não enviado - sensor não disponível");
        return;
    }

    // Ler dados do sensor
    SensorData data = g_sensor->read();
    if (!data.valid)
    {
        Serial.println("[TELEMETRY] ✗ Não enviado - dados do sensor inválidos");
        return;
    }

    // Criar timestamp ISO 8601 (sincronizado com NTP local)
    String timestamp = getISOTimestamp();

    // Obter IP do dispositivo
    String ipAddress = WiFi.localIP().toString();

    // Payload estruturado: separação entre device info e readings
    String payload = "{";

    // === DEVICE: Informações do hardware e status ===
    payload += "\"device\":{";
    payload += "\"id\":\"" + String(DEVICE_ID) + "\",";
    payload += "\"type\":\"" + String(g_sensor->getName()) + "\",";
    payload += "\"uptime\":" + String(millis() / 1000) + ",";
    payload += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    payload += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    payload += "\"ip\":\"" + ipAddress + "\"";
    payload += "},";

    // === READINGS: Array dinâmico de leituras ===
    payload += "\"readings\":[";

    // Leitura 1: Temperatura
    if (data.temperature != 0)
    {
        payload += "{";
        payload += "\"type\":\"temperature\",";
        payload += "\"value\":" + String(data.temperature, 1) + ",";
        payload += "\"unit\":\"°C\"";
        payload += "}";
    }

    // Leitura 2: Umidade (se disponível)
    if (data.humidity != 0)
    {
        if (data.temperature != 0)
            payload += ",";
        payload += "{";
        payload += "\"type\":\"humidity\",";
        payload += "\"value\":" + String(data.humidity, 1) + ",";
        payload += "\"unit\":\"%\"";
        payload += "}";
    }

    payload += "],";

    // === TIMESTAMP: Momento da coleta ===
    payload += "\"timestamp\":\"" + String(timestamp) + "\"";

    payload += "}";

    publishMessage(MQTT_TELEMETRY_TOPIC, payload.c_str());

    Serial.print("[TELEMETRY] ✓ Enviado para ");
    Serial.print(MQTT_TELEMETRY_TOPIC);
    Serial.print(": ");
    Serial.println(payload);
}
