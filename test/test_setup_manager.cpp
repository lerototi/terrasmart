#include <gtest/gtest.h>
#include "setup_manager.h"

// Mock de millis() para testes
unsigned long mock_millis = 0;

#ifndef ARDUINO
// Para compilação nativa, definir millis()
unsigned long millis()
{
    return mock_millis;
}
#endif

// ============================================================
// SetupManager Tests
// ============================================================

class SetupManagerTest : public ::testing::Test
{
protected:
    SetupManager manager;

    void SetUp() override
    {
        manager.resetSetupState();
    }
};

// ========== State Transition Tests ==========

TEST_F(SetupManagerTest, InitialStateIsIdle)
{
    EXPECT_EQ(manager.getSetupState(), SETUP_IDLE);
}

TEST_F(SetupManagerTest, CanTransitionToWiFiConfig)
{
    manager.setSetupState(SETUP_WIFI_CONFIG);
    EXPECT_EQ(manager.getSetupState(), SETUP_WIFI_CONFIG);
}

TEST_F(SetupManagerTest, RecordsStateChangeTime)
{
    unsigned long beforeTime = millis();
    manager.setSetupState(SETUP_WIFI_CONFIG);
    unsigned long afterTime = millis();

    unsigned long timeSinceChange = manager.getTimeSinceStateChange();
    // Deve ser aproximadamente 0 (ou muito pouco tempo passou)
    EXPECT_LT(timeSinceChange, 100); // Tolerância de 100ms
}

// ========== WiFi Configuration Tests ==========

TEST_F(SetupManagerTest, WiFiAttemptsStartAtZero)
{
    EXPECT_EQ(manager.getWiFiAttempts(), 0);
    EXPECT_TRUE(manager.canRetryWiFi());
}

TEST_F(SetupManagerTest, WiFiAttemptIncrements)
{
    // Simular uma tentativa (sem realmente conectar)
    manager.setSetupState(SETUP_WIFI_TESTING);
    manager.testWiFiConnection("test-ssid", "test-pass");

    // Attempts deve ter incrementado
    EXPECT_GT(manager.getWiFiAttempts(), 0);
}

TEST_F(SetupManagerTest, CanRetryWiFiUntilThree)
{
    EXPECT_TRUE(manager.canRetryWiFi()); // 0/3

    // Simular 3 tentativas
    manager.resetWiFiAttempts();

    // Force attempts to 2
    for (int i = 0; i < 2; i++)
    {
        EXPECT_TRUE(manager.canRetryWiFi());
        manager.testWiFiConnection("ssid", "pass"); // Fails, increments
    }

    // Agora está em 2/3, ainda pode tentar
    EXPECT_TRUE(manager.canRetryWiFi());
}

TEST_F(SetupManagerTest, CannotRetryWiFiAfterThreeAttempts)
{
    // Força 3 tentativas
    manager.setSetupState(SETUP_WIFI_TESTING);

    // Incrementa manualmente para teste
    // (Em produção, seria via testWiFiConnection que falha)
    for (int i = 0; i < 3; i++)
    {
        manager.testWiFiConnection("ssid", "pass");
    }

    EXPECT_FALSE(manager.canRetryWiFi());
    EXPECT_EQ(manager.getWiFiAttempts(), 3);
}

TEST_F(SetupManagerTest, ResetWiFiAttemptsWorks)
{
    // Incrementa attempts
    manager.setSetupState(SETUP_WIFI_TESTING);
    manager.testWiFiConnection("ssid", "pass");

    EXPECT_GT(manager.getWiFiAttempts(), 0);

    // Reset
    manager.resetWiFiAttempts();
    EXPECT_EQ(manager.getWiFiAttempts(), 0);
    EXPECT_TRUE(manager.canRetryWiFi());
}

TEST_F(SetupManagerTest, SaveWiFiConfigSetsValues)
{
    String testSsid = "my-network";
    String testPass = "my-password";

    manager.saveWiFiConfig(testSsid, testPass);

    EXPECT_EQ(manager.getConfig().wifiSsid, testSsid);
    EXPECT_EQ(manager.getConfig().wifiPass, testPass);
}

// ========== MQTT Configuration Tests ==========

TEST_F(SetupManagerTest, MQTTAttemptsStartAtZero)
{
    EXPECT_EQ(manager.getMQTTAttempts(), 0);
    EXPECT_TRUE(manager.canRetryMQTT());
}

TEST_F(SetupManagerTest, CanRetryMQTTUntilThree)
{
    EXPECT_TRUE(manager.canRetryMQTT());

    // Simular algumas tentativas
    manager.resetMQTTAttempts();
    EXPECT_TRUE(manager.canRetryMQTT());
}

TEST_F(SetupManagerTest, ResetMQTTAttemptsWorks)
{
    manager.setSetupState(SETUP_MQTT_TESTING);
    manager.testMQTTConnection("localhost", 1883, "user", "pass");

    EXPECT_GT(manager.getMQTTAttempts(), 0);

    manager.resetMQTTAttempts();
    EXPECT_EQ(manager.getMQTTAttempts(), 0);
}

TEST_F(SetupManagerTest, SaveMQTTConfigSetsValues)
{
    String testHost = "192.168.1.1";
    uint16_t testPort = 1883;
    String testUser = "mqtt_user";
    String testPass = "mqtt_pass";

    manager.saveMQTTConfig(testHost, testPort, testUser, testPass);

    EXPECT_EQ(manager.getConfig().mqttHost, testHost);
    EXPECT_EQ(manager.getConfig().mqttPort, testPort);
    EXPECT_EQ(manager.getConfig().mqttUser, testUser);
    EXPECT_EQ(manager.getConfig().mqttPass, testPass);
}

// ========== Configuration Status Tests ==========

TEST_F(SetupManagerTest, IsFullyConfiguredFalseInitially)
{
    EXPECT_FALSE(manager.isWiFiConfigured());
    EXPECT_FALSE(manager.isMQTTConfigured());
    EXPECT_FALSE(manager.isFullyConfigured());
}

TEST_F(SetupManagerTest, CanDetectWiFiConfigured)
{
    manager.saveWiFiConfig("ssid", "pass");
    manager.recordWiFiSuccess();

    EXPECT_TRUE(manager.isWiFiConfigured());
}

TEST_F(SetupManagerTest, CanDetectMQTTConfigured)
{
    manager.saveMQTTConfig("host", 1883, "user", "pass");
    manager.recordMQTTSuccess();

    EXPECT_TRUE(manager.isMQTTConfigured());
}

TEST_F(SetupManagerTest, IsFullyConfiguredOnlyWhenBothOK)
{
    EXPECT_FALSE(manager.isFullyConfigured());

    manager.saveWiFiConfig("ssid", "pass");
    manager.recordWiFiSuccess();
    EXPECT_FALSE(manager.isFullyConfigured()); // WiFi OK, MQTT não

    manager.saveMQTTConfig("host", 1883, "user", "pass");
    manager.recordMQTTSuccess();
    EXPECT_TRUE(manager.isFullyConfigured()); // Ambos OK
}

// ========== Operational State Tests ==========

TEST_F(SetupManagerTest, OperationalStateStartsNormal)
{
    EXPECT_EQ(manager.getOperationalState(), OPERATIONAL_NORMAL);
}

TEST_F(SetupManagerTest, CanTransitionOperationalStates)
{
    manager.setOperationalState(OPERATIONAL_WIFI_ERROR);
    EXPECT_EQ(manager.getOperationalState(), OPERATIONAL_WIFI_ERROR);

    manager.setOperationalState(OPERATIONAL_NORMAL);
    EXPECT_EQ(manager.getOperationalState(), OPERATIONAL_NORMAL);
}

// ========== WiFi Failure Handling Tests ==========

TEST_F(SetupManagerTest, RecordWiFiFailureIncrementsCounter)
{
    manager.setOperationalState(OPERATIONAL_NORMAL);

    bool shouldRollback = manager.recordWiFiFailure();
    EXPECT_FALSE(shouldRollback); // Primeira falha
    EXPECT_EQ(manager.getWiFiAttempts(), 1);
}

TEST_F(SetupManagerTest, WiFiRollbackAfterThreeFailures)
{
    manager.setOperationalState(OPERATIONAL_NORMAL);

    // Primeira falha
    bool rollback1 = manager.recordWiFiFailure();
    EXPECT_FALSE(rollback1);

    // Segunda falha
    bool rollback2 = manager.recordWiFiFailure();
    EXPECT_FALSE(rollback2);

    // Terceira falha - deve ativar rollback
    bool rollback3 = manager.recordWiFiFailure();
    EXPECT_TRUE(rollback3);
    EXPECT_EQ(manager.getOperationalState(), OPERATIONAL_ROLLBACK);
}

TEST_F(SetupManagerTest, RecordWiFiSuccessResetsCounter)
{
    // Simular algumas falhas
    manager.recordWiFiFailure();
    manager.recordWiFiFailure();

    EXPECT_EQ(manager.getWiFiAttempts(), 2);

    // Sucesso deve resetar
    manager.recordWiFiSuccess();
    EXPECT_EQ(manager.getWiFiAttempts(), 0);
    EXPECT_TRUE(manager.isWiFiConfigured());
}

// ========== MQTT Failure Handling Tests ==========

TEST_F(SetupManagerTest, RecordMQTTFailureIncrementsCounter)
{
    manager.setOperationalState(OPERATIONAL_NORMAL);

    bool shouldRollback = manager.recordMQTTFailure();
    EXPECT_FALSE(shouldRollback); // Primeira falha
    EXPECT_EQ(manager.getMQTTAttempts(), 1);
}

TEST_F(SetupManagerTest, MQTTRollbackAfterThreeFailures)
{
    manager.setOperationalState(OPERATIONAL_NORMAL);

    // Três falhas
    manager.recordMQTTFailure();
    manager.recordMQTTFailure();
    bool rollback3 = manager.recordMQTTFailure();

    EXPECT_TRUE(rollback3);
    EXPECT_EQ(manager.getOperationalState(), OPERATIONAL_ROLLBACK);
}

TEST_F(SetupManagerTest, RecordMQTTSuccessResetsCounter)
{
    manager.recordMQTTFailure();
    manager.recordMQTTFailure();

    EXPECT_EQ(manager.getMQTTAttempts(), 2);

    manager.recordMQTTSuccess();
    EXPECT_EQ(manager.getMQTTAttempts(), 0);
    EXPECT_TRUE(manager.isMQTTConfigured());
}

// ========== Error Tracking Tests ==========

TEST_F(SetupManagerTest, LastErrorCanBeSet)
{
    manager.setSetupState(SETUP_WIFI_FAILED);
    // Simular erro (seria de testWiFiConnection)

    String errorMsg = "Unable to connect";
    // TODO: Quando testWiFiConnection é implementado propriamente
}

TEST_F(SetupManagerTest, ResetClearsAllState)
{
    manager.saveMQTTConfig("host", 1883, "user", "pass");
    manager.saveWiFiConfig("ssid", "pass");
    manager.recordWiFiFailure();
    manager.recordMQTTFailure();

    EXPECT_TRUE(manager.getConfig().wifiSsid.length() > 0);
    EXPECT_EQ(manager.getWiFiAttempts(), 1);

    manager.resetSetupState();

    EXPECT_EQ(manager.getWiFiAttempts(), 0);
    EXPECT_EQ(manager.getMQTTAttempts(), 0);
    EXPECT_FALSE(manager.isWiFiConfigured());
    EXPECT_FALSE(manager.isMQTTConfigured());
    EXPECT_EQ(manager.getSetupState(), SETUP_IDLE);
}

// ========== State String Conversion Tests ==========

TEST_F(SetupManagerTest, SetupStateToStringWorks)
{
    EXPECT_NE(manager.setupStateToString(SETUP_IDLE), "");
    EXPECT_EQ(manager.setupStateToString(SETUP_IDLE), "IDLE");
    EXPECT_EQ(manager.setupStateToString(SETUP_WIFI_CONFIG), "WIFI_CONFIG");
    EXPECT_EQ(manager.setupStateToString(SETUP_MQTT_CONFIG), "MQTT_CONFIG");
}

TEST_F(SetupManagerTest, OperationalStateToStringWorks)
{
    EXPECT_NE(manager.operationalStateToString(OPERATIONAL_NORMAL), "");
    EXPECT_EQ(manager.operationalStateToString(OPERATIONAL_NORMAL), "NORMAL");
    EXPECT_EQ(manager.operationalStateToString(OPERATIONAL_WIFI_ERROR), "WIFI_ERROR");
}

// ========== Integration-like Tests ==========

TEST_F(SetupManagerTest, CompleteSetupFlow)
{
    // Step 1: WiFi
    EXPECT_EQ(manager.getSetupState(), SETUP_IDLE);

    manager.setSetupState(SETUP_WIFI_CONFIG);
    manager.saveWiFiConfig("home-network", "password123");

    // Simular sucesso (testWiFiConnection falharia normalmente)
    manager.recordWiFiSuccess();
    EXPECT_TRUE(manager.isWiFiConfigured());

    // Step 2: MQTT
    manager.setSetupState(SETUP_MQTT_CONFIG);
    manager.saveMQTTConfig("192.168.1.100", 1883, "admin", "secret");

    // Simular sucesso
    manager.recordMQTTSuccess();
    EXPECT_TRUE(manager.isMQTTConfigured());

    // Step 3: Confirmação
    manager.setSetupState(SETUP_CONFIRMATION);
    EXPECT_TRUE(manager.isFullyConfigured());

    // Completo
    manager.setSetupState(SETUP_COMPLETE);
    EXPECT_EQ(manager.getSetupState(), SETUP_COMPLETE);
}

TEST_F(SetupManagerTest, FailureRecoveryFlow)
{
    // Modo operacional
    manager.setOperationalState(OPERATIONAL_NORMAL);

    // WiFi falha 3 vezes
    bool rollback = false;
    for (int i = 0; i < 3; i++)
    {
        rollback = manager.recordWiFiFailure();
    }

    EXPECT_TRUE(rollback);
    EXPECT_EQ(manager.getOperationalState(), OPERATIONAL_ROLLBACK);
}
