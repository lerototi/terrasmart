#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "config/config_manager.h"
#include "wifi/wifi_manager.h"
#include "mqtt/mqtt_manager.h"
#include "telemetry/telemetry.h"
#include "captive/captive_portal.h"
#include "button_handler.h"
#include "setup_manager.h"
#include "sensors/sensor_base.h"
#include "sensors/a02yyuw_sensor.h"

unsigned long lastSensorRead = 0;
unsigned long lastTelemetry = 0;
unsigned long lastNTPSync = 0;
bool ntpSynced = false;

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

  // Sincronizar NTP com múltiplos servidores (local + público)
  Serial.println("[NTP] Sincronizando relógio...");
  Serial.printf("[NTP] Servidores: %s, pool.ntp.br, time.google.com\n", NTP_SERVER);
  Serial.printf("[NTP] Timezone: UTC%+d\n", NTP_TIMEZONE);

  // Configura 3 servidores: 1) HA local, 2) Pool brasileiro, 3) Google
  configTime(NTP_TIMEZONE * 3600, 0, NTP_SERVER, "pool.ntp.br", "time.google.com");

  // Inicializar sensor
  if (!initSensor())
  {
    Serial.println("[BOOT] ⚠ Aviso: Sensor não inicializado, mas continuando...");
  }

  Serial.println("[BOOT] Setup completo!\n");
}

// Função para obter timestamp ISO8601
String getISOTimestamp()
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

  // Adicionar timezone offset
  sprintf(buffer + strlen(buffer), "%+03d:00", NTP_TIMEZONE);

  return String(buffer);
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
  else if (deviceMode == MODE_OPERATIONAL)
  {
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
    yield(); // Dar tempo ao watchdog

    // Processar MQTT e manter conexão
    loopMQTT();
    yield(); // Dar tempo ao watchdog

    // Verificar e logar sincronização NTP (apenas uma vez)
    if (!ntpSynced)
    {
      time_t now = time(nullptr);
      if (now > 100000) // Timestamp válido
      {
        ntpSynced = true;
        Serial.printf("[NTP] ✓ Sincronizado: %s\n", getISOTimestamp().c_str());
      }
    }

    // Re-sincronizar NTP a cada hora
    if (millis() - lastNTPSync > (NTP_UPDATE_INTERVAL * 1000UL))
    {
      lastNTPSync = millis();
      configTime(NTP_TIMEZONE * 3600, 0, NTP_SERVER, "pool.ntp.br", "time.google.com");
    }

    // Telemetria por evento: ler sensor frequentemente mas enviar só quando houver mudança
    unsigned long now = millis();

    if (now - lastSensorRead >= SENSOR_READ_INTERVAL)
    {
      lastSensorRead = now;

      SensorData data = readSensor();

      if (!data.valid)
      {
        // Se leitura inválida, tentar novamente no próximo ciclo
        return;
      }

      // Obter sensor para verificar mudanças
      BaseSensor *sensor = getSensor();
      if (!sensor)
      {
        return;
      }

      // Cast para A02YYUWSensor para acessar métodos específicos
      A02YYUWSensor *ultrasonicSensor = static_cast<A02YYUWSensor *>(sensor);

      // Verificar se houve mudança significativa ou se passou do intervalo de heartbeat
      bool hasChange = ultrasonicSensor->hasSignificantChange(data);
      bool forceHeartbeat = (now - lastTelemetry >= TELEMETRY_FORCE_INTERVAL);

      if (hasChange || forceHeartbeat)
      {
        lastTelemetry = now;

        // Definir tipo de trigger
        TelemetryTrigger trigger = hasChange ? TRIGGER_CHANGE : TRIGGER_HEARTBEAT;
        sendTelemetry(trigger);

        ultrasonicSensor->updateLastSent(data);

        if (hasChange)
        {
          Serial.printf("[Telemetry] Mudança detectada - Nível: %.1f%%, Distância: %.1fcm\n",
                        data.temperature, data.distance);
        }
        else
        {
          Serial.println("[Telemetry] Heartbeat forçado (5min)");
        }
      }
    }

    yield(); // Dar tempo ao watchdog
  }
}
