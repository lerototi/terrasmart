# 🎯 Guia de Teste - Telemetria MQTT

## 📋 Checklist Pré-Teste:

- [ ] ESP está conectado ao Wi-Fi
- [ ] Broker MQTT está ativo (10.0.0.84:1883)
- [ ] Credenciais MQTT corretas (mqtt_addon / 1234)
- [ ] Firewall permite porta 1883
- [ ] Firmware foi feito upload

---

## 🚀 Teste Prático:

### Passo 1: Iniciar Monitor Serial

```bash
cd /home/leroto/Documents/PlatformIO/Projects/terraSmart_device
platformio device monitor -b 115200
```

### Passo 2: Reiniciar ESP

Desconecte/reconecte o USB ou reset do ESP.

### Passo 3: Observar Inicialização

Procure por este padrão:

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
```

### Passo 4: Observar Telemetria

A cada 5 segundos:

```
[TELEMETRY] ✓ {"temperature":25.3,"uptime":5,"heap":41124,"rssi":-60}
[TELEMETRY] ✓ {"temperature":28.1,"uptime":10,"heap":41108,"rssi":-60}
[TELEMETRY] ✓ {"temperature":22.7,"uptime":15,"heap":41092,"rssi":-60}
```

---

## 📡 Teste no Broker MQTT:

### Terminal 2: Ouvir Telemetria

```bash
mosquitto_sub -h 10.0.0.84 -u mqtt_addon -P 1234 \
  -t "addon/esp/esp_sensor_01/telemetry" -v
```

Esperado:
```
addon/esp/esp_sensor_01/telemetry {"temperature":25.3,"uptime":5,"heap":41124,"rssi":-60}
addon/esp/esp_sensor_01/telemetry {"temperature":28.1,"uptime":10,"heap":41108,"rssi":-60}
```

### Terminal 3: Ouvir Status

```bash
mosquitto_sub -h 10.0.0.84 -u mqtt_addon -P 1234 \
  -t "addon/esp/esp_sensor_01/status" -v
```

Esperado:
```
addon/esp/esp_sensor_01/status online
```

### Terminal 4: Ouvir Todos os Tópicos

```bash
mosquitto_sub -h 10.0.0.84 -u mqtt_addon -P 1234 \
  -t "addon/esp/esp_sensor_01/#" -v
```

Esperado:
```
addon/esp/esp_sensor_01/status online
addon/esp/esp_sensor_01/telemetry {"temperature":25.3,...}
addon/esp/esp_sensor_01/telemetry {"temperature":28.1,...}
```

---

## 🎯 Teste Interativo:

### 1. Enviar Comando

```bash
mosquitto_pub -h 10.0.0.84 -u mqtt_addon -P 1234 \
  -t "addon/esp/esp_sensor_01/cmd" \
  -m '{"action":"led","value":1}'
```

Esperado no ESP:
```
[COMMAND] Recebido comando JSON
[COMMAND] Action: led
[COMMAND] Value: 1
```

### 2. Verificar Resposta

```bash
mosquitto_sub -h 10.0.0.84 -u mqtt_addon -P 1234 \
  -t "addon/esp/esp_sensor_01/resp"
```

---

## 📊 Análise de Dados:

### Temperatura

```json
"temperature": 25.3
```

- Valor simulado entre 20.0 e 32.0°C
- Atualizado a cada 5 segundos

### Uptime

```json
"uptime": 5
```

- Tempo de operação em segundos
- Aumenta constantemente

### Heap (Memória Livre)

```json
"heap": 41124
```

- Memória RAM livre em bytes
- Se diminuir muito, pode indicar vazamento

### RSSI (Força do Sinal)

```json
"rssi": -60
```

- Valor em dBm (decibéis)
- `-30 a -50`: Excelente
- `-50 a -70`: Bom
- `-70 a -80`: Aceitável
- `-80 a -90`: Fraco
- `< -90`: Muito Fraco

---

## 🐛 Troubleshooting:

### ❌ "MQTT desconectado"

```
[TELEMETRY] ✗ Não enviado - MQTT desconectado
```

**Solução:**
1. Verificar broker está ativo
2. Verificar conectividade: `ping 10.0.0.84`
3. Verificar credenciais em `config.h`
4. Ver logs detalhados de erro

### ❌ "Timeout na conexão"

```
[MQTT] Timeout na conexão (rc=-4)
```

**Solução:**
1. Broker não respondendo
2. Firewall bloqueando porta 1883
3. Rede instável

### ❌ "Usuário/Senha incorretos"

```
[MQTT] Usuário/Senha incorretos (rc=4)
```

**Solução:**
1. Verificar `config.h`:
   - MQTT_USER = "mqtt_addon"
   - MQTT_PASS = "1234"
2. Verificar no broker MQTT

### ❌ "Servidor MQTT indisponível"

```
[MQTT] Servidor MQTT indisponível (rc=3)
```

**Solução:**
1. Reiniciar broker MQTT
2. Verificar logs do broker

### ❌ Telemetria Não Aparece

**Verificar em ordem:**

1. ✅ ESP conectado ao Wi-Fi?
   ```
   [WIFI] Conectado:
   ```

2. ✅ MQTT conectado?
   ```
   [MQTT] OK ✓
   ```

3. ✅ Telemetria sendo enviada?
   ```
   [TELEMETRY] ✓
   ```

4. ✅ Broker recebendo?
   ```bash
   mosquitto_sub ... (esperar mensagens)
   ```

---

## 📈 Performance:

### Esperado:

- Telemetria a cada 5 segundos
- Heap estável (não crescer)
- RSSI estável (não flutuar muito)
- CPU baixo (<50%)

### Monitorar:

```bash
# Ver heap ao longo do tempo
mosquitto_sub -h 10.0.0.84 -u mqtt_addon -P 1234 \
  -t "addon/esp/esp_sensor_01/telemetry" \
  -C 10  # Últimas 10 mensagens
```

---

## 🔄 Teste de Reconexão:

### 1. Desligar Broker MQTT

```bash
sudo systemctl stop mosquitto
```

Esperado no ESP:
```
[MQTT] Timeout na conexão (rc=-4)
[MQTT] Conectando...
[TELEMETRY] ✗ Não enviado - MQTT desconectado
```

### 2. Ligar Broker MQTT

```bash
sudo systemctl start mosquitto
```

Esperado no ESP:
```
[MQTT] Conectando...
[MQTT] OK ✓
[MQTT] Conectado ao broker: 10.0.0.84:1883
[TELEMETRY] ✓ {...}
```

---

## 📊 Análise de Logs:

### Sucesso Completo:

```
=== ESP iniciado ===
[BOOT] Config encontrada → modo operacional
[CONFIG] SSID: JANELA AMARELA
[WIFI] Conectado: JANELA AMARELA, IP: 192.168.100.50
[MQTT] Configurado
[MQTT] Conectando...
[MQTT] OK ✓
[MQTT] Conectado ao broker: 10.0.0.84:1883
[MQTT] Inscrito em: addon/esp/esp_sensor_01/cmd
[TELEMETRY] ✓ {"temperature":25.3,"uptime":5,"heap":41124,"rssi":-60}
[TELEMETRY] ✓ {"temperature":28.1,"uptime":10,"heap":41108,"rssi":-60}
```

### Problema: MQTT Desconectado

```
[MQTT] Conectando...
[MQTT] Timeout na conexão (rc=-4)
[MQTT] Conectando...
[TELEMETRY] ✗ Não enviado - MQTT desconectado
```

**Ação:** Verificar broker MQTT

### Problema: Credenciais

```
[MQTT] Conectando...
[MQTT] Usuário/Senha incorretos (rc=4)
[MQTT] Conectando...
[MQTT] Usuário/Senha incorretos (rc=4)
```

**Ação:** Verificar config.h

---

## ✅ Checklist Final:

- [ ] Monitor mostra logs esperados
- [ ] Telemetria é recebida no broker
- [ ] Dados estão corretos
- [ ] Heap não cresce indefinidamente
- [ ] Reconexão funciona após falha
- [ ] Status online está correto

---

## 🎉 Tudo Funcionando?

Se sim, você tem:

✅ Portal Captivo funcional
✅ Configuração salva
✅ Wi-Fi conectando
✅ MQTT conectando
✅ Telemetria enviando dados

**Parabéns!** 🚀

