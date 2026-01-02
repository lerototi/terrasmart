#include <ESP8266WiFi.h>
#include "telemetry.h"
#include "../mqtt/mqtt_manager.h"
#include "config.h"

void sendTelemetry() {
  // Verificar se MQTT está conectado
  if (!mqttIsConnected()) {
    Serial.println("[TELEMETRY] ✗ Não enviado - MQTT desconectado");
    return;
  }

  float temperature = random(200, 320) / 10.0;

  String payload = "{";
  payload += "\"temperature\":" + String(temperature, 1) + ",";
  payload += "\"uptime\":" + String(millis() / 1000) + ",";
  payload += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  payload += "\"rssi\":" + String(WiFi.RSSI());
  payload += "}";

  publishMessage(MQTT_TELEMETRY_TOPIC, payload.c_str());

  Serial.print("[TELEMETRY] ✓ ");
  Serial.println(payload);
}

