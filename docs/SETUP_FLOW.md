# 🏗️ Arquitetura - Fluxo de Setup em Steps

## 📋 Visão Geral

Novo fluxo de configuração com **3 fases obrigatórias** antes de desligar o AP:

```
┌─────────────────────────────────────────────────────────────────┐
│                    FLUXO DE CONFIGURAÇÃO                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1️⃣  STEP 1: WiFi                                              │
│     ├─ Usuário seleciona rede WiFi                             │
│     └─ Insere senha                                            │
│     ⏳ Tenta conectar                                           │
│          ├─ ✅ Sucesso → Vai para STEP 2                      │
│          └─ ❌ Falha → Retorna para STEP 1 (max 3 tentativas) │
│                                                                 │
│  2️⃣  STEP 2: MQTT                                              │
│     ├─ Usuário insere host/port/user/pass                      │
│     └─ Testa conexão MQTT                                      │
│     ⏳ Tenta conectar                                           │
│          ├─ ✅ Sucesso → Vai para STEP 3                      │
│          └─ ❌ Falha → Retorna para STEP 2 (max 3 tentativas) │
│                                                                 │
│  3️⃣  STEP 3: Confirmação                                       │
│     ├─ Mostra resumo: WiFi + MQTT                             │
│     └─ Usuário clica "Finalizar"                              │
│          ✅ AP desliga → Modo Operacional                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│              FALLBACK: 3 Tentativas Falhas                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  WiFi falha 3x? → Volta para STEP 1                            │
│  MQTT falha 3x? → Volta para STEP 2                            │
│                                                                 │
│  Modo Operacional com erro de conexão?                         │
│  ├─ WiFi: 3 falhas → Volta para AP                             │
│  └─ MQTT: 3 falhas → Tenta reconectar, sem voltar AP           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🧠 Estados do Sistema

### Setup States (Durante AP)
```
SETUP_IDLE
├─ Esperando ação do usuário
│
├─ SETUP_WIFI_CONFIG
│  └─ Usuário configurando WiFi
│     ├─ → SETUP_WIFI_TESTING (clicou conectar)
│
├─ SETUP_WIFI_TESTING  
│  └─ Testando conexão WiFi
│     ├─ ✅ Sucesso → SETUP_MQTT_CONFIG
│     └─ ❌ Falha → SETUP_WIFI_FAILED → SETUP_WIFI_CONFIG
│
├─ SETUP_MQTT_CONFIG
│  └─ Usuário configurando MQTT
│     ├─ → SETUP_MQTT_TESTING (clicou conectar)
│
├─ SETUP_MQTT_TESTING
│  └─ Testando conexão MQTT
│     ├─ ✅ Sucesso → SETUP_CONFIRMATION
│     └─ ❌ Falha → SETUP_MQTT_FAILED → SETUP_MQTT_CONFIG
│
├─ SETUP_CONFIRMATION
│  └─ Mostrando resumo
│     └─ → SETUP_COMPLETE (clicou Finalizar)
│
└─ SETUP_COMPLETE
   └─ Reinicia em modo operacional
```

### Runtime States (Modo Operacional)
```
OPERATIONAL
├─ WiFi conectado ✅
├─ MQTT conectado ✅
└─ Enviando telemetria periodicamente

OPERATIONAL_WIFI_ERROR
├─ WiFi desconectou
├─ Tentando reconectar
└─ Contador de tentativas: 0/3

OPERATIONAL_MQTT_ERROR
├─ WiFi ok ✅
├─ MQTT com erro
├─ Tentando reconectar
└─ Contador de tentativas: 0/3

OPERATIONAL_ROLLBACK
├─ WiFi falhou 3x OU MQTT falhou 3x após WiFi ok
└─ Volta para AP mode
```

---

## 📊 Estrutura de Dados

### SetupConfig
```cpp
struct SetupConfig {
  // WiFi
  String wifiSsid;
  String wifiPass;
  int wifiAttempts;           // 0-3
  bool wifiConfigured;
  
  // MQTT
  String mqttHost;
  uint16_t mqttPort;
  String mqttUser;
  String mqttPass;
  int mqttAttempts;           // 0-3
  bool mqttConfigured;
  
  // Status geral
  SetupState currentState;
  unsigned long lastStateChange;
  String lastError;
};
```

---

## 🌐 Endpoints HTTP

### Portal - WiFi
```
GET /step/wifi
  └─ Mostra formulário de WiFi

POST /api/wifi/test
  ├─ Entrada: {ssid, pass}
  └─ Saída: {success, message}

POST /api/wifi/save
  ├─ Salva config após sucesso
  └─ Redireciona para /step/mqtt
```

### Portal - MQTT
```
GET /step/mqtt
  └─ Mostra formulário de MQTT

POST /api/mqtt/test
  ├─ Entrada: {host, port, user, pass}
  └─ Saída: {success, message}

POST /api/mqtt/save
  ├─ Salva config após sucesso
  └─ Redireciona para /step/confirm
```

### Portal - Confirmação
```
GET /step/confirm
  ├─ Mostra resumo: WiFi + MQTT
  └─ Botão: "Finalizar Configuração"

POST /api/setup/complete
  ├─ Desativa AP
  ├─ Reinicia
  └─ Inicia modo operacional
```

### API - Status
```
GET /api/setup/status
  └─ {state, wifiConfigured, mqttConfigured, errors}
```

---

## 🔄 Fluxo de Boot

### Boot Inicial (Sem Config)
```
1. ESP inicia
2. Verifica: config válida?
   ├─ NÃO → AP + Portal (STEP 1: WiFi)
   └─ SIM → Próximo passo

3. Conecta WiFi
   ├─ ✅ OK → Próximo passo
   └─ ❌ FALHA → AP + Portal (STEP 1: WiFi)

4. Conecta MQTT
   ├─ ✅ OK → Modo Operacional
   └─ ❌ FALHA → AP + Portal (STEP 2: MQTT)
```

### Boot em Modo Operacional (Reconexão)
```
1. ESP inicia
2. WiFi: Conecta
   ├─ ✅ OK → Próximo passo
   └─ ❌ FALHA (tentativa 1-3) → Tenta novamente
      └─ FALHA 3x → AP + Portal (STEP 1: WiFi)

3. MQTT: Conecta
   ├─ ✅ OK → Modo Normal
   └─ ❌ FALHA (tentativa 1-3) → Tenta novamente
      └─ FALHA 3x → Continua operacional, aguarda manual
```

---

## 💾 Persistência

```
LittleFS
├─ /config.json (WiFi + MQTT)
│  {
│    "wifiSsid": "...",
│    "wifiPass": "...",
│    "mqttHost": "...",
│    "mqttPort": 1883,
│    "mqttUser": "...",
│    "mqttPass": "..."
│  }
│
└─ /setup_state.json (Estado de setup)
   {
     "wifiAttempts": 0,
     "mqttAttempts": 0,
     "lastError": ""
   }
```

---

## 🎯 Implementação

### Arquivos Novos
1. **`include/setup_manager.h`** - Interface
2. **`src/setup/setup_manager.cpp`** - Lógica de estado
3. **`test/test_setup_manager.cpp`** - 15+ testes

### Arquivos Modificados
1. **`src/captive/captive_portal.cpp`** - Multi-step UI
2. **`src/main.cpp`** - Novo boot flow
3. **`src/config/config_manager.cpp`** - Suporte a MQTT

---

## 📝 Próximas Etapas

1. ✅ Criar `setup_manager.h`
2. ✅ Implementar `setup_manager.cpp`
3. ✅ Modificar portal HTML (3 steps)
4. ✅ Criar endpoints `/api/*`
5. ✅ Integrar ao `main.cpp`
6. ✅ Criar testes
7. ✅ Documentação final

---

## 🔍 Exemplo de Uso

### Durante Setup
```cpp
SetupManager setup;

// Tela 1: WiFi
setup.setState(SETUP_WIFI_CONFIG);
// Usuário entra SSID/Pass
setup.testWiFi("minha-rede", "senha123");
// Se ok → SETUP_MQTT_CONFIG

// Tela 2: MQTT
setup.setState(SETUP_MQTT_CONFIG);
// Usuário entra host/port/user/pass
setup.testMQTT("10.0.0.84", 1883, "usuario", "senha");
// Se ok → SETUP_CONFIRMATION

// Tela 3: Confirmação
// Finalizar → SETUP_COMPLETE → Reinicia
```

### Durante Operação
```cpp
// Loop principal
if (wifiConnected) {
  // OK, continua
} else {
  wifiAttempts++;
  if (wifiAttempts >= 3) {
    // Volta AP + Portal
    enterSetupMode();
  }
}
```

