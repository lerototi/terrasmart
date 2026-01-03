#include "config_manager.h"
#include <LittleFS.h>

static const char *CONFIG_FILE = "/config.json";

bool loadConfig(DeviceConfig &config)
{
  Serial.println("\n[CONFIG] Carregando configuração de LittleFS...");

  if (!LittleFS.begin())
  {
    Serial.println("[CONFIG] ✗ Falha ao inicializar LittleFS");
    return false;
  }

  if (!LittleFS.exists(CONFIG_FILE))
  {
    Serial.printf("[CONFIG] ✗ Arquivo %s não existe\n", CONFIG_FILE);
    return false;
  }

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f)
  {
    Serial.println("[CONFIG] ✗ Falha ao abrir arquivo de config");
    return false;
  }

  String json = f.readString();
  f.close();

  Serial.printf("[CONFIG] JSON bruto: %s\n", json.c_str());

  // Parse SSID
  int ssidStart = json.indexOf("\"ssid\"");
  if (ssidStart < 0)
  {
    Serial.println("[CONFIG] ✗ JSON inválido - campo 'ssid' não encontrado");
    return false;
  }

  int ssidValueStart = json.indexOf("\"", ssidStart + 7) + 1;
  int ssidValueEnd = json.indexOf("\"", ssidValueStart);

  if (ssidValueStart < 0 || ssidValueEnd < 0)
  {
    Serial.println("[CONFIG] ✗ JSON inválido - valor de ssid malformado");
    return false;
  }

  config.wifiSsid = json.substring(ssidValueStart, ssidValueEnd);

  // Parse PASSWORD
  int passStart = json.indexOf("\"pass\"");
  if (passStart < 0)
  {
    Serial.println("[CONFIG] ✗ JSON inválido - campo 'pass' não encontrado");
    return false;
  }

  int passValueStart = json.indexOf("\"", passStart + 7) + 1;
  int passValueEnd = json.indexOf("\"", passValueStart);

  if (passValueStart < 0 || passValueEnd < 0)
  {
    Serial.println("[CONFIG] ✗ JSON inválido - valor de pass malformado");
    return false;
  }

  config.wifiPass = json.substring(passValueStart, passValueEnd);

  // Parse MQTT Host (opcional - pode não existir em configs antigas)
  int hostStart = json.indexOf("\"host\"");
  if (hostStart >= 0)
  {
    int hostValueStart = json.indexOf("\"", hostStart + 7) + 1;
    int hostValueEnd = json.indexOf("\"", hostValueStart);
    if (hostValueStart >= 0 && hostValueEnd >= 0)
    {
      config.mqttHost = json.substring(hostValueStart, hostValueEnd);
    }
  }

  // Parse MQTT Port (opcional)
  int portStart = json.indexOf("\"port\"");
  if (portStart >= 0)
  {
    int portValueStart = json.indexOf(":", portStart) + 1;
    int portValueEnd = json.indexOf(",", portValueStart);
    if (portValueEnd < 0)
      portValueEnd = json.indexOf("}", portValueStart);
    if (portValueStart >= 0 && portValueEnd >= 0)
    {
      config.mqttPort = json.substring(portValueStart, portValueEnd).toInt();
    }
  }

  // Parse MQTT User (opcional)
  int userStart = json.indexOf("\"user\"");
  if (userStart >= 0)
  {
    int userValueStart = json.indexOf("\"", userStart + 7) + 1;
    int userValueEnd = json.indexOf("\"", userValueStart);
    if (userValueStart >= 0 && userValueEnd >= 0)
    {
      config.mqttUser = json.substring(userValueStart, userValueEnd);
    }
  }

  // Parse MQTT Password (opcional)
  int mqttPassStart = json.indexOf("\"mqttPass\"");
  if (mqttPassStart >= 0)
  {
    int mqttPassValueStart = json.indexOf("\"", mqttPassStart + 11) + 1;
    int mqttPassValueEnd = json.indexOf("\"", mqttPassValueStart);
    if (mqttPassValueStart >= 0 && mqttPassValueEnd >= 0)
    {
      config.mqttPass = json.substring(mqttPassValueStart, mqttPassValueEnd);
    }
  }

  Serial.println("\n[CONFIG] ✓ Configuração carregada:");
  Serial.printf("[CONFIG]   SSID: '%s' (len=%d)\n", config.wifiSsid.c_str(), config.wifiSsid.length());
  Serial.printf("[CONFIG]   PASS: '%s' (len=%d)\n", config.wifiPass.c_str(), config.wifiPass.length());
  if (config.mqttHost.length() > 0)
  {
    Serial.printf("[CONFIG]   MQTT Host: '%s'\n", config.mqttHost.c_str());
    Serial.printf("[CONFIG]   MQTT Port: %d\n", config.mqttPort);
    Serial.printf("[CONFIG]   MQTT User: '%s'\n", config.mqttUser.c_str());
  }
  Serial.println();

  return true;
}

bool saveConfig(const DeviceConfig &config)
{
  if (!LittleFS.begin())
    return false;

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f)
    return false;

  f.printf(
      "{\"ssid\":\"%s\",\"pass\":\"%s\",\"host\":\"%s\",\"port\":%d,\"user\":\"%s\",\"mqttPass\":\"%s\"}",
      config.wifiSsid.c_str(),
      config.wifiPass.c_str(),
      config.mqttHost.c_str(),
      config.mqttPort,
      config.mqttUser.c_str(),
      config.mqttPass.c_str());
  f.close();
  return true;
}

bool hasValidConfig()
{
  DeviceConfig cfg;
  return loadConfig(cfg) && cfg.wifiSsid.length() > 0 && cfg.mqttHost.length() > 0;
}
