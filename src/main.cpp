#include <Arduino.h>
#include "config.h"
#include "config/config_manager.h"
#include "wifi/wifi_manager.h"
#include "mqtt/mqtt_manager.h"
#include "telemetry/telemetry.h"
#include "captive/captive_portal.h"

unsigned long lastTelemetry = 0;

enum DeviceMode {
  MODE_AP,
  MODE_OPERATIONAL
};
DeviceMode deviceMode;
DeviceConfig config;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║        ESP INICIADO                ║");
  Serial.println("╚════════════════════════════════════╝");

  if (!hasValidConfig()) {
    Serial.println("[BOOT] Sem config → modo AP + Portal Captivo");
    deviceMode = MODE_AP;
    startCaptivePortal();
    return;
  }

  Serial.println("[BOOT] Config encontrada → modo operacional");
  deviceMode = MODE_OPERATIONAL;
  
  if (!loadConfig(config)) {
    Serial.println("[ERROR] Falha ao carregar config");
    return;
  }
  
  Serial.printf("[BOOT] Tentando conectar ao WiFi: %s\n", config.wifiSsid.c_str());
  
  if (!connectWifi(config)) {
    Serial.println("[ERROR] Falha ao conectar WiFi");
    Serial.println("[BOOT] Retornando ao modo AP...");
    deviceMode = MODE_AP;
    startCaptivePortal();
    return;
  }
  
  Serial.println("[BOOT] WiFi conectado! Iniciando MQTT...");
  setupMQTT();
  Serial.println("[BOOT] Setup completo!\n");
}

void loop() {
  if (deviceMode == MODE_AP) {
    // Modo AP com portal captivo
    captiveLoop();
    return;
  }

  // Modo Operacional
  if (deviceMode == MODE_OPERATIONAL) {
    // Processar MQTT e manter conexão
    loopMQTT();

    // Enviar telemetria periodicamente
    if (millis() - lastTelemetry > TELEMETRY_INTERVAL) {
      lastTelemetry = millis();
      sendTelemetry();
    }
  }
}
