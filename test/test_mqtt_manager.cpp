#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <ctime>

// Mock de MQTT Client
class MQTTClientMock {
private:
    static bool connected;
    static std::vector<std::pair<std::string, std::string>> messages;
    static int lastConnectionAttempt;
    static int reconnectDelay;

public:
    static void reset() {
        connected = false;
        messages.clear();
        lastConnectionAttempt = 0;
        reconnectDelay = 5000;
    }

    static bool connect(const char* id, const char* user, const char* pass) {
        lastConnectionAttempt = std::time(nullptr);
        
        // Simula conexão com delay de 500ms
        if (reconnectDelay > 0) {
            reconnectDelay -= 100;
            return false;
        }
        
        connected = true;
        return true;
    }

    static bool isConnected() {
        return connected;
    }

    static bool publish(const char* topic, const char* payload) {
        if (!connected) {
            return false;
        }
        messages.push_back({topic, payload});
        return true;
    }

    static void disconnect() {
        connected = false;
    }

    static int getLastConnectionTime() {
        return lastConnectionAttempt;
    }

    static std::vector<std::pair<std::string, std::string>> getMessages() {
        return messages;
    }

    static void setConnected(bool state) {
        connected = state;
    }

    static int getReconnectDelay() {
        return reconnectDelay;
    }
};

bool MQTTClientMock::connected = false;
std::vector<std::pair<std::string, std::string>> MQTTClientMock::messages;
int MQTTClientMock::lastConnectionAttempt = 0;
int MQTTClientMock::reconnectDelay = 5000;

// Namespace MQTTManager com funções testáveis
namespace MQTTManager {

    struct MQTTConfig {
        std::string host;
        int port;
        std::string username;
        std::string password;
        std::string deviceId;
    };

    struct TelemetryData {
        float temperature;
        int uptime;
        int heap;
        int rssi;
    };

    bool isValidConfig(const MQTTConfig& config) {
        return !config.host.empty() && 
               config.port > 0 && 
               config.port <= 65535 &&
               !config.username.empty() &&
               !config.password.empty() &&
               !config.deviceId.empty();
    }

    std::string createTelemetryJSON(const TelemetryData& data) {
        std::string json = "{\"temp\":" + std::to_string(data.temperature) +
                          ",\"uptime\":" + std::to_string(data.uptime) +
                          ",\"heap\":" + std::to_string(data.heap) +
                          ",\"rssi\":" + std::to_string(data.rssi) + "}";
        return json;
    }

    bool publishTelemetry(const std::string& topic, const TelemetryData& data) {
        if (!MQTTClientMock::isConnected()) {
            return false;
        }

        std::string payload = createTelemetryJSON(data);
        return MQTTClientMock::publish(topic.c_str(), payload.c_str());
    }

    bool publishStatus(const std::string& topic, const std::string& status) {
        if (!MQTTClientMock::isConnected()) {
            return false;
        }

        std::string json = "{\"status\":\"" + status + "\"}";
        return MQTTClientMock::publish(topic.c_str(), json.c_str());
    }

    int getConnectionLatency(const MQTTConfig& config) {
        // Simula latência baseada na distância/configuração
        if (config.host == "127.0.0.1") {
            return 1;  // localhost - 1ms
        } else if (config.host == "10.0.0.84") {
            return 25; // rede local - ~25ms
        } else {
            return 100; // internet - ~100ms
        }
    }

    bool validateTopic(const std::string& topic) {
        return !topic.empty() && topic.find(' ') == std::string::npos;
    }

    int calculateQoS(bool critical) {
        return critical ? 2 : 0;  // QoS 2 para crítico, 0 para telemetria
    }
}

// ===== TESTES =====

class MQTTManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        MQTTClientMock::reset();
    }

    void TearDown() override {
        MQTTClientMock::reset();
    }
};

// Teste: Validação de configuração MQTT válida
TEST_F(MQTTManagerTest, ValidConfig) {
    MQTTManager::MQTTConfig config = {
        "10.0.0.84",
        1883,
        "mqtt_addon",
        "1234",
        "esp_sensor_01"
    };

    EXPECT_TRUE(MQTTManager::isValidConfig(config));
}

// Teste: Validação - host vazio
TEST_F(MQTTManagerTest, InvalidConfigEmptyHost) {
    MQTTManager::MQTTConfig config = {
        "",
        1883,
        "mqtt_addon",
        "1234",
        "esp_sensor_01"
    };

    EXPECT_FALSE(MQTTManager::isValidConfig(config));
}

// Teste: Validação - porta inválida (0)
TEST_F(MQTTManagerTest, InvalidConfigZeroPort) {
    MQTTManager::MQTTConfig config = {
        "10.0.0.84",
        0,
        "mqtt_addon",
        "1234",
        "esp_sensor_01"
    };

    EXPECT_FALSE(MQTTManager::isValidConfig(config));
}

// Teste: Validação - porta inválida (> 65535)
TEST_F(MQTTManagerTest, InvalidConfigPortTooHigh) {
    MQTTManager::MQTTConfig config = {
        "10.0.0.84",
        65536,
        "mqtt_addon",
        "1234",
        "esp_sensor_01"
    };

    EXPECT_FALSE(MQTTManager::isValidConfig(config));
}

// Teste: Validação - username vazio
TEST_F(MQTTManagerTest, InvalidConfigEmptyUsername) {
    MQTTManager::MQTTConfig config = {
        "10.0.0.84",
        1883,
        "",
        "1234",
        "esp_sensor_01"
    };

    EXPECT_FALSE(MQTTManager::isValidConfig(config));
}

// Teste: Criar JSON de telemetria
TEST_F(MQTTManagerTest, CreateTelemetryJSON) {
    MQTTManager::TelemetryData data = {25.5f, 3600, 45000, -65};
    std::string json = MQTTManager::createTelemetryJSON(data);

    EXPECT_NE(json.find("\"temp\":25"), std::string::npos);
    EXPECT_NE(json.find("\"uptime\":3600"), std::string::npos);
    EXPECT_NE(json.find("\"heap\":45000"), std::string::npos);
    EXPECT_NE(json.find("\"rssi\":-65"), std::string::npos);
}

// Teste: Publicar telemetria quando desconectado
TEST_F(MQTTManagerTest, PublishTelemetryDisconnected) {
    MQTTManager::TelemetryData data = {25.5f, 3600, 45000, -65};
    bool result = MQTTManager::publishTelemetry("topic/telemetry", data);

    EXPECT_FALSE(result);
}

// Teste: Publicar telemetria quando conectado
TEST_F(MQTTManagerTest, PublishTelemetryConnected) {
    MQTTClientMock::setConnected(true);

    MQTTManager::TelemetryData data = {25.5f, 3600, 45000, -65};
    bool result = MQTTManager::publishTelemetry("topic/telemetry", data);

    EXPECT_TRUE(result);
    auto messages = MQTTClientMock::getMessages();
    EXPECT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].first, "topic/telemetry");
}

// Teste: Publicar status quando desconectado
TEST_F(MQTTManagerTest, PublishStatusDisconnected) {
    bool result = MQTTManager::publishStatus("topic/status", "online");

    EXPECT_FALSE(result);
}

// Teste: Publicar status quando conectado
TEST_F(MQTTManagerTest, PublishStatusConnected) {
    MQTTClientMock::setConnected(true);

    bool result = MQTTManager::publishStatus("topic/status", "online");

    EXPECT_TRUE(result);
    auto messages = MQTTClientMock::getMessages();
    EXPECT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].first, "topic/status");
}

// Teste: Latência para broker local
TEST_F(MQTTManagerTest, LocalBrokerLatency) {
    MQTTManager::MQTTConfig config = {"127.0.0.1", 1883, "user", "pass", "id"};
    int latency = MQTTManager::getConnectionLatency(config);

    EXPECT_LE(latency, 5);  // Localhost deve ser bem rápido
}

// Teste: Latência para rede local
TEST_F(MQTTManagerTest, LocalNetworkLatency) {
    MQTTManager::MQTTConfig config = {"10.0.0.84", 1883, "user", "pass", "id"};
    int latency = MQTTManager::getConnectionLatency(config);

    EXPECT_GT(latency, 5);
    EXPECT_LT(latency, 50);
}

// Teste: Validação de topic válido
TEST_F(MQTTManagerTest, ValidTopic) {
    EXPECT_TRUE(MQTTManager::validateTopic("addon/esp/device/telemetry"));
    EXPECT_TRUE(MQTTManager::validateTopic("topic/with/many/levels"));
}

// Teste: Validação de topic inválido (com espaço)
TEST_F(MQTTManagerTest, InvalidTopicWithSpace) {
    EXPECT_FALSE(MQTTManager::validateTopic("addon/esp device/telemetry"));
}

// Teste: Validação de topic inválido (vazio)
TEST_F(MQTTManagerTest, InvalidTopicEmpty) {
    EXPECT_FALSE(MQTTManager::validateTopic(""));
}

// Teste: QoS para mensagem crítica
TEST_F(MQTTManagerTest, CriticalMessageQoS) {
    int qos = MQTTManager::calculateQoS(true);
    EXPECT_EQ(qos, 2);
}

// Teste: QoS para telemetria
TEST_F(MQTTManagerTest, TelemetryQoS) {
    int qos = MQTTManager::calculateQoS(false);
    EXPECT_EQ(qos, 0);
}

// Teste: Múltiplas mensagens de telemetria
TEST_F(MQTTManagerTest, MultipleTelemetryMessages) {
    MQTTClientMock::setConnected(true);

    for (int i = 0; i < 5; i++) {
        MQTTManager::TelemetryData data = {20.0f + i, 3600 + i*60, 45000, -65};
        MQTTManager::publishTelemetry("topic/telemetry", data);
    }

    auto messages = MQTTClientMock::getMessages();
    EXPECT_EQ(messages.size(), 5);
}
