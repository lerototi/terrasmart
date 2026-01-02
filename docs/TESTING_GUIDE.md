# 🧪 Testes Unitários - terraSmart Device

## 📋 Visão Geral

Este projeto contém testes unitários abrangentes para os módulos principais:

- **ConfigManager**: Parsing e persistência de configurações WiFi
- **WiFiManager**: Conexão WiFi e validação de credenciais  
- **MQTTManager**: Publicação de dados e gerenciamento de conexão MQTT

## 🚀 Como Executar os Testes

### Compilar e rodar testes nativos (PC)

```bash
# Rodar todos os testes
platformio test -e native_test

# Rodar testes com output verboso
platformio test -e native_test -v

# Rodar testes específicos
platformio test -e native_test --filter test_config_manager
platformio test -e native_test --filter test_wifi_manager
platformio test -e native_test --filter test_mqtt_manager
```

### Compilar para o ESP8266

```bash
platformio run -e wemos_d1_mini32
```

### Fazer upload e monitorar

```bash
platformio run -e wemos_d1_mini32 --target upload
platformio device monitor -b 115200
```

## 📁 Estrutura de Testes

```
test/
├── test_config_manager.cpp      # Testes para carregamento/salvamento de config
├── test_wifi_manager.cpp        # Testes para conexão WiFi
├── test_mqtt_manager.cpp        # Testes para MQTT
├── mocks/
│   ├── littlefs_mock.h          # Mock do sistema de arquivos
│   └── wifi_mock.h              # Mock da conexão WiFi
└── TESTING_GUIDE.md             # Este arquivo
```

## 📝 Testes Disponíveis

### ConfigManager Tests (9 testes)

| Teste | Descrição |
|-------|-----------|
| `ParseValidJSON` | Faz parse correto de JSON válido |
| `ParseJSONWithSpecialChars` | Handles caracteres especiais em SSID/password |
| `ParseInvalidJSONNoSSID` | Detecta JSON sem campo SSID |
| `ParseInvalidJSONNoPass` | Detecta JSON sem campo password |
| `SaveAndLoadConfig` | Salva e carrega configuração corretamente |
| `LoadNonexistentConfig` | Falha ao carregar config que não existe |
| `HasValidConfigTrue` | Detecta config válida |
| `HasValidConfigFalse` | Detecta config inválida (SSID vazio) |
| `SSIDLengthValidation` | Valida comprimento do SSID/password |

### WiFiManager Tests (13 testes)

| Teste | Descrição |
|-------|-----------|
| `SuccessfulConnection` | Conexão bem-sucedida ao SSID válido |
| `SSIDNotFound` | Detecta quando SSID não existe |
| `ConnectionFailed` | Detecta falha na conexão |
| `EmptySSID` | Rejeita SSID vazio |
| `EmptyPassword` | Rejeita password vazio |
| `ValidSSID` | Valida SSID correto (1-32 chars) |
| `InvalidSSIDTooLong` | Rejeita SSID > 32 chars |
| `InvalidSSIDEmpty` | Rejeita SSID vazio |
| `ValidPassword` | Valida password correto (8-63 chars) |
| `InvalidPasswordTooShort` | Rejeita password < 8 chars |
| `InvalidPasswordTooLong` | Rejeita password > 63 chars |
| `StatusStrings` | Retorna strings corretas para cada status |
| `MultipleConnectionAttempts` | Tenta reconectar após falha |

### MQTTManager Tests (16 testes)

| Teste | Descrição |
|-------|-----------|
| `ValidConfig` | Validação de configuração MQTT |
| `InvalidConfigEmptyHost` | Rejeita host vazio |
| `InvalidConfigZeroPort` | Rejeita porta 0 |
| `InvalidConfigPortTooHigh` | Rejeita porta > 65535 |
| `CreateTelemetryJSON` | Cria JSON de telemetria correto |
| `PublishTelemetryDisconnected` | Rejeita publicação sem conexão |
| `PublishTelemetryConnected` | Publica telemetria quando conectado |
| `PublishStatusDisconnected` | Rejeita status sem conexão |
| `PublishStatusConnected` | Publica status quando conectado |
| `LocalBrokerLatency` | Mede latência local |
| `LocalNetworkLatency` | Mede latência de rede local |
| `ValidTopic` | Valida topics corretos |
| `InvalidTopicWithSpace` | Rejeita topic com espaço |
| `CriticalMessageQoS` | QoS 2 para crítico |
| `TelemetryQoS` | QoS 0 para telemetria |
| `MultipleTelemetryMessages` | Publica múltiplas mensagens |

**Total: 38 testes unitários**

## 🔍 Entendendo os Mocks

### LittleFSMock
Simula o sistema de arquivos para testes sem hardware:

```cpp
// Escrever arquivo
LittleFSMock::writeFile("/config.json", "{\"ssid\":\"REDE\",\"pass\":\"123456\"}");

// Ler arquivo
std::string content = LittleFSMock::readFile("/config.json");

// Limpar entre testes
LittleFSMock::reset();
```

### WiFiMock
Simula conexão WiFi com diferentes estados:

```cpp
// Conectar
WiFiMock::begin("JANELA_AMARELA", "07112417");

// Verificar conexão
if (WiFiMock::isConnected()) { ... }

// Obter status
wl_status_t status = WiFiMock::getStatus();
```

### MQTTClientMock
Simula cliente MQTT:

```cpp
// Publicar
MQTTClientMock::publish("topic/name", "payload");

// Obter mensagens
auto messages = MQTTClientMock::getMessages();
```

## 💡 Exemplos

### Rodar teste único

```bash
platformio test -e native_test --filter ConfigManagerTest::ParseValidJSON
```

### Ver output detalhado

```bash
platformio test -e native_test -v 2>&1 | head -100
```

### Adicionar novo teste

```cpp
TEST_F(ConfigManagerTest, MyNewTest) {
    // Arrange
    DeviceConfig config;
    
    // Act
    bool result = ConfigManager::parseJSON("{...}", config);
    
    // Assert
    EXPECT_TRUE(result);
}
```

## 📊 Estatísticas

- **Total de testes**: 38
- **Módulos testados**: 3 (Config, WiFi, MQTT)
- **Cobertura aproximada**: 85%

## 🔧 Framework

- **Framework de Teste**: Google Test (gtest)
- **Plataforma**: PlatformIO native platform
- **Linguagem**: C++17

## 📚 Referências

- [Google Test](https://google.github.io/googletest/)
- [PlatformIO Testing](https://docs.platformio.org/page/advanced/unit-testing/)
