# 📚 Documentação - terraSmart Device

**Índice completo e navegação de toda a documentação do projeto**

---

## 🎯 Começar Aqui

### Para Usuários Novos
1. 📖 **[README.md](../README.md)** - Visão geral do projeto (5 min)
2. ⚡ **[VSCODE_TASKS.md](VSCODE_TASKS.md)** - Tasks do VS Code (Ctrl+Shift+B) ⭐ NOVO (10 min)
3. 🚀 **[GUIA_FINAL.md](GUIA_FINAL.md)** - Guia completo passo a passo (20 min)
4. 🧪 **[TESTING_GUIDE.md](TESTING_GUIDE.md)** - Como rodar testes (10 min)

### Para Desenvolvedores
1. ⚡ **[VSCODE_TASKS.md](VSCODE_TASKS.md)** - Tasks do VS Code (workflow profissional)
2. 🔨 **[BUILD_GUIDE.md](BUILD_GUIDE.md)** - Guia de compilação (linha de comando)
3. 🏗️ **[TESTING_ARCHITECTURE.md](TESTING_ARCHITECTURE.md)** - Entender arquitetura (10 min)
4. 💡 **[TESTING_BEST_PRACTICES.md](TESTING_BEST_PRACTICES.md)** - Padrões de código (15 min)
5. 📝 **[EXAMPLES_AND_SNIPPETS.md](EXAMPLES_AND_SNIPPETS.md)** - Exemplos práticos (20 min)

---

## 📂 Documentação por Categoria

### ⚡ **VS CODE E BUILD**

| Arquivo | Descrição | Tempo |
|---------|-----------|-------|
| **[VSCODE_TASKS.md](VSCODE_TASKS.md)** | Tasks do VS Code (menu interativo com Ctrl+Shift+B) | 15 min |
| **[BUILD_GUIDE.md](BUILD_GUIDE.md)** | Guia completo de compilação (linha de comando) | 10 min |

**Atalho Rápido:** `Ctrl+Shift+B` → Menu interativo ✨

---

### 🧪 **TESTES UNITÁRIOS** (42 testes - 100% aprovação ✅)

| Arquivo | Descrição | Tempo |
|---------|-----------|-------|
| **[TESTS_README.md](TESTS_README.md)** | Visão geral dos testes | 5 min |
| **[TESTING_GUIDE.md](TESTING_GUIDE.md)** | Como executar testes | 10 min |
| **[TESTING_BEST_PRACTICES.md](TESTING_BEST_PRACTICES.md)** | Padrões e conventions | 15 min |
| **[TESTING_ARCHITECTURE.md](TESTING_ARCHITECTURE.md)** | Diagramas e arquitetura | 10 min |
| **[ALL_TESTS_LIST.md](ALL_TESTS_LIST.md)** | Lista completa dos 42 testes | 10 min |
| **[TEST_SUMMARY.md](TEST_SUMMARY.md)** | Resultados e estatísticas | 5 min |
| **[EXAMPLES_AND_SNIPPETS.md](EXAMPLES_AND_SNIPPETS.md)** | 12 exemplos de testes | 20 min |

**→ [Mais sobre testes →](#testes-detalhados)**

---

### 🌐 **WiFi e PORTAL CAPTIVO**

| Arquivo | Descrição |
|---------|-----------|
| **[WIFI_SCAN_FEATURE.md](WIFI_SCAN_FEATURE.md)** | Sistema de scan de redes WiFi |
| **[WIFI_DEBUG_STATUS1.md](WIFI_DEBUG_STATUS1.md)** | Debug de status WiFi (diagnosticar problemas) |
| **[WIFI_QUICK_DEBUG.md](WIFI_QUICK_DEBUG.md)** | Quick reference para WiFi |
| **[CAPTIVE_PORTAL_DEBUG.md](CAPTIVE_PORTAL_DEBUG.md)** | Debug do portal captivo |
| **[SCAN_IMPLEMENTATION.md](SCAN_IMPLEMENTATION.md)** | Implementação do scanner de redes |

---

### 📡 **MQTT e TELEMETRIA**

| Arquivo | Descrição |
|---------|-----------|
| **[MQTT_CONEXAO_DEBUG.md](MQTT_CONEXAO_DEBUG.md)** | Debug de conexão MQTT |
| **[MQTT_DEBUG_AVANCADO.md](MQTT_DEBUG_AVANCADO.md)** | Debug avançado MQTT |
| **[MQTT_TELEMETRY_DEBUG.md](MQTT_TELEMETRY_DEBUG.md)** | Debug de telemetria |
| **[MQTT_PROXIMO_PASSO.md](MQTT_PROXIMO_PASSO.md)** | Próximas etapas MQTT |
| **[QUICK_START_TELEMETRY.md](QUICK_START_TELEMETRY.md)** | Quick start telemetria |
| **[TELEMETRY_FIXES.md](TELEMETRY_FIXES.md)** | Correções de telemetria |
| **[TELEMETRY_SUMMARY.md](TELEMETRY_SUMMARY.md)** | Sumário de telemetria |
| **[TELEMETRY_TEST_GUIDE.md](TELEMETRY_TEST_GUIDE.md)** | Guia de teste de telemetria |

---

### 📖 **GUIAS GERAIS**

| Arquivo | Descrição |
|---------|-----------|
| **[GUIA_FINAL.md](GUIA_FINAL.md)** | Guia final completo do projeto |
| **[RESUMO_EXECUTIVO.md](RESUMO_EXECUTIVO.md)** | Resumo executivo (status, features, etc) |

---

## 🧪 Testes Detalhados

### Estatísticas
```
✅ Total de Testes:        42
✅ Sucessos:               42 (100%)
✅ Falhas:                 0 (0%)
✅ Tempo Total:            0.71s
✅ Cobertura:              ~90%
```

### Testes por Módulo

#### ConfigManager (10 testes - 95% cobertura)
- Parsing JSON válido
- Parsing JSON com caracteres especiais
- Validação de JSON malformado
- Persistência em LittleFS
- Validação de SSID/Password

#### WiFiManager (14 testes - 90% cobertura)
- Conexão WiFi bem-sucedida
- SSID não encontrado
- Validação de credenciais
- Status codes WiFi
- Reconexão automática

#### MQTTManager (18 testes - 85% cobertura)
- Validação de configuração MQTT
- Publicação de telemetria
- Quality of Service (QoS)
- Topics MQTT
- Latência de conexão

**→ [Ver lista completa de testes →](ALL_TESTS_LIST.md)**

---

## 🚀 Quick Start

### 1️⃣ Configurar WiFi
```bash
# ESP entra em AP_MODE
# Conectar em: terraSmart-XXXX (192.168.4.1)
# Selecionar rede WiFi
# Reiniciar ESP
```

### 2️⃣ Rodar Testes
```bash
./run_tests.sh all
# → 42/42 testes ✅
```

### 3️⃣ Compilar e Fazer Upload
```bash
platformio run -e wemos_d1_mini32 --target upload
```

### 4️⃣ Monitorar Serial
```bash
platformio device monitor -b 115200
```

---

## 🔧 Configurações Principais

### MQTT (arquivo: `include/config.h`)
```cpp
#define MQTT_HOST "10.0.0.84"
#define MQTT_PORT 1883
#define MQTT_USER "mqtt_addon"
#define MQTT_PASS "1234"
#define DEVICE_ID "esp_sensor_01"
```

### Topics
- `addon/esp/esp_sensor_01/telemetry` - Dados do sensor
- `addon/esp/esp_sensor_01/status` - Status
- `addon/esp/esp_sensor_01/cmd` - Comandos

---

## 📊 Arquitetura

```
┌─────────────────────────────────────┐
│         ESP8266 WeMos D1 Mini       │
├─────────────────────────────────────┤
│ ConfigManager → LittleFS            │
│      ↓                              │
│ WiFiManager → ESP8266WiFi           │
│      ↓                              │
│ MQTTManager → PubSubClient          │
│      ↓                              │
│ CaptivePortal → HTTP + DNS          │
└─────────────────────────────────────┘
         ↓
    Home Assistant (MQTT)
```

---

## 🎯 Documentação por Objetivo

### "Quero rodar os testes"
→ Abra: **[TESTING_GUIDE.md](TESTING_GUIDE.md)**

### "Quero entender o projeto"
→ Abra: **[README.md](../README.md)** e **[GUIA_FINAL.md](GUIA_FINAL.md)**

### "Quero adicionar um novo teste"
→ Abra: **[TESTING_BEST_PRACTICES.md](TESTING_BEST_PRACTICES.md)** e **[EXAMPLES_AND_SNIPPETS.md](EXAMPLES_AND_SNIPPETS.md)**

### "WiFi não está conectando"
→ Abra: **[WIFI_DEBUG_STATUS1.md](WIFI_DEBUG_STATUS1.md)** e **[WIFI_QUICK_DEBUG.md](WIFI_QUICK_DEBUG.md)**

### "MQTT não está funcionando"
→ Abra: **[MQTT_DEBUG_AVANCADO.md](MQTT_DEBUG_AVANCADO.md)** e **[MQTT_CONEXAO_DEBUG.md](MQTT_CONEXAO_DEBUG.md)**

### "Telemetria não está sendo publicada"
→ Abra: **[TELEMETRY_TEST_GUIDE.md](TELEMETRY_TEST_GUIDE.md)** e **[MQTT_TELEMETRY_DEBUG.md](MQTT_TELEMETRY_DEBUG.md)**

### "Quero ver exemplos práticos"
→ Abra: **[EXAMPLES_AND_SNIPPETS.md](EXAMPLES_AND_SNIPPETS.md)**

---

## 📚 Leitura Recomendada por Nível

### 🟢 Iniciante (25 min)
1. README.md (5 min)
2. GUIA_FINAL.md (10 min)
3. TESTS_README.md (10 min)

### 🟡 Intermediário (45 min)
1. TESTING_GUIDE.md (10 min)
2. TESTING_ARCHITECTURE.md (15 min)
3. EXAMPLES_AND_SNIPPETS.md (20 min)

### 🔴 Avançado (60+ min)
1. TESTING_BEST_PRACTICES.md (15 min)
2. ALL_TESTS_LIST.md (15 min)
3. Todos os docs de debug (30+ min)

---

## 📖 Documentação Técnica

### Estrutura de Código
```
src/
├── main.cpp                    Entry point
├── command_handler.cpp         Manipulação de comandos
├── config/
│   ├── config_manager.cpp      Persistência de config
│   └── config_manager.h
├── wifi/
│   ├── wifi_manager.cpp        Gerenciamento WiFi
│   └── wifi_manager.h
├── mqtt/
│   ├── mqtt_manager.cpp        Gerenciamento MQTT
│   └── mqtt_manager.h
├── captive/
│   ├── captive_portal.cpp      Portal captivo
│   └── captive_portal.h
└── telemetry/
    ├── telemetry.cpp           Dados de telemetria
    └── telemetry.h
```

### Testes
```
test/
├── test_main.cpp               Google Test entry point
├── test_config_manager.cpp     10 testes
├── test_wifi_manager.cpp       14 testes
├── test_mqtt_manager.cpp       18 testes
└── mocks/
    ├── littlefs_mock.h         Mock de filesystem
    └── wifi_mock.h             Mock de WiFi
```

---

## 🔗 Links Úteis

- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP8266 Arduino Core](https://github.com/esp8266/Arduino)
- [PubSubClient (MQTT)](https://github.com/knolleary/pubsubclient)
- [Google Test Framework](https://google.github.io/googletest/)
- [Home Assistant](https://www.home-assistant.io/)

---

## 📊 Status da Documentação

| Categoria | Documentos | Status |
|-----------|-----------|--------|
| Testes | 7 | ✅ Completo |
| WiFi | 5 | ✅ Completo |
| MQTT | 8 | ✅ Completo |
| Guias | 2 | ✅ Completo |
| **Total** | **23** | **✅ Completo** |

---

## ✅ Checklist de Leitura

- [ ] README.md (projeto)
- [ ] TESTS_README.md (visão geral testes)
- [ ] TESTING_GUIDE.md (como rodar testes)
- [ ] GUIA_FINAL.md (projeto completo)
- [ ] Documentação específica do seu problema

---

## 🤝 Contribuir

Se encontrar erros ou tiver sugestões:

1. Abra um issue detalhando o problema
2. Consulte documentação relevante
3. Execute testes: `./run_tests.sh all`
4. Faça seu fix/melhoria
5. Verifique: `./run_tests.sh all` ✅

---

## 📞 Suporte Rápido

**Problema:** WiFi não conecta  
**Solução:** Veja [WIFI_DEBUG_STATUS1.md](WIFI_DEBUG_STATUS1.md)

**Problema:** MQTT não conecta  
**Solução:** Veja [MQTT_DEBUG_AVANCADO.md](MQTT_DEBUG_AVANCADO.md)

**Problema:** Telemetria não publica  
**Solução:** Veja [TELEMETRY_TEST_GUIDE.md](TELEMETRY_TEST_GUIDE.md)

**Problema:** Teste falhando  
**Solução:** Veja [TESTING_BEST_PRACTICES.md](TESTING_BEST_PRACTICES.md)

---

**Última atualização:** 2 de Janeiro, 2026  
**Documentação Completa:** ✅ 23 arquivos  
**Cobertura:** ~95% do projeto

[← Voltar para README.md](../README.md)
