# 🔥 Exemplos Práticos - Como Usar Testes

## Exemplo 1: Seu Primeiro Teste

### Passo 1: Abra o arquivo de testes
```bash
code test/test_config_manager.cpp
```

### Passo 2: Encontre um teste simples
```cpp
TEST_F(ConfigManagerTest, ParseValidJSON) {
    std::string validJSON = "{\"ssid\":\"JANELA_AMARELA\",\"pass\":\"07112417\"}";
    DeviceConfig config;

    bool result = ConfigManager::parseJSON(validJSON, config);

    EXPECT_TRUE(result);
    EXPECT_EQ(config.wifiSsid, "JANELA_AMARELA");
    EXPECT_EQ(config.wifiPass, "07112417");
}
```

### Passo 3: Execute
```bash
./run_tests.sh config
```

### Resultado:
```
✅ ConfigManagerTest.ParseValidJSON        [PASSED]
```

---

## Exemplo 2: Adicionar um Novo Teste

### Cenário: Testar SSID com números e underscores

### Passo 1: Abra o arquivo
```bash
vim test/test_wifi_manager.cpp
```

### Passo 2: Adicione o novo teste
```cpp
// Depois do último teste, adicione:

TEST_F(WiFiManagerTest, SSIDWithNumbersAndUnderscores) {
    // ARRANGE
    std::string ssid = "REDE_IOT_2024";
    std::string password = "senha_segura_123";
    
    // ACT
    auto result = WiFiManager::connectWifi(ssid, password);
    
    // ASSERT
    EXPECT_TRUE(WiFiManager::isValidSSID(ssid));
    EXPECT_TRUE(WiFiManager::isValidPassword(password));
}
```

### Passo 3: Compile e execute
```bash
./run_tests.sh wifi
```

### Resultado esperado:
```
✅ WiFiManagerTest.SSIDWithNumbersAndUnderscores [PASSED]
```

---

## Exemplo 3: Testando Casos de Erro

### Cenário: Testar JSON malformado

```cpp
TEST_F(ConfigManagerTest, ParseMalformedJSON) {
    // ARRANGE
    std::string malformedJSON = "{malformed}";  // JSON inválido
    DeviceConfig config;
    
    // ACT
    bool result = ConfigManager::parseJSON(malformedJSON, config);
    
    // ASSERT
    EXPECT_FALSE(result);  // Deve falhar
    EXPECT_EQ(config.wifiSsid, "");  // SSID fica vazio
    EXPECT_EQ(config.wifiPass, "");  // Pass fica vazio
}
```

### Executar:
```bash
./run_tests.sh config
```

---

## Exemplo 4: Usar Mocks para Testar Publicação MQTT

### Cenário: Testar se telemetria é publicada corretamente

```cpp
TEST_F(MQTTManagerTest, TelemetryFormatIsCorrect) {
    // ARRANGE
    MQTTClientMock::setConnected(true);
    MQTTManager::TelemetryData data = {
        temperature: 23.5f,
        uptime: 7200,
        heap: 50000,
        rssi: -55
    };
    
    // ACT
    bool success = MQTTManager::publishTelemetry("sensor/temp", data);
    
    // ASSERT
    EXPECT_TRUE(success);
    
    // Verificar que a mensagem foi publicada
    auto messages = MQTTClientMock::getMessages();
    EXPECT_EQ(messages.size(), 1);
    EXPECT_NE(messages[0].second.find("23.5"), std::string::npos);
}
```

---

## Exemplo 5: Testar Validação de Entrada

### Cenário: Validar que password curto é rejeitado

```cpp
TEST_F(WiFiManagerTest, PasswordTooShortIsRejected) {
    // ARRANGE
    std::string shortPassword = "1234567";  // 7 caracteres (mínimo é 8)
    
    // ACT
    bool isValid = WiFiManager::isValidPassword(shortPassword);
    
    // ASSERT
    EXPECT_FALSE(isValid);  // Deve ser rejeitado
}
```

### Rodar teste específico:
```bash
platformio test -e native_test --filter "*PasswordTooShort*"
```

---

## Exemplo 6: Testar Multiple Scenarios com Parametrização

### Cenário: Testar vários SSIDs válidos

```cpp
class SSIDValidationTest : public ::testing::TestWithParam<std::string> {};

INSTANTIATE_TEST_SUITE_P(
    ValidSSIDs,
    SSIDValidationTest,
    ::testing::Values(
        "HOME_WIFI",
        "CAFE_NETWORK",
        "HOTEL_IOT",
        "5G_FAST",
        "REDE_123"
    )
);

TEST_P(SSIDValidationTest, AllValidSSIDsAreAccepted) {
    std::string ssid = GetParam();
    EXPECT_TRUE(WiFiManager::isValidSSID(ssid));
}
```

### Resultado: 5 testes (um para cada SSID)

---

## Exemplo 7: Debugar um Teste Falhando

### Cenário: Teste está falhando, você quer ver o porquê

### Opção 1: Usar modo verbose
```bash
./run_tests.sh verbose
```

### Opção 2: Usar filtro para rodar apenas um teste
```bash
platformio test -e native_test --filter "*ParseValidJSON*"
```

### Opção 3: Super verbose (mostra tudo)
```bash
platformio test -e native_test -vvv 2>&1 | tail -200
```

---

## Exemplo 8: Testar Estado Anterior e Posterior

### Cenário: Verificar que salvando muda o estado

```cpp
TEST_F(ConfigManagerTest, SaveChangesPersistentState) {
    // ARRANGE - Estado inicial
    EXPECT_FALSE(LittleFSMock::exists("/config.json"));
    
    DeviceConfig config;
    config.wifiSsid = "NOVA_REDE";
    config.wifiPass = "nova_senha_123";
    
    // ACT - Salvar config
    ConfigManager::saveConfig(config);
    
    // ASSERT - Verificar estado mudou
    EXPECT_TRUE(LittleFSMock::exists("/config.json"));
    
    // E que pode carregar
    DeviceConfig loaded;
    bool success = ConfigManager::loadConfig(loaded);
    EXPECT_TRUE(success);
    EXPECT_EQ(loaded.wifiSsid, "NOVA_REDE");
}
```

---

## Exemplo 9: Teste com Setup/Teardown

### Cenário: Garantir limpeza entre testes

```cpp
class MyCustomTest : public ::testing::Test {
protected:
    // Executado ANTES de cada teste
    void SetUp() override {
        // Limpar mocks
        LittleFSMock::reset();
        WiFiMock::reset();
        MQTTClientMock::reset();
        
        // Preparar dados iniciais
        testSSID = "TEST_NETWORK";
        testPass = "password123";
    }
    
    // Executado DEPOIS de cada teste
    void TearDown() override {
        // Limpeza final se necessário
        LittleFSMock::reset();
    }
    
    // Variáveis compartilhadas entre testes
    std::string testSSID;
    std::string testPass;
};

TEST_F(MyCustomTest, Example1) {
    // testSSID e testPass já estão preparados
    EXPECT_EQ(testSSID, "TEST_NETWORK");
}

TEST_F(MyCustomTest, Example2) {
    // Limpo pelo TearDown() do teste anterior
    // testSSID mantém seu valor inicial
}
```

---

## Exemplo 10: Teste de Integração Simples

### Cenário: Testar fluxo completo de conexão

```cpp
TEST_F(IntegrationTest, CompleteWiFiFlow) {
    // ARRANGE - Preparar config
    DeviceConfig config;
    config.wifiSsid = "VALID_NETWORK";
    config.wifiPass = "password123";
    
    // ACT 1 - Salvar config
    ConfigManager::saveConfig(config);
    
    // ASSERT 1 - Verificar salvo
    EXPECT_TRUE(ConfigManager::hasValidConfig());
    
    // ACT 2 - Tentar conectar
    auto result = WiFiManager::connectWifi(
        config.wifiSsid,
        config.wifiPass
    );
    
    // ASSERT 2 - Verificar conexão
    EXPECT_TRUE(result.isConnected);
    
    // ACT 3 - Tentar publicar telemetria
    MQTTClientMock::setConnected(true);
    MQTTManager::TelemetryData data = {25.0f, 3600, 45000, -65};
    bool published = MQTTManager::publishTelemetry("topic", data);
    
    // ASSERT 3 - Verificar publicação
    EXPECT_TRUE(published);
}
```

---

## Exemplo 11: Assertions Mais Comuns

```cpp
// Comparações básicas
EXPECT_EQ(a, b);              // a == b
EXPECT_NE(a, b);              // a != b
EXPECT_LT(a, b);              // a < b
EXPECT_LE(a, b);              // a <= b
EXPECT_GT(a, b);              // a > b
EXPECT_GE(a, b);              // a >= b

// Booleanos
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);

// Strings
EXPECT_EQ("abc", "abc");
EXPECT_NE(str1, str2);
EXPECT_STREQ("abc", "abc");   // C-style strings

// Floating point com tolerância
EXPECT_NEAR(23.456, 23.455, 0.01);  // Diferença ≤ 0.01

// Collections
EXPECT_EQ(container.size(), 5);
EXPECT_TRUE(container.empty());

// Nulo/nullptr
EXPECT_NULL(ptr);
EXPECT_NOT_NULL(ptr);

// Exceções
EXPECT_THROW(function(), ExceptionType);
EXPECT_NO_THROW(function());

// Fatal vs não-fatal
EXPECT_EQ(a, b);  // Continua se falhar
ASSERT_EQ(a, b);  // Para se falhar
```

---

## Exemplo 12: Dados de Teste Reutilizáveis

### Criar arquivo `test/fixtures/test_data.h`

```cpp
#pragma once

namespace TestData {
    // JSON válidos
    const std::string VALID_CONFIG_JSON = 
        "{\"ssid\":\"JANELA_AMARELA\",\"pass\":\"07112417\"}";
    
    const std::string VALID_CONFIG_WITH_SPECIAL = 
        "{\"ssid\":\"REDE-WIFI_2.4GHz\",\"pass\":\"Senh@123!\"}";
    
    // JSON inválidos
    const std::string JSON_NO_SSID = 
        "{\"pass\":\"password123\"}";
    
    const std::string JSON_NO_PASS = 
        "{\"ssid\":\"JANELA_AMARELA\"}";
    
    const std::string JSON_MALFORMED = 
        "{malformed}";
    
    // Credenciais válidas
    const std::string VALID_SSID = "JANELA_AMARELA";
    const std::string VALID_PASS = "07112417";
    
    // Credenciais inválidas
    const std::string SHORT_PASS = "1234567";  // 7 caracteres
    const std::string LONG_SSID = 
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";  // 33 caracteres
}
```

### Usar nos testes:
```cpp
#include "fixtures/test_data.h"

TEST_F(ConfigTest, UseSharedData) {
    DeviceConfig config;
    bool result = ConfigManager::parseJSON(
        TestData::VALID_CONFIG_JSON, 
        config
    );
    EXPECT_TRUE(result);
    EXPECT_EQ(config.wifiSsid, TestData::VALID_SSID);
}
```

---

## 🎯 Quick Reference

```bash
# Rodar todos os testes
./run_tests.sh all

# Apenas ConfigManager
./run_tests.sh config

# Apenas WiFiManager
./run_tests.sh wifi

# Apenas MQTTManager
./run_tests.sh mqtt

# Verbose (mostra cada teste)
./run_tests.sh verbose

# Apenas compilar
./run_tests.sh compile

# Teste específico
platformio test -e native_test --filter "ParseValidJSON"

# Múltiplos testes (regex)
platformio test -e native_test --filter "*Valid*"

# Super verbose (debug)
platformio test -e native_test -vvv
```

---

**Dicas Finais**:

1. ✅ Sempre use padrão AAA (Arrange-Act-Assert)
2. ✅ Um conceito por teste
3. ✅ Nomes descritivos
4. ✅ Limpe estado com SetUp/TearDown
5. ✅ Use mocks para dependências
6. ✅ Teste casos extremos
7. ✅ Documente casos complexos

---

**Criado**: 2 de Janeiro, 2026
