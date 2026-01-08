#include "wifi_manager.h"
#include <ESP8266WiFi.h>
#include "setup_manager.h"

extern SetupManager g_setupManager;

bool connectWifi(const DeviceConfig &config)
{
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║     CONECTANDO AO WiFi             ║");
  Serial.println("╚════════════════════════════════════╝");

  // Mostrar configuração
  Serial.printf("[WiFi] SSID: %s\n", config.wifiSsid.c_str());
  Serial.printf("[WiFi] Senha: %s\n", config.wifiPass.c_str());
  Serial.printf("[WiFi] Comprimento SSID: %d\n", config.wifiSsid.length());
  Serial.printf("[WiFi] Comprimento Senha: %d\n", config.wifiPass.length());

  // Verificar se SSID não está vazio
  if (config.wifiSsid.length() == 0)
  {
    Serial.println("[ERROR] SSID vazio!");
    return false;
  }

  // Configurar modo WiFi
  WiFi.disconnect(true); // desconectar e desligar AP anterior
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  // Tentar conectar
  Serial.println("[WiFi] Iniciando conexão...");
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());

  unsigned long start = millis();
  int attemptCount = 0;

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000); // Aumentado de 500ms para 1000ms para reduzir spam de logs
    attemptCount++;

    int status = WiFi.status();
    Serial.printf("[WiFi] Status: %d ", status);

    // Mostrar significado do status
    switch (status)
    {
    case WL_IDLE_STATUS:
      Serial.print("(Idle)");
      break;
    case WL_NO_SSID_AVAIL:
      Serial.print("(SSID não encontrado)");
      break;
    case WL_SCAN_COMPLETED:
      Serial.print("(Scan completo)");
      break;
    case WL_CONNECTED:
      Serial.print("(Conectado)");
      break;
    case WL_CONNECT_FAILED:
      Serial.print("(Falha na conexão)");
      break;
    case WL_CONNECTION_LOST:
      Serial.print("(Conexão perdida)");
      break;
    case WL_DISCONNECTED:
      Serial.print("(Desconectado)");
      break;
    default:
      Serial.print("(Desconhecido)");
    }
    Serial.printf(" - Tentativa %d (%.1fs)\n", attemptCount, (millis() - start) / 1000.0);

    if (millis() - start > 15000)
    {
      Serial.println("\n[ERROR] Timeout ao conectar WiFi (15s)");
      Serial.printf("[WiFi] SSID: %s\n", config.wifiSsid.c_str());
      Serial.println("[WiFi] Possíveis causas:");
      Serial.println("  - SSID não existe");
      Serial.println("  - Senha incorreta");
      Serial.println("  - Sinal WiFi muito fraco");
      Serial.println("  - Outros problemas de conectividade");
      return false;
    }
  }

  Serial.println("\n[WiFi] ✓ Conectado com sucesso!");
  Serial.printf("[WiFi] SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
  Serial.printf("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("[WiFi] DNS: %s\n", WiFi.dnsIP().toString().c_str());
  Serial.println();

  return true;
}

bool checkWiFiConnection()
{
  static bool wasConnected = true; // Assume estava conectado inicialmente

  if (WiFi.status() == WL_CONNECTED)
  {
    // WiFi OK
    if (!wasConnected)
    {
      // Recuperou de uma desconexão - resetar contador de falhas
      Serial.println("[WiFi] ✓ Reconectado com sucesso!");
      g_setupManager.recordWiFiSuccess();
      wasConnected = true;
    }
    return true;
  }
  else
  {
    // WiFi falhou
    if (wasConnected)
    {
      Serial.printf("[WiFi] ✗ Conexão perdida (status: %d)\n", WiFi.status());
      wasConnected = false;
    }
    // Não registrar falha - apenas retry contínuo
    return false;
  }
}