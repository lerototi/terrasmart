# 🐛 Diagnóstico - Erro de Conexão MQTT rc=-2

## 📊 Erro Recebido:

```
[MQTT] ✗ Falha na conexão - rc=-2
[MQTT] ► Socket não conectado (verificar IP/Porta/Firewall)
```

---

## 🔍 O que significa rc=-2?

**"Socket não conectado"** = O ESP não consegue estabelecer conexão TCP/IP com o broker MQTT.

---

## 🛠️ Possíveis Causas (Em Ordem de Probabilidade):

### 1️⃣ **IP/Host Incorreto** (40% de probabilidade)

**Verificar em `config.h`:**
```cpp
#define MQTT_HOST "10.0.0.84"
```

**Como testar:**
```bash
# Do computador onde está o Home Assistant:
ping 10.0.0.84
# ou
hostname -I
# ou
ip addr show

# Se não conseguir ping, pode estar em rede diferente
```

**Solução:**
1. Descubra o IP real do servidor Home Assistant
2. Atualize em `config.h`
3. Recompile e upload

---

### 2️⃣ **Porta Incorreta** (20% de probabilidade)

**Verificar em `config.h`:**
```cpp
#define MQTT_PORT 1883
```

**Porta padrão MQTT:** `1883`
**MQTT com SSL:** `8883`

**Como testar:**
```bash
# Verificar qual porta MQTT está usando:
netstat -tulpn | grep mqtt
# ou
ss -tulpn | grep mqtt
```

**Solução:**
- Verificar qual porta o broker está usando
- Atualizar em `config.h`

---

### 3️⃣ **Firewall Bloqueando** (25% de probabilidade)

**No servidor Home Assistant:**
```bash
# Verificar se porta 1883 está aberta:
sudo ufw status
sudo ufw allow 1883

# Ou no Docker:
# Verificar se porta está mapeada
docker ps
docker inspect <mosquitto_container>
```

**No router/WiFi:**
- Verificar se há bloqueio entre dispositivos
- Alguns roteadores bloqueiam comunicação interna

---

### 4️⃣ **Broker MQTT Não Está Rodando** (10% de probabilidade)

**Verificar status:**
```bash
# Se Mosquitto instalado:
systemctl status mosquitto

# Se Docker:
docker ps | grep mosquitto
```

**Iniciar se necessário:**
```bash
# Mosquitto:
systemctl start mosquitto

# Home Assistant (reinicia tudo):
systemctl restart homeassistant
```

---

### 5️⃣ **WiFi Desconectado/Instável** (5% de probabilidade)

**Ver nos logs do ESP:**
```
[MQTT] Estado WiFi: ? (esperado 3)
[MQTT] SSID: ?
[MQTT] IP: ?
```

Se status WiFi ≠ 3, está desconectado.

---

## 🧪 Diagnóstico Passo a Passo:

### Passo 1: Verificar WiFi

```bash
# No monitor serial do ESP, procure:
[WIFI] Conectado: JANELA AMARELA, IP: 192.168.x.x
```

✅ Se ver, WiFi OK
❌ Se não ver, problema é WiFi, não MQTT

### Passo 2: Verificar IP do Servidor

```bash
# No servidor Home Assistant:
hostname -I
# Exemplo output:
# 10.0.0.84 (WiFi)
# 192.168.100.1 (Ethernet)
```

✅ Anote o IP correto
❌ Se não sabe qual usar, tente ambos

### Passo 3: Testar Conectividade

```bash
# De outro computador na mesma rede:
ping 10.0.0.84
telnet 10.0.0.84 1883
```

✅ Se ping responde, rede OK
✅ Se telnet abre, porta OK
❌ Se telnet falha, firewall ou broker não ativo

### Passo 4: Verificar Broker

```bash
# Verificar se está ativo:
systemctl status mosquitto

# Logs do Mosquitto:
sudo tail -f /var/log/mosquitto/mosquitto.log

# Ou se Docker:
docker logs -f mosquitto_container_name
```

### Passo 5: Testar Credenciais

```bash
# Testar conexão com mosquitto:
mosquitto_pub -h 10.0.0.84 -u mqtt_addon -P 1234 \
  -t "test" -m "teste"

# Se funcionar: credenciais OK ✓
# Se falhar: credenciais erradas ✗
```

---

## 📋 Checklist de Debug:

```
☐ WiFi conectado?
  └─ Procure: [WIFI] Conectado:

☐ IP local correto?
  └─ Procure: IP: 192.168.x.x ou 10.0.0.x

☐ Broker está ativo?
  └─ Teste: systemctl status mosquitto

☐ Porta 1883 aberta?
  └─ Teste: telnet <host> 1883

☐ Credenciais corretas?
  └─ Teste: mosquitto_pub -h <host> -u mqtt_addon -P 1234

☐ Config.h atualizado?
  └─ Host: 10.0.0.84
  └─ Porta: 1883
  └─ Usuário: mqtt_addon
  └─ Senha: 1234
```

---

## 🚀 Solução Rápida:

Se está com erro rc=-2:

### 1. No Servidor Home Assistant:

```bash
# Descobrir IP:
hostname -I

# Verificar broker ativo:
systemctl status mosquitto

# Se não ativo, iniciar:
systemctl start mosquitto
```

### 2. No ESP (em `config.h`):

```cpp
#define MQTT_HOST "10.0.0.84"    // ← ATUALIZAR com IP correto
#define MQTT_PORT 1883
#define MQTT_USER "mqtt_addon"
#define MQTT_PASS "1234"
```

### 3. Recompile:

```bash
platformio run --target upload
```

### 4. Verifique logs:

```bash
platformio device monitor -b 115200
```

Procure por: `[MQTT] ✓ Conectado com sucesso!`

---

## 🔧 Teste de Conectividade Rápido:

### Terminal 1: Monitor ESP
```bash
platformio device monitor -b 115200
```

### Terminal 2: Teste Mosquitto
```bash
# Ouvir tópico
mosquitto_sub -h 10.0.0.84 -u mqtt_addon -P 1234 \
  -t "addon/esp/esp_sensor_01/status"

# Se conectado, verá: online
```

---

## 📞 Se Ainda Não Funcionar:

### Verificar com este comando:

```bash
# Ver todas as informações de conectividade
echo "=== TESTE MQTT ==="
echo "1. Ping ao broker:"
ping -c 1 10.0.0.84
echo ""
echo "2. Telnet à porta:"
timeout 2 telnet 10.0.0.84 1883
echo ""
echo "3. Status do Mosquitto:"
systemctl status mosquitto
echo ""
echo "4. Mosquitto escutando:"
netstat -tulpn | grep mosquitto
```

---

## 💡 Dicas:

1. **Rede WiFi vs Ethernet:**
   - Se servidor em Ethernet: use IP Ethernet
   - Se servidor em WiFi: use IP WiFi

2. **Firewall do Router:**
   - Alguns roteadores bloqueiam entre WiFi/Ethernet
   - Verifique configurações do roteador

3. **Home Assistant em Docker:**
   - Verificar se porta 1883 está mapeada
   - `docker-compose.yml` deve ter `ports: "1883:1883"`

4. **Home Assistant em VM:**
   - Verificar networking da VM
   - Pode ser modo bridge vs NAT

---

## 🎯 Resultado Esperado:

Após aplicar a solução:

```
[MQTT] Host: 10.0.0.84
[MQTT] Porta: 1883
...
[MQTT] ═══════════════════════════════════
[MQTT] Tentando conectar em: 10.0.0.84:1883
[MQTT] IP Local: 192.168.100.50
[MQTT] ✓ Conectado com sucesso!
[MQTT] Broker: 10.0.0.84:1883
[TELEMETRY] ✓ {"temperature":25.3,...}
```

---

✅ **Pronto para debugar!**

