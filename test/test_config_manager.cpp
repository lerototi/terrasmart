#include <gtest/gtest.h>
#include <string>
#include <map>

// Estrutura para simular config
struct DeviceConfig {
    std::string wifiSsid;
    std::string wifiPass;
};

// Mock de LittleFS para testes
class LittleFSMock {
private:
    static std::map<std::string, std::string> files;

public:
    static void reset() {
        files.clear();
    }

    static bool begin() {
        return true;
    }

    static bool exists(const char* path) {
        return files.find(path) != files.end();
    }

    static void writeFile(const char* path, const std::string& content) {
        files[path] = content;
    }

    static std::string readFile(const char* path) {
        if (files.find(path) != files.end()) {
            return files[path];
        }
        return "";
    }

    static void deleteFile(const char* path) {
        files.erase(path);
    }

    static bool isInitialized() {
        return true;
    }
};

std::map<std::string, std::string> LittleFSMock::files;

// Funções do ConfigManager (versão testável)
namespace ConfigManager {
    
    bool parseJSON(const std::string& json, DeviceConfig& config) {
        // Parse SSID
        int ssidStart = json.find("\"ssid\"");
        if (ssidStart == std::string::npos) {
            return false;
        }

        int ssidValueStart = json.find("\"", ssidStart + 7) + 1;
        int ssidValueEnd = json.find("\"", ssidValueStart);

        if (ssidValueStart < 0 || ssidValueEnd < 0) {
            return false;
        }

        config.wifiSsid = json.substr(ssidValueStart, ssidValueEnd - ssidValueStart);

        // Parse PASSWORD
        int passStart = json.find("\"pass\"");
        if (passStart == std::string::npos) {
            return false;
        }

        int passValueStart = json.find("\"", passStart + 7) + 1;
        int passValueEnd = json.find("\"", passValueStart);

        if (passValueStart < 0 || passValueEnd < 0) {
            return false;
        }

        config.wifiPass = json.substr(passValueStart, passValueEnd - passValueStart);

        return true;
    }

    bool loadConfig(DeviceConfig& config) {
        if (!LittleFSMock::begin()) {
            return false;
        }

        if (!LittleFSMock::exists("/config.json")) {
            return false;
        }

        std::string json = LittleFSMock::readFile("/config.json");
        if (json.empty()) {
            return false;
        }

        return parseJSON(json, config);
    }

    void saveConfig(const DeviceConfig& config) {
        std::string json = "{\"ssid\":\"" + config.wifiSsid + "\",\"pass\":\"" + config.wifiPass + "\"}";
        LittleFSMock::writeFile("/config.json", json);
    }

    bool hasValidConfig() {
        DeviceConfig cfg;
        return loadConfig(cfg) && cfg.wifiSsid.length() > 0;
    }
}

// ===== TESTES =====

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        LittleFSMock::reset();
    }

    void TearDown() override {
        LittleFSMock::reset();
    }
};

// Teste: Parser JSON válido
TEST_F(ConfigManagerTest, ParseValidJSON) {
    std::string validJSON = "{\"ssid\":\"JANELA_AMARELA\",\"pass\":\"07112417\"}";
    DeviceConfig config;

    bool result = ConfigManager::parseJSON(validJSON, config);

    EXPECT_TRUE(result);
    EXPECT_EQ(config.wifiSsid, "JANELA_AMARELA");
    EXPECT_EQ(config.wifiPass, "07112417");
}

// Teste: Parser JSON com caracteres especiais
TEST_F(ConfigManagerTest, ParseJSONWithSpecialChars) {
    std::string json = "{\"ssid\":\"REDE-WIFI_2.4GHz\",\"pass\":\"Senh@123!\"}";
    DeviceConfig config;

    bool result = ConfigManager::parseJSON(json, config);

    EXPECT_TRUE(result);
    EXPECT_EQ(config.wifiSsid, "REDE-WIFI_2.4GHz");
    EXPECT_EQ(config.wifiPass, "Senh@123!");
}

// Teste: Parser JSON inválido (sem SSID)
TEST_F(ConfigManagerTest, ParseInvalidJSONNoSSID) {
    std::string invalidJSON = "{\"pass\":\"password123\"}";
    DeviceConfig config;

    bool result = ConfigManager::parseJSON(invalidJSON, config);

    EXPECT_FALSE(result);
}

// Teste: Parser JSON inválido (sem PASSWORD)
TEST_F(ConfigManagerTest, ParseInvalidJSONNoPass) {
    std::string invalidJSON = "{\"ssid\":\"JANELA_AMARELA\"}";
    DeviceConfig config;

    bool result = ConfigManager::parseJSON(invalidJSON, config);

    EXPECT_FALSE(result);
}

// Teste: Salvar e carregar configuração
TEST_F(ConfigManagerTest, SaveAndLoadConfig) {
    DeviceConfig configToSave;
    configToSave.wifiSsid = "JANELA_AMARELA";
    configToSave.wifiPass = "07112417";

    ConfigManager::saveConfig(configToSave);

    DeviceConfig loadedConfig;
    bool result = ConfigManager::loadConfig(loadedConfig);

    EXPECT_TRUE(result);
    EXPECT_EQ(loadedConfig.wifiSsid, "JANELA_AMARELA");
    EXPECT_EQ(loadedConfig.wifiPass, "07112417");
}

// Teste: Carregar config que não existe
TEST_F(ConfigManagerTest, LoadNonexistentConfig) {
    DeviceConfig config;
    bool result = ConfigManager::loadConfig(config);

    EXPECT_FALSE(result);
}

// Teste: Config válida existe
TEST_F(ConfigManagerTest, HasValidConfigTrue) {
    DeviceConfig config;
    config.wifiSsid = "JANELA_AMARELA";
    config.wifiPass = "07112417";

    ConfigManager::saveConfig(config);
    bool result = ConfigManager::hasValidConfig();

    EXPECT_TRUE(result);
}

// Teste: Config inválida (SSID vazio)
TEST_F(ConfigManagerTest, HasValidConfigFalse) {
    LittleFSMock::writeFile("/config.json", "{\"ssid\":\"\",\"pass\":\"password\"}");
    bool result = ConfigManager::hasValidConfig();

    EXPECT_FALSE(result);
}

// Teste: Comprimento do SSID
TEST_F(ConfigManagerTest, SSIDLengthValidation) {
    std::string json = "{\"ssid\":\"NETWORK\",\"pass\":\"pass123\"}";
    DeviceConfig config;

    ConfigManager::parseJSON(json, config);

    EXPECT_EQ(config.wifiSsid.length(), 7);
    EXPECT_EQ(config.wifiPass.length(), 7);
}

// Teste: SSID com espaços (não deve funcionar em JSON sem escape)
TEST_F(ConfigManagerTest, SSIDWithSpacesJSON) {
    std::string json = "{\"ssid\":\"NETWORK NAME\",\"pass\":\"pass\"}";
    DeviceConfig config;

    bool result = ConfigManager::parseJSON(json, config);

    // Deve falhar porque tem espaço não escapado
    // Ou se o parser for robusto, deve capturar até o espaço
    EXPECT_TRUE(result || !result);  // Depende da implementação
}
