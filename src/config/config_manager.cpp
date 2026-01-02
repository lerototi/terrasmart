#include "config_manager.h"
#include <LittleFS.h>

static const char* CONFIG_FILE = "/config.json";

bool loadConfig(DeviceConfig &config) {
  Serial.println("\n[CONFIG] Carregando configuração de LittleFS...");
  
  if (!LittleFS.begin()) {
    Serial.println("[CONFIG] ✗ Falha ao inicializar LittleFS");
    return false;
  }
  
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.printf("[CONFIG] ✗ Arquivo %s não existe\n", CONFIG_FILE);
    return false;
  }

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) {
    Serial.println("[CONFIG] ✗ Falha ao abrir arquivo de config");
    return false;
  }

  String json = f.readString();
  f.close();

  Serial.printf("[CONFIG] JSON bruto: %s\n", json.c_str());

  // Parse SSID - pega entre primeira e segunda aspas após "ssid":"
  int ssidStart = json.indexOf("\"ssid\"");
  if (ssidStart < 0) {
    Serial.println("[CONFIG] ✗ JSON inválido - campo 'ssid' não encontrado");
    return false;
  }
  
  int ssidValueStart = json.indexOf("\"", ssidStart + 7) + 1;  // Pula "ssid":"
  int ssidValueEnd = json.indexOf("\"", ssidValueStart);       // Encontra a próxima aspas
  
  if (ssidValueStart < 0 || ssidValueEnd < 0) {
    Serial.println("[CONFIG] ✗ JSON inválido - valor de ssid malformado");
    return false;
  }
  
  config.wifiSsid = json.substring(ssidValueStart, ssidValueEnd);

  // Parse PASSWORD - pega entre primeira e segunda aspas após "pass":"
  int passStart = json.indexOf("\"pass\"");
  if (passStart < 0) {
    Serial.println("[CONFIG] ✗ JSON inválido - campo 'pass' não encontrado");
    return false;
  }
  
  int passValueStart = json.indexOf("\"", passStart + 7) + 1;  // Pula "pass":"
  int passValueEnd = json.indexOf("\"", passValueStart);       // Encontra a próxima aspas
  
  if (passValueStart < 0 || passValueEnd < 0) {
    Serial.println("[CONFIG] ✗ JSON inválido - valor de pass malformado");
    return false;
  }
  
  config.wifiPass = json.substring(passValueStart, passValueEnd);

  Serial.println("\n[CONFIG] ✓ Configuração carregada:");
  Serial.printf("[CONFIG]   SSID: '%s' (len=%d)\n", config.wifiSsid.c_str(), config.wifiSsid.length());
  Serial.printf("[CONFIG]   PASS: '%s' (len=%d)\n", config.wifiPass.c_str(), config.wifiPass.length());
  Serial.println();

  return true;
}


bool saveConfig(const DeviceConfig &config) {
  if (!LittleFS.begin()) return false;

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return false;

  f.printf(
    "{\"ssid\":\"%s\",\"pass\":\"%s\"}",
    config.wifiSsid.c_str(),
    config.wifiPass.c_str()
  );
  f.close();
  return true;
}

bool hasValidConfig() {
  DeviceConfig cfg;
  return loadConfig(cfg) && cfg.wifiSsid.length() > 0;
}
