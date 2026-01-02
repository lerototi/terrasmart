# 🔍 Debug Completo - Conexão MQTT

## 📊 O Que Foi Adicionado:

### 1. **Setup MQTT com Logs Detalhados**
```
╔════════════════════════════════════╗
║     CONFIGURAÇÃO MQTT              ║
╚════════════════════════════════════╝
[MQTT] Host: 10.0.0.84
[MQTT] Porta: 1883
[MQTT] Usuário: mqtt_addon
[MQTT] Senha: 1234
[MQTT] Device ID: esp_sensor_01
[MQTT] Tópico Status: addon/esp/esp_sensor_01/status
[MQTT] Tópico Telemetria: addon/esp/esp_sensor_01/telemetry
[MQTT] Tópico Comando: addon/esp/esp_sensor_01/cmd
```

### 2. **Tentativa de Conexão com Debug**
```
[MQTT] ═══════════════════════════════════
[MQTT] Tentando conectar em: 10.0.0.84:1883 (usuário: mqtt_addon)
[MQTT] IP Local: 192.168.100.50
```

### 3. **Erro Específico com Sugestões**
```
[MQTT] ✗ Falha na conexão - rc=-2
[MQTT] ► Socket não conectado (verificar IP/Porta/Firewall)
[MQTT] Estado WiFi: 3 (conectado=3)
[MQTT] SSID: JANELA AMARELA
[MQTT] IP: 192.168.100.50
[MQTT] Tentará reconectar em 5 segundos...
```

### 4. **Função de Teste de Conectividade**

---

## 🧪 Como Usar o Teste:

### No Serial Monitor, você verá:

```
╔════════════════════════════════════╗
║   TESTE DE CONEXÃO MQTT            ║
╚════════════════════════════════════╝

1️⃣  VERIFICANDO WIFI:
   Status: 3 (esperado: 3)
   SSID: JANELA AMARELA
   IP Local: 192.168.100.50
   RSSI: -60 dBm
   ✓ WiFi OK

2️⃣  CONFIGURAÇÃO MQTT:
   Host: 10.0.0.84
   Porta: 1883
   Usuário: mqtt_addon
   Senha: 1234
   Device ID: esp_sensor_01

3️⃣  TENTANDO CONECTAR:
   Conectando em 10.0.0.84:1883... ✓ Sucesso!

4️⃣  STATUS MQTT ATUAL:
   Conectado: Não ✗
   Estado: -2
```

---

## 🔍 Diagnóstico pelo Erro rc=-2:

### rc=-2 = "Socket não conectado"

**Possíveis causas:**

1. **❌ Host/IP incorreto**
   - Verificar: `MQTT_HOST` em `config.h`
   - Atual: `10.0.0.84`
   - Teste: `ping 10.0.0.84`

2. **❌ Porta incorreta**
   - Verificar: `MQTT_PORT` em `config.h`
   - Atual: `1883`
   - Teste: `telnet 10.0.0.84 1883`

3. **❌ Firewall bloqueando**
   - Permitir porta 1883 no firewall
   - Conectividade: `ping 10.0.0.84`

4. **❌ Broker não está rodando**
   - Verificar: `systemctl status mosquitto`
   - Iniciar: `systemctl start mosquitto`

5. **❌ WiFi instável**
   - Verificar IP local no log
   - Verificar RSSI (sinal WiFi)

---

## 📋 Logs Esperados:

### ✅ Sucesso Completo:

```
╔════════════════════════════════════╗
║     CONFIGURAÇÃO MQTT              ║
╚════════════════════════════════════╝
[MQTT] Host: 10.0.0.84
[MQTT] Porta: 1883
[MQTT] Usuário: mqtt_addon
[MQTT] Senha: 1234

[MQTT] ═══════════════════════════════════
[MQTT] Tentando conectar em: 10.0.0.84:1883
[MQTT] IP Local: 192.168.100.50
[MQTT] ✓ Conectado com sucesso!
[MQTT] Broker: 10.0.0.84:1883
[MQTT] ✓ Inscrito em: addon/esp/esp_sensor_01/cmd
```

### ❌ Socket Não Conectado:

```
[MQTT] ✗ Falha na conexão - rc=-2
[MQTT] ► Socket não conectado (verificar IP/Porta/Firewall)
[MQTT] Estado WiFi: 3 (conectado=3)
[MQTT] SSID: JANELA AMARELA
[MQTT] IP: 192.168.100.50
```

**Ação:** Verificar conectividade com broker

### ❌ Timeout:

```
[MQTT] ✗ Falha na conexão - rc=-4
[MQTT] ► Timeout na conexão
```

**Ação:** Verificar firewall ou broker não respondendo

### ❌ Usuário/Senha:

```
[MQTT] ✗ Falha na conexão - rc=4
[MQTT] ► Usuário/Senha incorretos
```

**Ação:** Verificar `mqtt_addon` / `1234` em `config.h`

---

## 🧰 Checklist de Debug:

### 1. WiFi Conectado?
- [ ] Status WiFi: 3 (WL_CONNECTED)
- [ ] IP Local: não é 0.0.0.0
- [ ] RSSI: não é 0 dBm

### 2. Broker Acessível?
```bash
# De outro computador, fazer:
ping 10.0.0.84
telnet 10.0.0.84 1883
```

### 3. Broker Ativo?
```bash
# No servidor Home Assistant:
systemctl status mosquitto
# Ou
docker ps | grep mosquitto
```

### 4. Credenciais Corretas?
```bash
# Verificar em config.h:
MQTT_USER: "mqtt_addon"
MQTT_PASS: "1234"
```

### 5. Porta Correta?
```bash
# Default MQTT: 1883
# Verificar em config.h: MQTT_PORT 1883
```

---

## 🚀 Solução Rápida:

Se rc=-2 (Socket não conectado):

**1. Verificar IP do Broker:**
```bash
# No servidor Home Assistant:
hostname -I
# ou
ip addr show
```

**2. Atualizar em `config.h`:**
```cpp
#define MQTT_HOST "10.0.0.84"  // IP correto do servidor
#define MQTT_PORT 1883
```

**3. Recompilar e upload:**
```bash
platformio run --target upload
```

**4. Verificar logs:**
```bash
platformio device monitor -b 115200
```

---

## 📞 Teste de Conectividade via Terminal:

```bash
# Abrir monitor
platformio device monitor -b 115200

# Você verá logs como:
[MQTT] ═══════════════════════════════════
[MQTT] Tentando conectar em: 10.0.0.84:1883
[MQTT] IP Local: 192.168.100.50

# Se conectado: ✓ Conectado com sucesso!
# Se falhar: ✗ Falha na conexão - rc=...
```

---

## 🔧 Ações Por Erro:

| Erro | Causa | Solução |
|------|-------|---------|
| rc=-2 | Socket não conectado | Verificar IP/Porta/Firewall |
| rc=-4 | Timeout | Verificar broker ativo |
| rc=3 | Servidor indisponível | Reiniciar broker |
| rc=4 | Usuário/Senha | Verificar config.h |

---

## 📊 Estrutura do Debug:

```
┌─────────────────────────────────┐
│  setupMQTT()                    │
│  └─ Mostra configurações        │
└────────┬────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│  loopMQTT()                     │
│  ├─ Verifica WiFi               │
│  ├─ Tenta conectar MQTT         │
│  └─ Mostra erros específicos    │
└────────┬────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│  testMQTTConnection() (optional)│
│  └─ Teste de conectividade      │
└─────────────────────────────────┘
```

---

## ✅ Próximas Etapas:

1. ✅ Upload firmware
2. 📊 Verificar logs detalhados
3. 🔍 Identificar o erro específico
4. 🛠️ Aplicar solução apropriada
5. 🚀 Testar conexão MQTT

