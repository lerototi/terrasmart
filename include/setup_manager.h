#pragma once

// Compatibilidade com ambos os ambientes: ESP8266 e testes nativos
#ifdef ARDUINO
// Compilando para ESP8266
#include <Arduino.h>
#else
// Compilando para testes nativos (PC)
#include <cstdint>
#include <string>
using String = std::string;
using uint16_t = unsigned short;
// Simular alguns defines do Arduino
#ifndef HIGH
#define HIGH 1
#define LOW 0
#endif
#endif

// Estados de Setup (AP Mode)
enum SetupState
{
    SETUP_IDLE,         // Aguardando ação
    SETUP_WIFI_CONFIG,  // Usuário configurando WiFi
    SETUP_WIFI_TESTING, // Testando conexão WiFi
    SETUP_WIFI_FAILED,  // WiFi falhou
    SETUP_MQTT_CONFIG,  // Usuário configurando MQTT
    SETUP_MQTT_TESTING, // Testando conexão MQTT
    SETUP_MQTT_FAILED,  // MQTT falhou
    SETUP_CONFIRMATION, // Mostrando resumo
    SETUP_COMPLETE      // Setup concluído
};

// Estados de Operação (Modo Operacional)
enum OperationalState
{
    OPERATIONAL_NORMAL,     // WiFi + MQTT OK
    OPERATIONAL_WIFI_ERROR, // WiFi tentando reconectar
    OPERATIONAL_MQTT_ERROR, // MQTT tentando reconectar
    OPERATIONAL_ROLLBACK    // Voltando ao AP por erro persistente
};

// Estrutura com configurações de setup
struct SetupConfig
{
    // WiFi
    String wifiSsid;
    String wifiPass;
    int wifiAttempts;    // 0-3 (contador de tentativas)
    bool wifiConfigured; // Passou no teste
    unsigned long wifiLastAttemptTime;

    // MQTT
    String mqttHost;
    uint16_t mqttPort;
    String mqttUser;
    String mqttPass;
    int mqttAttempts;    // 0-3 (contador de tentativas)
    bool mqttConfigured; // Passou no teste
    unsigned long mqttLastAttemptTime;

    // Status geral
    SetupState currentSetupState;
    OperationalState currentOpState;
    unsigned long lastStateChange;
    String lastError;
    String lastErrorDetail;
};

// Interface do Setup Manager
class SetupManager
{
public:
    SetupManager();

    // ========== Setup Phase (AP Mode) ==========

    /// Muda para um novo estado de setup
    void setSetupState(SetupState newState);
    SetupState getSetupState() const;

    /// Testa conexão WiFi
    /// Incrementa wifiAttempts, atualiza lastError
    bool testWiFiConnection(const String &ssid, const String &pass);

    /// Salva WiFi após sucesso no teste
    void saveWiFiConfig(const String &ssid, const String &pass);

    /// Testa conexão MQTT
    /// Incrementa mqttAttempts, atualiza lastError
    bool testMQTTConnection(const String &host, uint16_t port,
                            const String &user, const String &pass);

    /// Salva MQTT após sucesso no teste
    void saveMQTTConfig(const String &host, uint16_t port,
                        const String &user, const String &pass);

    /// Verifica se WiFi não excedeu 3 tentativas
    bool canRetryWiFi() const;

    /// Verifica se MQTT não excedeu 3 tentativas
    bool canRetryMQTT() const;

    /// Reseta contadores de tentativas para novo step
    void resetWiFiAttempts();
    void resetMQTTAttempts();
    void resetSetupState();

    // ========== Operational Phase ==========

    /// Muda para estado operacional
    void setOperationalState(OperationalState newState);
    OperationalState getOperationalState() const;

    /// Registra falha de WiFi durante operação
    /// Retorna true se excedeu 3 tentativas (deve voltar AP)
    bool recordWiFiFailure();

    /// Registra falha de MQTT durante operação
    /// Retorna true se excedeu 3 tentativas
    bool recordMQTTFailure();

    /// Reseta falhas ao reconectar com sucesso
    void recordWiFiSuccess();
    void recordMQTTSuccess();

    // ========== Getters ==========

    const SetupConfig &getConfig() const;
    String getLastError() const;
    String getLastErrorDetail() const;

    bool isWiFiConfigured() const;
    bool isMQTTConfigured() const;
    bool isFullyConfigured() const;

    int getWiFiAttempts() const;
    int getMQTTAttempts() const;

    // Getters para portal captivo
    String getWiFiSSID() const;
    String getMQTTHost() const;
    uint16_t getMQTTPort() const;
    String getMQTTUser() const;
    void markSetupComplete();

    // ========== Persistência ==========

    /// Carrega estado de setup do LittleFS
    bool loadSetupState();

    /// Salva estado de setup no LittleFS
    bool saveSetupState();

    /// Salva configuração atual (WiFi + MQTT) no LittleFS
    bool saveCurrentConfig();

    /// Reseta toda configuração (WiFi, MQTT, estado) - usado pelo botão
    void resetConfiguration();

    // ========== Helpers ==========

    /// Retorna string do estado de setup (debug)
    String setupStateToString(SetupState state) const;

    /// Retorna string do estado operacional (debug)
    String operationalStateToString(OperationalState state) const;

    /// Retorna tempo desde última mudança de estado (ms)
    unsigned long getTimeSinceStateChange() const;

private:
    SetupConfig config;

    /// Testa conexão WiFi real (implementação interna)
    bool _testWiFiReal(const String &ssid, const String &pass);

    /// Testa conexão MQTT real (implementação interna)
    bool _testMQTTReal(const String &host, uint16_t port,
                       const String &user, const String &pass);
};

// Funções globais para fácil acesso
extern SetupManager g_setupManager;

void initSetupManager();
void updateSetupManager();
