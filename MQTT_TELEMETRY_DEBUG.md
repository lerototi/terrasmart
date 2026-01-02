# 🔧 Debug - Telemetria MQTT

## ✅ O Que Foi Corrigido:

### 1. **Loop MQTT Não-Bloqueante**
```cpp
// ANTES: Travava esperando reconexão
while (!mqtt.connected()) { ... delay(2000); }

// DEPOIS: Tenta reconectar a cada 5 segundos
if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
```

### 2. **Verificação de Conexão**
```cpp
// Nova função: mqttIsConnected()
bool mqttIsConnected() { return mqtt.connected(); }

// Telemetria só envia se conectado
if (!mqttIsConnected()) return;
```

### 3. **Logs Detalhados**
```
[MQTT] Conectando...
[MQTT] OK ✓
[MQTT] Conectado ao broker: 10.0.0.84:1883
[MQTT] Inscrito em: addon/esp/esp_sensor_01/cmd
[TELEMETRY] ✓ {"temperature":...}
```

### 4. **Mensagens de Erro Específicas**
```
[MQTT] Timeout na conexão (rc=-4)
[MQTT] Servidor MQTT indisponível (rc=3)
[MQTT] Usuário/Senha incorretos (rc=4)
```

---

## 📊 Configuração de Tópicos:

```
MQTT_BASE_TOPIC:       "addon/esp/"
DEVICE_ID:             "esp_sensor_01"

MQTT_TELEMETRY_TOPIC:  "addon/esp/esp_sensor_01/telemetry"
MQTT_STATUS_TOPIC:     "addon/esp/esp_sensor_01/status"
MQTT_CMD_TOPIC:        "addon/esp/esp_sensor_01/cmd"
MQTT_RESP_TOPIC:       "addon/esp/esp_sensor_01/resp"
```

---

## 🧪 Como Testar:

### 1. Verificar Logs Serial
```bash
platformio device monitor -b 115200
```

### Esperado após inicializar:
```
=== ESP iniciado ===
[BOOT] Config encontrada → modo operacional
[CONFIG] SSID: JANELA AMARELA
[WIFI] Conectado: JANELA AMARELA, IP: 192.168.x.x
[MQTT] Configurado
[MQTT] Conectando...
[MQTT] OK ✓
[MQTT] Conectado ao broker: 10.0.0.84:1883
[MQTT] Inscrito em: addon/esp/esp_sensor_01/cmd
[TELEMETRY] ✓ {"temperature":25.3,"uptime":5,"heap":41124,"rssi":-60}
[TELEMETRY] ✓ {"temperature":28.1,"uptime":10,"heap":41108,"rssi":-60}
```

### 2. Testar com mosquitto_sub
```bash
# Terminal 1: Ouvir telemetria
mosquitto_sub -h 10.0.0.84 -u mqtt_addon -P 1234 -t "addon/esp/esp_sensor_01/telemetry"

# Esperado (a cada 5 segundos):
{"temperature":25.3,"uptime":5,"heap":41124,"rssi":-60}
{"temperature":28.1,"uptime":10,"heap":41108,"rssi":-60}
```

### 3. Testar Status
```bash
mosquitto_sub -h 10.0.0.84 -u mqtt_addon -P 1234 -t "addon/esp/esp_sensor_01/status"

# Esperado:
online
```

---

## 🐛 Possíveis Problemas:

### ❌ "MQTT Timeout"
```
[MQTT] Timeout na conexão (rc=-4)
```

**Causas:**
- Broker MQTT desligado
- Firewall bloqueando porta 1883
- Rede instável

**Solução:**
```bash
# Verificar conectividade
ping 10.0.0.84
telnet 10.0.0.84 1883
```

### ❌ "Usuário/Senha incorretos"
```
[MQTT] Usuário/Senha incorretos (rc=4)
```

**Solução:**
- Verificar config.h:
  - MQTT_USER: "mqtt_addon"
  - MQTT_PASS: "1234"

### ❌ "Servidor MQTT indisponível"
```
[MQTT] Servidor MQTT indisponível (rc=3)
```

**Solução:**
- Reiniciar broker MQTT
- Verificar IP/Porta do broker

### ❌ Telemetria não chega
```
[TELEMETRY] ✗ Não enviado - MQTT desconectado
```

**Solução:**
- Verificar se MQTT conectou (procure por "OK ✓")
- Verificar logs de erro anterior

---

## 📋 Fluxo de Inicialização:

```
┌─────────────────────────────────────────┐
│  ESP Inicia                             │
└────────────┬────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│  hasValidConfig() == true               │
│  Carrega config de LittleFS             │
└────────────┬────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│  connectWifi()                          │
│  Conecta ao SSID salvo                  │
└────────────┬────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│  setupMQTT()                            │
│  Configura PubSubClient                 │
└────────────┬────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│  loop() → loopMQTT()                    │
│  Tenta conectar ao broker               │
└────────────┬────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│  loop() → sendTelemetry()               │
│  A cada 5 segundos                      │
│  Publica em addon/esp/.../telemetry     │
└─────────────────────────────────────────┘
```

---

## 🔍 Debug Detalhado:

### No código, adicionar logs pode ajudar:

```cpp
// Em mqtt_manager.cpp
void loopMQTT() {
  if (mqtt.connected()) {
    Serial.println("[MQTT] Já conectado, processando...");
    mqtt.loop();
    return;
  }

  unsigned long now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL) {
    Serial.printf("[MQTT] Aguardando... (%ld ms)\n", 
      RECONNECT_INTERVAL - (now - lastReconnectAttempt));
    return;
  }
  
  // ... resto do código
}
```

---

## 📊 Payload de Telemetria:

```json
{
  "temperature": 25.3,
  "uptime": 5,
  "heap": 41124,
  "rssi": -60
}
```

**Legenda:**
- `temperature`: Temperatura simulada (20.0 ~ 32.0°C)
- `uptime`: Tempo ligado em segundos
- `heap`: Memória RAM livre em bytes
- `rssi`: Força do sinal Wi-Fi em dBm

---

## 📱 Teste Rápido:

1. **Verificar Wi-Fi conectado:**
   ```
   [WIFI] Conectado: JANELA AMARELA, IP: 192.168.x.x
   ```

2. **Verificar MQTT conectado:**
   ```
   [MQTT] OK ✓
   [MQTT] Conectado ao broker: 10.0.0.84:1883
   ```

3. **Verificar telemetria enviada:**
   ```
   [TELEMETRY] ✓ {"temperature":...}
   ```

4. **Receber no broker:**
   ```bash
   mosquitto_sub -h 10.0.0.84 -u mqtt_addon -P 1234 \
     -t "addon/esp/esp_sensor_01/telemetry"
   ```

---

## 🎯 Checklist:

- [ ] ESP conectado ao Wi-Fi
- [ ] MQTT broker respondendo
- [ ] Logs mostram "[MQTT] OK ✓"
- [ ] Logs mostram "[TELEMETRY] ✓"
- [ ] mosquitto_sub recebe mensagens
- [ ] Home Assistant recebe dados (se usá-lo)

---

## 📞 Se Não Funcionar:

1. ✅ Verificar IP do broker: 10.0.0.84
2. ✅ Verificar porta: 1883
3. ✅ Verificar usuário: mqtt_addon
4. ✅ Verificar senha: 1234
5. ✅ Verificar firewall
6. ✅ Verificar broker MQTT ativo
7. ✅ Verificar conectividade: `ping 10.0.0.84`

