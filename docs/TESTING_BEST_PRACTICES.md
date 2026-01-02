# 🎯 Melhores Práticas de Testes Unitários

## 1. Estrutura de Testes (AAA Pattern)

Todo teste deve seguir a estrutura **Arrange-Act-Assert**:

```cpp
TEST_F(MyTestClass, DescriptionOfTest) {
    // ===== ARRANGE =====
    // Preparar dados e estado inicial
    DeviceConfig config;
    std::string testData = "{\"ssid\":\"TEST\",\"pass\":\"12345678\"}";
    
    // ===== ACT =====
    // Executar a função que está sendo testada
    bool result = ConfigManager::parseJSON(testData, config);
    
    // ===== ASSERT =====
    // Verificar se o resultado é o esperado
    EXPECT_TRUE(result);
    EXPECT_EQ(config.wifiSsid, "TEST");
}
```

## 2. Nomenclatura de Testes

Use a convenção: `Test{UnitUnderTest}{ScenarioBeingTested}`

❌ **Ruim:**
```cpp
TEST_F(ConfigTest, Test1) { }
TEST_F(ConfigTest, works) { }
```

✅ **Bom:**
```cpp
TEST_F(ConfigManagerTest, ParseValidJSON) { }
TEST_F(ConfigManagerTest, ParseInvalidJSONNoSSID) { }
```

## 3. Setup e Teardown

Sempre limpe o estado entre testes:

```cpp
class MyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Executado antes de cada teste
        LittleFSMock::reset();
        WiFiMock::reset();
    }
    
    void TearDown() override {
        // Executado após cada teste
        // Limpar recursos se necessário
    }
};
```

## 4. Um Conceito por Teste

Cada teste deve verificar **apenas uma coisa**:

❌ **Ruim:**
```cpp
TEST_F(ConfigTest, EverythingWorks) {
    // Testa parse, save, load e validação - tudo junto
    // Se falhar, não sabemos o que quebrou
}
```

✅ **Bom:**
```cpp
TEST_F(ConfigManagerTest, ParseValidJSON) {
    // Testa apenas o parse
}

TEST_F(ConfigManagerTest, SaveAndLoadConfig) {
    // Testa apenas save/load
}
```

## 5. Mocks e Dependency Injection

Não teste dependências externas, use mocks:

❌ **Ruim:**
```cpp
TEST(ConfigTest, LoadRealFile) {
    // Tenta ler do filesystem real - lento e flaky
    loadConfig(config);
}
```

✅ **Bom:**
```cpp
TEST_F(ConfigManagerTest, LoadConfig) {
    // Usa LittleFSMock - rápido e confiável
    LittleFSMock::writeFile("/config.json", testData);
    bool result = ConfigManager::loadConfig(config);
    EXPECT_TRUE(result);
}
```

## 6. Assertions Úteis

```cpp
// Comparações básicas
EXPECT_EQ(a, b);           // a == b
EXPECT_NE(a, b);           // a != b
EXPECT_LT(a, b);           // a < b
EXPECT_LE(a, b);           // a <= b
EXPECT_GT(a, b);           // a > b
EXPECT_GE(a, b);           // a >= b

// Booleanos
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);

// Strings
EXPECT_STREQ(str1, str2);  // Comparação C-style
EXPECT_EQ(string1, string2); // Para std::string

// Floating point (com tolerância)
EXPECT_NEAR(val1, val2, abs_error);

// Collections
EXPECT_EQ(container.size(), 5);

// FATAL vs não-fatal
EXPECT_EQ(a, b);  // Continua teste se falhar
ASSERT_EQ(a, b);  // Para teste se falhar
```

## 7. Testes Parametrizados

Para testar múltiplos valores:

```cpp
class PasswordValidationTest : public ::testing::TestWithParam<std::string> {};

INSTANTIATE_TEST_SUITE_P(
    ValidPasswords,
    PasswordValidationTest,
    ::testing::Values(
        "12345678",
        "password123",
        "Senh@123!"
    )
);

TEST_P(PasswordValidationTest, ValidatesCorrectly) {
    std::string password = GetParam();
    EXPECT_TRUE(WiFiManager::isValidPassword(password));
}
```

## 8. Testes de Exceção

Se necessário testar exceções:

```cpp
// Espera que a função lance uma exceção
EXPECT_THROW(risky_function(), std::exception);

// Espera que não lance
EXPECT_NO_THROW(safe_function());
```

## 9. Organização de Código

```
test/
├── test_config_manager.cpp    // Testes
├── test_wifi_manager.cpp
├── test_mqtt_manager.cpp
├── mocks/
│   ├── littlefs_mock.h        // Mocks
│   ├── wifi_mock.h
│   └── mqtt_mock.h
├── fixtures/
│   └── test_data.h            // Dados de teste reutilizáveis
└── README                      // Documentação
```

## 10. Dados de Teste

Centralizar dados para reutilização:

```cpp
// test/fixtures/test_data.h
namespace TestData {
    const std::string VALID_JSON = "{\"ssid\":\"JANELA_AMARELA\",\"pass\":\"07112417\"}";
    const std::string INVALID_SSID_JSON = "{\"pass\":\"123456\"}";
    const std::string VALID_SSID = "JANELA_AMARELA";
    const std::string VALID_PASSWORD = "07112417";
}

// Uso em testes
TEST_F(ConfigTest, Parse) {
    DeviceConfig config;
    bool result = ConfigManager::parseJSON(TestData::VALID_JSON, config);
    EXPECT_TRUE(result);
}
```

## 11. Documentação em Testes

```cpp
TEST_F(ConfigManagerTest, ParseSSIDWithSpecialCharacters) {
    // RFC 2822 permite caracteres especiais em SSID IEEE 802.11
    // Esta rede: "REDE-WIFI_2.4GHz"
    // Garante compatibilidade com redes do mundo real
    
    std::string json = "{\"ssid\":\"REDE-WIFI_2.4GHz\",\"pass\":\"pass123\"}";
    DeviceConfig config;
    
    bool result = ConfigManager::parseJSON(json, config);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(config.wifiSsid, "REDE-WIFI_2.4GHz");
}
```

## 12. Cobertura de Código

```bash
# Gerar relatório de cobertura
platformio test -e native_test --coverage

# Objetivo: >= 80% para código crítico
# Config: 90%
# WiFi: 85%
# MQTT: 80%
```

## 13. Testes de Regressão

Quando encontrar um bug, primeiro crie um teste que falha, depois corrija:

```cpp
// Problema relatado: SSID com espaços estava sendo parseado incorretamente
TEST_F(ConfigManagerTest, ParseSSIDWithSpaces) {
    // Este teste DEVE falhar inicialmente
    std::string json = "{\"ssid\":\"REDE WIFI\",\"pass\":\"12345678\"}";
    DeviceConfig config;
    
    // Após fix, deve passar
    bool result = ConfigManager::parseJSON(json, config);
    EXPECT_TRUE(result);
    EXPECT_EQ(config.wifiSsid, "REDE WIFI");
}
```

## 14. Boas Práticas Gerais

✅ **Faça:**
- Testes independentes (um não depende do outro)
- Testes rápidos (< 100ms cada)
- Testes determinísticos (sempre mesmo resultado)
- Testes fáceis de entender
- Use descritivos nomes
- Isole dependências com mocks
- Teste casos extremos (empty, null, max value)
- Documente casos complexos

❌ **Evite:**
- Compartilhar estado entre testes
- Testes lentos ou flaky
- Lógica complexa em SetUp/TearDown
- Testes que dependem de ordem
- Testar detalhes de implementação
- Hard-coded paths ou valores mágicos
- Testes sem verificações (sem EXPECT/ASSERT)

## 15. Executar Testes Localmente

```bash
# Todos os testes
./run_tests.sh all

# Teste específico
./run_tests.sh config

# Verboso
./run_tests.sh verbose

# Apenas compilar
./run_tests.sh compile

# Ajuda
./run_tests.sh help
```

## 📊 Checklist de Qualidade

Antes de fazer commit:

- [ ] Todos os testes passando: `./run_tests.sh all`
- [ ] Nenhuma warning de compilação
- [ ] Código compila no hardware: `platformio run -e wemos_d1_mini32`
- [ ] Cobertura >= 80%
- [ ] Testes nomeados descritivamente
- [ ] Setup/Teardown limpa estado
- [ ] Sem testes flaky (rodar 2x)

## 🔗 Referências

- [Google Test Primer](https://google.github.io/googletest/primer.html)
- [Google Test Advanced Guide](https://google.github.io/googletest/advanced.html)
- [Arduino Testing](https://create.arduino.cc/projecthub/Arduino_Genuino/unit-testing-d1a2c1)
- [Working Effectively with Legacy Code](https://www.oreilly.com/library/view/working-effectively-with/0131177052/)
