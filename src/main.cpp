#include <Arduino.h>
#include "config.h"
#include "config/config_manager.h"
#include "wifi/wifi_manager.h"
#include "mqtt/mqtt_manager.h"
#include "telemetry/telemetry.h"
#include "captive/captive_portal.h"
#include "button_handler.h"
#include "setup_manager.h"

unsigned long lastTelemetry = 0;

// Instâncias globais para botão e setup manager
ButtonHandler buttonHandler;
extern SetupManager g_setupManager;

enum DeviceMode
{
  MODE_AP,
  MODE_OPERATIONAL
};
DeviceMode deviceMode;
DeviceConfig config;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Inicializar botão de reset (GPIO0 = D3 no ESP8266)
  buttonHandler.begin(D5);
  Serial.printf("[INIT] Botão inicializado no pino GPIO%d (D3)\n", D5);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║        ESP INICIADO                ║");
  Serial.println("╚════════════════════════════════════╝");

  if (!hasValidConfig())
  {
    Serial.println("[BOOT] Sem config → modo AP + Portal Captivo");
    deviceMode = MODE_AP;
    startCaptivePortal();
    return;
  }

  Serial.println("[BOOT] Config encontrada → modo operacional");
  deviceMode = MODE_OPERATIONAL;

  if (!loadConfig(config))
  {
    Serial.println("[ERROR] Falha ao carregar config");
    return;
  }

  Serial.printf("[BOOT] Tentando conectar ao WiFi: %s\n", config.wifiSsid.c_str());

  if (!connectWifi(config))
  {
    Serial.println("[ERROR] Falha ao conectar WiFi");
    Serial.println("[BOOT] Retornando ao modo AP...");
    deviceMode = MODE_AP;
    startCaptivePortal();
    return;
  }

  Serial.println("[BOOT] WiFi conectado! Iniciando MQTT...");
  setupMQTT(config);
  Serial.println("[BOOT] Setup completo!\n");
}

void loop()
{
  // Verificar botão de reset (pressionar por 5 segundos) - ANTES de qualquer outra lógica
  if (buttonHandler.checkAndClearLongPress())
  {
    Serial.println("[RESET] Executando reset de configuração...");
    g_setupManager.resetConfiguration();
    deviceMode = MODE_AP;
    startCaptivePortal();
    return;
  }

  if (deviceMode == MODE_AP)
  {
    // Modo AP com portal captivo
    captiveLoop();
    return;
  }

  // Modo Operacional
  if (deviceMode == MODE_OPERATIONAL)
  {
    // Debug: log a cada 5 segundos para confirmar que está no modo operacional
    static unsigned long lastDebugLog = 0;
    if (millis() - lastDebugLog > 5000)
    {
      lastDebugLog = millis();
      Serial.println("[DEBUG] Modo operacional ativo - verificando conexões...");
    }

    // Verificar se deve fazer rollback por falhas consecutivas
    if (g_setupManager.getOperationalState() == OPERATIONAL_ROLLBACK)
    {
      Serial.println("\n[ROLLBACK] Estado de rollback detectado - Resetando configurações...");
      g_setupManager.resetConfiguration();
      deviceMode = MODE_AP;
      startCaptivePortal();
      return;
    }

    // Verificar conexão WiFi continuamente
    checkWiFiConnection();

    // Processar MQTT e manter conexão
    loopMQTT();

    // Enviar telemetria periodicamente
    if (millis() - lastTelemetry > TELEMETRY_INTERVAL)
    {
      lastTelemetry = millis();
      Serial.println("[DEBUG] Enviando telemetria...");
      sendTelemetry();
    }
  }
}
