#include "setup_manager.h"

#ifdef ARDUINO
// Apenas incluir headers do Arduino durante compilação real
#include "config.h"
#include "config/config_manager.h"
#include "wifi/wifi_manager.h"
#include "mqtt/mqtt_manager.h"
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
    config.lastStateChange = millis();
    config.mqttPort = 1883; // Default
}

// ========== Setup Phase ==========

void SetupManager::setSetupState(SetupState newState)
{
    if (config.currentSetupState != newState)
    {
        config.currentSetupState = newState;
        config.lastStateChange = millis();

        Serial.printf("[SETUP] Estado: %s\n", setupStateToString(newState).c_str());
    }
}

SetupState SetupManager::getSetupState() const
{
    return config.currentSetupState;
}

bool SetupManager::testWiFiConnection(const String &ssid, const String &pass)
{
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
}

void SetupManager::saveWiFiConfig(const String &ssid, const String &pass)
{
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
}

bool SetupManager::testMQTTConnection(const String &host, uint16_t port,
                                      const String &user, const String &pass)
{
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
}

void SetupManager::saveMQTTConfig(const String &host, uint16_t port,
                                  const String &user, const String &pass)
{
    config.mqttHost = host;
    config.mqttPort = port;
    config.mqttUser = user;
    config.mqttPass = pass;

    // TODO: Salvar MQTT em config.json (estender DeviceConfig)
    Serial.printf("[SETUP] MQTT config: %s:%d (user=%s)\n",
                  host.c_str(), port, user.c_str());
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
        config.lastStateChange = millis();

        Serial.printf("[OP] Estado: %s\n", operationalStateToString(newState).c_str());
    }
}

OperationalState SetupManager::getOperationalState() const
{
    return config.currentOpState;
}

bool SetupManager::recordWiFiFailure()
{
    config.wifiAttempts++;
    config.wifiLastAttemptTime = millis();

    Serial.printf("[OP] WiFi falho (tentativa %d/3)\n", config.wifiAttempts);

    if (config.wifiAttempts >= 3)
    {
        Serial.println("[OP] WiFi falhou 3x - Retornando ao AP para reconfigurr");
        setOperationalState(OPERATIONAL_ROLLBACK);
        return true; // Deve voltar ao AP
    }

    setOperationalState(OPERATIONAL_WIFI_ERROR);
    return false; // Pode continuar tentando
}

bool SetupManager::recordMQTTFailure()
{
    config.mqttAttempts++;
    config.mqttLastAttemptTime = millis();

    Serial.printf("[OP] MQTT falho (tentativa %d/3)\n", config.mqttAttempts);

    if (config.mqttAttempts >= 3)
    {
        Serial.println("[OP] MQTT falhou 3x - Retornando ao AP para reconfigurar");
        setOperationalState(OPERATIONAL_ROLLBACK);
        return true; // Deve voltar ao AP
    }

    setOperationalState(OPERATIONAL_MQTT_ERROR);
    return false; // Pode continuar tentando
}

void SetupManager::recordWiFiSuccess()
{
    config.wifiAttempts = 0;
    config.wifiConfigured = true;
    config.lastError = "";

    Serial.println("[OP] WiFi reconectado com sucesso");
}

void SetupManager::recordMQTTSuccess()
{
    config.mqttAttempts = 0;
    config.mqttConfigured = true;
    config.lastError = "";

    Serial.println("[OP] MQTT reconectado com sucesso");
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
    // TODO: Implementar carregamento de estado do LittleFS
    Serial.println("[SETUP] Estado carregado do LittleFS");
    return true;
}

bool SetupManager::saveSetupState()
{
    // TODO: Implementar salvamento de estado no LittleFS
    Serial.println("[SETUP] Estado salvo no LittleFS");
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

    Serial.println("[SETUP] Estado resetado");
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
    return millis() - config.lastStateChange;
}

// ========== Private Implementations ==========

bool SetupManager::_testWiFiReal(const String &ssid, const String &pass)
{
    // Usa a função do WiFiManager para testar
    DeviceConfig testConfig;
    testConfig.wifiSsid = ssid;
    testConfig.wifiPass = pass;

    // Timeout de 10 segundos para teste
    unsigned long start = millis();
    const unsigned long timeout = 10000;

    if (!connectWifi(testConfig))
    {
        return false;
    }

    return true;
}

bool SetupManager::_testMQTTReal(const String &host, uint16_t port,
                                 const String &user, const String &pass)
{
    // Simula teste de MQTT (será melhorado depois)
    Serial.printf("[MQTT] Simulando teste: %s:%d\n", host.c_str(), port);

    // Por enquanto apenas valida:
    // - Host não vazio
    // - Port válido (1-65535)
    // - User não vazio
    // - Pass não vazio

    if (host.length() == 0)
    {
        config.lastErrorDetail = "Host não pode estar vazio";
        return false;
    }

    if (port < 1 || port > 65535)
    {
        config.lastErrorDetail = "Porta deve estar entre 1-65535";
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

    // TODO: Fazer teste real de conexão MQTT
    return true;
}

// ========== Funções Globais ==========

void initSetupManager()
{
    g_setupManager = SetupManager();
    g_setupManager.loadSetupState();

    Serial.println("[SETUP] SetupManager inicializado");
}

void updateSetupManager()
{
    // TODO: Lógica periódica de atualização se necessário
}
