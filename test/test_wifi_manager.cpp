#include <gtest/gtest.h>
#include <string>
#include <vector>

// Enums WiFi
enum wl_status_t {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6,
    WL_NO_SHIELD = 255
};

// Mock de WiFi
class WiFiMock {
private:
    static wl_status_t status;
    static std::string ssid;
    static std::string password;
    static bool connected;

public:
    static void reset() {
        status = WL_DISCONNECTED;
        ssid = "";
        password = "";
        connected = false;
    }

    static void setSSID(const std::string& s) {
        ssid = s;
    }

    static void setPassword(const std::string& p) {
        password = p;
    }

    static void setStatus(wl_status_t s) {
        status = s;
    }

    static void setConnected(bool c) {
        connected = c;
    }

    static wl_status_t getStatus() {
        return status;
    }

    static std::string getSSID() {
        return ssid;
    }

    static std::string getPassword() {
        return password;
    }

    static bool isConnected() {
        return connected;
    }

    static void beginAP(const char* ssid_name, const char* password_name) {
        ssid = ssid_name;
        password = password_name;
        status = WL_CONNECTED;
        connected = true;
    }

    static void begin(const char* ssid_name, const char* password_name) {
        ssid = ssid_name;
        password = password_name;
        // Simula tentativa de conexão
        if (ssid == "VALID_NETWORK") {
            status = WL_CONNECTED;
            connected = true;
        } else if (ssid == "NETWORK_OFFLINE") {
            status = WL_NO_SSID_AVAIL;
            connected = false;
        } else if (ssid == "WEAK_SIGNAL") {
            status = WL_CONNECT_FAILED;
            connected = false;
        } else {
            status = WL_IDLE_STATUS;
            connected = false;
        }
    }
};

wl_status_t WiFiMock::status = WL_DISCONNECTED;
std::string WiFiMock::ssid = "";
std::string WiFiMock::password = "";
bool WiFiMock::connected = false;

// Namespace WiFiManager com funções testáveis
namespace WiFiManager {

    struct WiFiStatus {
        bool isConnected;
        wl_status_t statusCode;
        std::string statusString;
    };

    std::string getStatusString(wl_status_t status) {
        switch (status) {
            case WL_IDLE_STATUS:       return "Aguardando...";
            case WL_NO_SSID_AVAIL:     return "SSID não encontrado";
            case WL_SCAN_COMPLETED:    return "Scan completo";
            case WL_CONNECTED:         return "Conectado";
            case WL_CONNECT_FAILED:    return "Falha na conexão";
            case WL_CONNECTION_LOST:   return "Conexão perdida";
            case WL_DISCONNECTED:      return "Desconectado";
            default:                   return "Status desconhecido";
        }
    }

    WiFiStatus connectWifi(const std::string& ssid, const std::string& password) {
        if (ssid.empty() || password.empty()) {
            return {false, WL_DISCONNECTED, "SSID ou senha vazio"};
        }

        WiFiMock::begin(ssid.c_str(), password.c_str());

        WiFiStatus result;
        result.statusCode = WiFiMock::getStatus();
        result.isConnected = WiFiMock::isConnected();
        result.statusString = getStatusString(result.statusCode);

        return result;
    }

    bool isValidSSID(const std::string& ssid) {
        return !ssid.empty() && ssid.length() <= 32;
    }

    bool isValidPassword(const std::string& password) {
        return !password.empty() && password.length() >= 8 && password.length() <= 63;
    }
}

// ===== TESTES =====

class WiFiManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        WiFiMock::reset();
    }

    void TearDown() override {
        WiFiMock::reset();
    }
};

// Teste: Conexão bem-sucedida
TEST_F(WiFiManagerTest, SuccessfulConnection) {
    auto result = WiFiManager::connectWifi("VALID_NETWORK", "password123");

    EXPECT_TRUE(result.isConnected);
    EXPECT_EQ(result.statusCode, WL_CONNECTED);
    EXPECT_EQ(result.statusString, "Conectado");
}

// Teste: SSID não encontrado
TEST_F(WiFiManagerTest, SSIDNotFound) {
    auto result = WiFiManager::connectWifi("NETWORK_OFFLINE", "password123");

    EXPECT_FALSE(result.isConnected);
    EXPECT_EQ(result.statusCode, WL_NO_SSID_AVAIL);
    EXPECT_EQ(result.statusString, "SSID não encontrado");
}

// Teste: Falha na conexão (sinal fraco)
TEST_F(WiFiManagerTest, ConnectionFailed) {
    auto result = WiFiManager::connectWifi("WEAK_SIGNAL", "password123");

    EXPECT_FALSE(result.isConnected);
    EXPECT_EQ(result.statusCode, WL_CONNECT_FAILED);
    EXPECT_EQ(result.statusString, "Falha na conexão");
}

// Teste: SSID vazio
TEST_F(WiFiManagerTest, EmptySSID) {
    auto result = WiFiManager::connectWifi("", "password123");

    EXPECT_FALSE(result.isConnected);
    EXPECT_EQ(result.statusCode, WL_DISCONNECTED);
}

// Teste: Senha vazia
TEST_F(WiFiManagerTest, EmptyPassword) {
    auto result = WiFiManager::connectWifi("JANELA_AMARELA", "");

    EXPECT_FALSE(result.isConnected);
}

// Teste: Validação de SSID - válido
TEST_F(WiFiManagerTest, ValidSSID) {
    EXPECT_TRUE(WiFiManager::isValidSSID("JANELA_AMARELA"));
    EXPECT_TRUE(WiFiManager::isValidSSID("A"));
    EXPECT_TRUE(WiFiManager::isValidSSID("12345678901234567890123456789012")); // 32 chars
}

// Teste: Validação de SSID - inválido (muito longo)
TEST_F(WiFiManagerTest, InvalidSSIDTooLong) {
    std::string longSSID(33, 'A');  // 33 caracteres
    EXPECT_FALSE(WiFiManager::isValidSSID(longSSID));
}

// Teste: Validação de SSID - inválido (vazio)
TEST_F(WiFiManagerTest, InvalidSSIDEmpty) {
    EXPECT_FALSE(WiFiManager::isValidSSID(""));
}

// Teste: Validação de Senha - válida
TEST_F(WiFiManagerTest, ValidPassword) {
    EXPECT_TRUE(WiFiManager::isValidPassword("12345678"));      // 8 chars
    EXPECT_TRUE(WiFiManager::isValidPassword("password123"));    // 11 chars
    EXPECT_TRUE(WiFiManager::isValidPassword("123456789012345678901234567890123456789012345678901234567890123")); // 63 chars
}

// Teste: Validação de Senha - inválida (muito curta)
TEST_F(WiFiManagerTest, InvalidPasswordTooShort) {
    EXPECT_FALSE(WiFiManager::isValidPassword("1234567"));  // 7 chars
}

// Teste: Validação de Senha - inválida (muito longa)
TEST_F(WiFiManagerTest, InvalidPasswordTooLong) {
    std::string longPass(64, 'A');  // 64 caracteres
    EXPECT_FALSE(WiFiManager::isValidPassword(longPass));
}

// Teste: Validação de Senha - inválida (vazia)
TEST_F(WiFiManagerTest, InvalidPasswordEmpty) {
    EXPECT_FALSE(WiFiManager::isValidPassword(""));
}

// Teste: String de status para cada código
TEST_F(WiFiManagerTest, StatusStrings) {
    EXPECT_EQ(WiFiManager::getStatusString(WL_IDLE_STATUS), "Aguardando...");
    EXPECT_EQ(WiFiManager::getStatusString(WL_NO_SSID_AVAIL), "SSID não encontrado");
    EXPECT_EQ(WiFiManager::getStatusString(WL_CONNECTED), "Conectado");
    EXPECT_EQ(WiFiManager::getStatusString(WL_CONNECT_FAILED), "Falha na conexão");
    EXPECT_EQ(WiFiManager::getStatusString(WL_DISCONNECTED), "Desconectado");
}

// Teste: Múltiplas tentativas de conexão
TEST_F(WiFiManagerTest, MultipleConnectionAttempts) {
    // Primeira tentativa - falha
    auto result1 = WiFiManager::connectWifi("INVALID", "password123");
    EXPECT_FALSE(result1.isConnected);

    // Segunda tentativa - sucesso
    auto result2 = WiFiManager::connectWifi("VALID_NETWORK", "password123");
    EXPECT_TRUE(result2.isConnected);
}
