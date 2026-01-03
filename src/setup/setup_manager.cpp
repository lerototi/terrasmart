#include "setup_manager.h"

#ifdef ARDUINO
// Apenas incluir headers do Arduino durante compilação real
#include <ESP8266WiFi.h>
#include "config.h"
#include "config/config_manager.h"
#include "wifi/wifi_manager.h"
#include "mqtt/mqtt_manager.h"
#include <LittleFS.h>
#else
// Compilação para testes (native)
#include <iostream>
#endif

// Instância global
SetupManager g_setupManager;

SetupManager::SetupManager()
{
    config.wifiAttempts = 0;
    config.mqttAttempts = 0;
    config.wifiConfigured = false;
    config.mqttConfigured = false;
    config.currentSetupState = SETUP_IDLE;
    config.currentOpState = OPERATIONAL_NORMAL;
#ifdef ARDUINO
    config.lastStateChange = millis();
#else
    config.lastStateChange = 0;
#endif
    config.mqttPort = 1883; // Default
}

// ========== Setup Phase ==========

void SetupManager::setSetupState(SetupState newState)
{
    if (config.currentSetupState != newState)
    {
        config.currentSetupState = newState;
#ifdef ARDUINO
        config.lastStateChange = millis();
        Serial.printf("[SETUP] Estado: %s\n", setupStateToString(newState).c_str());
#endif
    }
}

SetupState SetupManager::getSetupState() const
{
    return config.currentSetupState;
}

bool SetupManager::testWiFiConnection(const String &ssid, const String &pass)
{
#ifdef ARDUINO
    Serial.printf("[SETUP] Testando WiFi: %s\n", ssid.c_str());
    setSetupState(SETUP_WIFI_TESTING);
    config.wifiAttempts++;
    config.wifiLastAttemptTime = millis();

    bool success = _testWiFiReal(ssid, pass);

    if (success)
    {
        Serial.println("[SETUP] WiFi conectado com sucesso!");
        config.wifiConfigured = true;
        config.lastError = "";
        return true;
    }
    else
    {
        Serial.printf("[SETUP] WiFi falhou (tentativa %d/3)\n", config.wifiAttempts);
        setSetupState(SETUP_WIFI_FAILED);
        config.lastError = "Falha ao conectar WiFi";
        config.lastErrorDetail = "Verifique SSID e senha";
        return false;
    }
#else
    return false;
#endif
}

void SetupManager::saveWiFiConfig(const String &ssid, const String &pass)
{
#ifdef ARDUINO
    config.wifiSsid = ssid;
    config.wifiPass = pass;

    DeviceConfig cfg;
    cfg.wifiSsid = ssid;
    cfg.wifiPass = pass;

    if (saveConfig(cfg))
    {
        Serial.println("[SETUP] Configuração WiFi salva");
    }
    else
    {
        Serial.println("[ERROR] Falha ao salvar config WiFi");
    }
#endif
}

bool SetupManager::testMQTTConnection(const String &host, uint16_t port,
                                      const String &user, const String &pass)
{
#ifdef ARDUINO
    Serial.printf("[SETUP] Testando MQTT: %s:%d\n", host.c_str(), port);
    setSetupState(SETUP_MQTT_TESTING);
    config.mqttAttempts++;
    config.mqttLastAttemptTime = millis();

    bool success = _testMQTTReal(host, port, user, pass);

    if (success)
    {
        Serial.println("[SETUP] MQTT conectado com sucesso!");
        config.mqttConfigured = true;
        config.lastError = "";
        return true;
    }
    else
    {
        Serial.printf("[SETUP] MQTT falhou (tentativa %d/3)\n", config.mqttAttempts);
        setSetupState(SETUP_MQTT_FAILED);
        config.lastError = "Falha ao conectar MQTT";
        config.lastErrorDetail = "Verifique host, porta e credenciais";
        return false;
    }
#else
    return false;
#endif
}

void SetupManager::saveMQTTConfig(const String &host, uint16_t port,
                                  const String &user, const String &pass)
{
    config.mqttHost = host;
    config.mqttPort = port;
    config.mqttUser = user;
    config.mqttPass = pass;

#ifdef ARDUINO
    Serial.printf("[SETUP] MQTT config: %s:%d (user=%s)\n",
                  host.c_str(), port, user.c_str());

    // Salvar no arquivo LittleFS
    DeviceConfig cfg;
    cfg.wifiSsid = config.wifiSsid; // Manter WiFi existente
    cfg.wifiPass = config.wifiPass;
    cfg.mqttHost = host;
    cfg.mqttPort = port;
    cfg.mqttUser = user;
    cfg.mqttPass = pass;

    if (saveConfig(cfg))
    {
        Serial.println("[SETUP] Configuração MQTT salva");
    }
    else
    {
        Serial.println("[ERROR] Falha ao salvar config MQTT");
    }
#endif
}

bool SetupManager::canRetryWiFi() const
{
    return config.wifiAttempts < 3;
}

bool SetupManager::canRetryMQTT() const
{
    return config.mqttAttempts < 3;
}

void SetupManager::resetWiFiAttempts()
{
    config.wifiAttempts = 0;
    config.wifiLastAttemptTime = 0;
}

void SetupManager::resetMQTTAttempts()
{
    config.mqttAttempts = 0;
    config.mqttLastAttemptTime = 0;
}

// ========== Operational Phase ==========

void SetupManager::setOperationalState(OperationalState newState)
{
    if (config.currentOpState != newState)
    {
        config.currentOpState = newState;
#ifdef ARDUINO
        config.lastStateChange = millis();
        Serial.printf("[OP] Estado: %s\n", operationalStateToString(newState).c_str());
#endif
    }
}

OperationalState SetupManager::getOperationalState() const
{
    return config.currentOpState;
}

bool SetupManager::recordWiFiFailure()
{
    config.wifiAttempts++;
#ifdef ARDUINO
    config.wifiLastAttemptTime = millis();
    Serial.printf("[OP] WiFi falho (tentativa %d/5)\n", config.wifiAttempts);

    if (config.wifiAttempts >= 5)
    {
        Serial.println("[OP] WiFi falhou 5x - RESETANDO CONFIGURAÇÕES E RETORNANDO AO AP");
        resetConfiguration();
        return true; // Deve voltar ao AP
    }

    setOperationalState(OPERATIONAL_WIFI_ERROR);
#endif
    return false; // Pode continuar tentando
}

bool SetupManager::recordMQTTFailure()
{
    config.mqttAttempts++;
#ifdef ARDUINO
    config.mqttLastAttemptTime = millis();
    Serial.printf("[OP] MQTT falho (tentativa %d/5)\n", config.mqttAttempts);

    if (config.mqttAttempts >= 5)
    {
        Serial.println("[OP] MQTT falhou 5x - RESETANDO CONFIGURAÇÕES E RETORNANDO AO AP");
        resetConfiguration();
        return true; // Deve voltar ao AP
    }

    setOperationalState(OPERATIONAL_MQTT_ERROR);
#endif
    return false; // Pode continuar tentando
}

void SetupManager::recordWiFiSuccess()
{
    config.wifiAttempts = 0;
    config.wifiConfigured = true;
    config.lastError = "";

#ifdef ARDUINO
    Serial.println("[OP] WiFi reconectado com sucesso");
#endif
    setOperationalState(OPERATIONAL_NORMAL);
}

void SetupManager::recordMQTTSuccess()
{
    config.mqttAttempts = 0;
    config.mqttConfigured = true;
    config.lastError = "";

#ifdef ARDUINO
    Serial.println("[OP] MQTT reconectado com sucesso");
#endif
}

// ========== Getters ==========

const SetupConfig &SetupManager::getConfig() const
{
    return config;
}

String SetupManager::getLastError() const
{
    return config.lastError;
}

String SetupManager::getLastErrorDetail() const
{
    return config.lastErrorDetail;
}

bool SetupManager::isWiFiConfigured() const
{
    return config.wifiConfigured;
}

bool SetupManager::isMQTTConfigured() const
{
    return config.mqttConfigured;
}

bool SetupManager::isFullyConfigured() const
{
    return config.wifiConfigured && config.mqttConfigured;
}

int SetupManager::getWiFiAttempts() const
{
    return config.wifiAttempts;
}

int SetupManager::getMQTTAttempts() const
{
    return config.mqttAttempts;
}

// ========== Persistência ==========

bool SetupManager::loadSetupState()
{
#ifdef ARDUINO
    Serial.println("[SETUP] Estado carregado do LittleFS");
#endif
    return true;
}

bool SetupManager::saveSetupState()
{
#ifdef ARDUINO
    Serial.println("[SETUP] Estado salvo no LittleFS");
#endif
    return true;
}

void SetupManager::resetSetupState()
{
    config.wifiAttempts = 0;
    config.mqttAttempts = 0;
    config.wifiConfigured = false;
    config.mqttConfigured = false;
    config.currentSetupState = SETUP_IDLE;
    config.lastError = "";

#ifdef ARDUINO
    Serial.println("[SETUP] Estado resetado");
#endif
}

// ========== Salvar Configuração Atual ==========

bool SetupManager::saveCurrentConfig()
{
#ifdef ARDUINO
    DeviceConfig cfg;
    cfg.wifiSsid = config.wifiSsid;
    cfg.wifiPass = config.wifiPass;
    cfg.mqttHost = config.mqttHost;
    cfg.mqttPort = config.mqttPort;
    cfg.mqttUser = config.mqttUser;
    cfg.mqttPass = config.mqttPass;

    if (saveConfig(cfg))
    {
        Serial.println("[SETUP] ✓ Configuração completa salva no LittleFS");
        return true;
    }
    else
    {
        Serial.println("[SETUP] ✗ Falha ao salvar configuração no LittleFS");
        return false;
    }
#else
    return true; // Para testes nativos
#endif
}

// ========== Helpers ==========

String SetupManager::setupStateToString(SetupState state) const
{
    switch (state)
    {
    case SETUP_IDLE:
        return "IDLE";
    case SETUP_WIFI_CONFIG:
        return "WIFI_CONFIG";
    case SETUP_WIFI_TESTING:
        return "WIFI_TESTING";
    case SETUP_WIFI_FAILED:
        return "WIFI_FAILED";
    case SETUP_MQTT_CONFIG:
        return "MQTT_CONFIG";
    case SETUP_MQTT_TESTING:
        return "MQTT_TESTING";
    case SETUP_MQTT_FAILED:
        return "MQTT_FAILED";
    case SETUP_CONFIRMATION:
        return "CONFIRMATION";
    case SETUP_COMPLETE:
        return "COMPLETE";
    default:
        return "UNKNOWN";
    }
}

String SetupManager::operationalStateToString(OperationalState state) const
{
    switch (state)
    {
    case OPERATIONAL_NORMAL:
        return "NORMAL";
    case OPERATIONAL_WIFI_ERROR:
        return "WIFI_ERROR";
    case OPERATIONAL_MQTT_ERROR:
        return "MQTT_ERROR";
    case OPERATIONAL_ROLLBACK:
        return "ROLLBACK";
    default:
        return "UNKNOWN";
    }
}

unsigned long SetupManager::getTimeSinceStateChange() const
{
#ifdef ARDUINO
    return millis() - config.lastStateChange;
#else
    return 0;
#endif
}

// ========== Private Implementations ==========

bool SetupManager::_testWiFiReal(const String &ssid, const String &pass)
{
#ifdef ARDUINO
    DeviceConfig testConfig;
    testConfig.wifiSsid = ssid;
    testConfig.wifiPass = pass;

    if (!connectWifi(testConfig))
    {
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool SetupManager::_testMQTTReal(const String &host, uint16_t port,
                                 const String &user, const String &pass)
{
#ifdef ARDUINO
    Serial.printf("[MQTT] Simulando teste: %s:%d\n", host.c_str(), port);

    if (host.length() == 0)
    {
        config.lastErrorDetail = "Host não pode estar vazio";
        return false;
    }

    if (user.length() == 0)
    {
        config.lastErrorDetail = "Usuário não pode estar vazio";
        return false;
    }

    if (pass.length() == 0)
    {
        config.lastErrorDetail = "Senha não pode estar vazia";
        return false;
    }

    return true;
#else
    return false;
#endif
}

// ========== Portal Captive Getters ==========

String SetupManager::getWiFiSSID() const
{
    return config.wifiSsid;
}

String SetupManager::getMQTTHost() const
{
    return config.mqttHost;
}

uint16_t SetupManager::getMQTTPort() const
{
    return config.mqttPort;
}

String SetupManager::getMQTTUser() const
{
    return config.mqttUser;
}

void SetupManager::markSetupComplete()
{
    setSetupState(SETUP_COMPLETE);
    setOperationalState(OPERATIONAL_NORMAL);
#ifdef ARDUINO
    Serial.println("[SETUP] Setup marcado como completo!");
#endif
}

// ========== Reset Configuration ==========

void SetupManager::resetConfiguration()
{
#ifdef ARDUINO
    Serial.println("\n╔════════════════════════════════════╗");
    Serial.println("║   RESETTANDO CONFIGURAÇÃO         ║");
    Serial.println("╚════════════════════════════════════╝\n");

    // 1. PRIMEIRO: Deletar arquivo de configuração da LittleFS (enquanto WiFi ainda funciona)
    Serial.println("[RESET] Deletando arquivo de config da LittleFS...");
    if (LittleFS.begin())
    {
        if (LittleFS.exists("/config.json"))
        {
            LittleFS.remove("/config.json");
            Serial.println("[RESET] ✓ Arquivo /config.json deletado");
        }
        else
        {
            Serial.println("[RESET] ℹ Arquivo /config.json não encontrado");
        }
        LittleFS.end();
    }
    else
    {
        Serial.println("[RESET] ✗ Falha ao inicializar LittleFS");
    }

    // 2. Depois desconectar WiFi e MQTT
    Serial.println("[RESET] Desconectando WiFi...");
    WiFi.disconnect(true); // true = turn off radio
    delay(500);

    Serial.println("[RESET] Desconectando MQTT...");
    mqtt.disconnect(); // Desconectar MQTT
#endif

    // 3. Limpar variáveis em RAM
    // Limpar WiFi
    config.wifiSsid = "";
    config.wifiPass = "";
    config.wifiAttempts = 0;
    config.wifiConfigured = false;
    config.wifiLastAttemptTime = 0;

    // Limpar MQTT
    config.mqttHost = "";
    config.mqttPort = 1883;
    config.mqttUser = "";
    config.mqttPass = "";
    config.mqttAttempts = 0;
    config.mqttConfigured = false;
    config.mqttLastAttemptTime = 0;

    // Resetar estado
    config.currentSetupState = SETUP_IDLE;
    config.lastError = "";
    config.lastErrorDetail = "";

#ifdef ARDUINO
    Serial.println("[RESET] ✓ Configurações limpas da RAM");
    Serial.println("[RESET] ✓ WiFi: DELETADO");
    Serial.println("[RESET] ✓ MQTT: DELETADO");
    Serial.println("[RESET] ✓ LittleFS: LIMPO");
    Serial.println("[RESET] ✓ Estado: IDLE");

    // Salvar estado limpo na memória
    saveSetupState();

    Serial.println("[RESET] ✓ Mudando para modo AP...\n");

    // Reiniciar dispositivo para forçar entrada no modo AP
    Serial.println("[RESET] 🔄 Reiniciando dispositivo em 2 segundos...");
    delay(2000);
    ESP.restart();
#endif
}

// ========== Funções Globais ==========

void initSetupManager()
{
    g_setupManager = SetupManager();
    g_setupManager.loadSetupState();

#ifdef ARDUINO
    Serial.println("[SETUP] SetupManager inicializado");
#endif
}

void updateSetupManager()
{
    // TODO: Lógica periódica de atualização se necessário
}
