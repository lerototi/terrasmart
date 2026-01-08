#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "command_handler.h"
#include "config/config_manager.h"
#include "setup_manager.h"

extern SetupManager g_setupManager;

WiFiClient espClient;
PubSubClient mqtt(espClient);

// Configuração MQTT atual (carregada da memória)
static DeviceConfig currentMQTTConfig;

static unsigned long lastReconnectAttempt = 0;
static const unsigned long RECONNECT_INTERVAL = 5000; // 5 segundos entre tentativas

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String msg;
  for (uint8_t i = 0; i < length; i++)
  {
    msg += (char)payload[i];
  }
  handleCommand(msg);
}

void setupMQTT(const DeviceConfig &config)
{
  // Armazenar configuração MQTT atual
  currentMQTTConfig = config;

  mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);

  // Debug: mostrar configurações
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║     CONFIGURAÇÃO MQTT              ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.printf("[MQTT] Host: %s\n", config.mqttHost.c_str());
  Serial.printf("[MQTT] Porta: %d\n", config.mqttPort);
  Serial.printf("[MQTT] Usuário: %s\n", config.mqttUser.c_str());
  Serial.printf("[MQTT] Senha: %s\n", config.mqttPass.c_str());
  Serial.printf("[MQTT] Device ID: %s\n", DEVICE_ID);
  Serial.printf("[MQTT] Tópico Status: %s\n", MQTT_STATUS_TOPIC);
  Serial.printf("[MQTT] Tópico Telemetria: %s\n", MQTT_TELEMETRY_TOPIC);
  Serial.printf("[MQTT] Tópico Comando: %s\n", MQTT_CMD_TOPIC);
  Serial.println();
}

void publishStatus(const char *status)
{
  if (mqtt.connected())
  {
    mqtt.publish(MQTT_STATUS_TOPIC, status, true);
    Serial.printf("[MQTT] Status publicado: %s\n", status);
  }
  else
  {
    Serial.printf("[MQTT] Falha ao publicar status (desconectado): %s\n", status);
  }
}

void publishMessage(const char *topic, const char *payload)
{
  if (mqtt.connected())
  {
    mqtt.publish(topic, payload, true);
    Serial.printf("[MQTT] Mensagem publicada em %s\n", topic);
  }
  else
  {
    Serial.printf("[MQTT] Falha ao publicar (desconectado) em %s\n", topic);
  }
}

bool mqttIsConnected()
{
  return mqtt.connected();
}

void loopMQTT()
{
  // Verificar se WiFi está conectado
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  // Se já está conectado, apenas processar
  if (mqtt.connected())
  {
    mqtt.loop();
    return;
  }

  // Se desconectado, tentar reconectar (com throttle)
  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL)
  {
    return; // Não reconectar ainda
  }

  lastReconnectAttempt = now;

  Serial.print("[MQTT] Conectando em ");
  Serial.print(currentMQTTConfig.mqttHost);
  Serial.print(":");
  Serial.println(currentMQTTConfig.mqttPort);

  if (mqtt.connect(
          DEVICE_ID,
          currentMQTTConfig.mqttUser.c_str(),
          currentMQTTConfig.mqttPass.c_str(),
          MQTT_STATUS_TOPIC,
          1,
          true,
          "offline"))
  {
    Serial.println("[MQTT] ✓ Conectado!");
    publishStatus("online");
    mqtt.subscribe(MQTT_CMD_TOPIC);

    // Registrar sucesso de MQTT
    g_setupManager.recordMQTTSuccess();
  }
  else
  {
    int state = mqtt.state();
    Serial.print("[MQTT] ✗ Falha rc=");
    Serial.print(state);
    Serial.println(" - Retry em 5s");
    Serial.println("[MQTT] 💡 Reset: botão 5s");
  }
}

void testMQTTConnection()
{
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   TESTE DE CONEXÃO MQTT            ║");
  Serial.println("╚════════════════════════════════════╝");

  // 1. Verificar WiFi
  Serial.println("\n1️⃣  VERIFICANDO WIFI:");
  Serial.printf("   Status: %d (esperado: %d)\n", WiFi.status(), WL_CONNECTED);
  Serial.printf("   SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("   IP Local: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("   RSSI: %d dBm\n", WiFi.RSSI());

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("   ✗ WiFi NÃO CONECTADO!");
    return;
  }
  Serial.println("   ✓ WiFi OK");

  // 2. Verificar Configuração
  Serial.println("\n2️⃣  CONFIGURAÇÃO MQTT:");
  Serial.printf("   Host: %s\n", currentMQTTConfig.mqttHost.c_str());
  Serial.printf("   Porta: %d\n", currentMQTTConfig.mqttPort);
  Serial.printf("   Usuário: %s\n", currentMQTTConfig.mqttUser.c_str());
  Serial.printf("   Senha: %s\n", currentMQTTConfig.mqttPass.c_str());
  Serial.printf("   Device ID: %s\n", DEVICE_ID);

  // 3. Tentar Conectar
  Serial.println("\n3️⃣  TENTANDO CONECTAR:");
  WiFiClient testClient;

  Serial.printf("   Conectando em %s:%d... ", currentMQTTConfig.mqttHost.c_str(), currentMQTTConfig.mqttPort);
  if (testClient.connect(currentMQTTConfig.mqttHost.c_str(), currentMQTTConfig.mqttPort))
  {
    Serial.println("✓ Sucesso!");
    testClient.stop();
  }
  else
  {
    Serial.println("✗ Falha!");
    Serial.println("   Possíveis causas:");
    Serial.println("   - IP/Hostname incorreto");
    Serial.println("   - Porta incorreta");
    Serial.println("   - Firewall bloqueando");
    Serial.println("   - Servidor não está ativo");
  }

  // 4. Status MQTT
  Serial.println("\n4️⃣  STATUS MQTT ATUAL:");
  Serial.printf("   Conectado: %s\n", mqtt.connected() ? "Sim ✓" : "Não ✗");
  Serial.printf("   Estado: %d\n", mqtt.state());

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   FIM DO TESTE                     ║");
  Serial.println("╚════════════════════════════════════╝\n");
}
