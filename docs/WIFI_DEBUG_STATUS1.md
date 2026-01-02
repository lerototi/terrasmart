# 🔍 Debug - WiFi Status 1 (WL_NO_SSID_AVAIL)

## 📊 Problema Identificado:

```
[MQTT] ✗ WiFi não conectado (status: 1)
```

**Status 1 = WL_NO_SSID_AVAIL = "SSID não disponível"**

---

## 🎯 Possíveis Causas:

### 1. **SSID Não Está Sendo Carregado Corretamente** (40%)
- Config está vazia
- JSON inválido em `/config.json`
- Caracteres especiais no SSID

### 2. **SSID Correto, Mas WiFi Não Encontra** (35%)
- Rede WiFi desligada
- ESP muito distante do roteador
- Roteador não está transmitindo SSID

### 3. **Senha Incorreta** (20%)
- Salvo errado no portal captivo
- Caracteres especiais na senha

### 4. **Problema de Hardware** (5%)
- Antena WiFi solta
- Problema com rádio do ESP8266

---

## 🧪 Como Diagnosticar:

### Novo Firmware Com Debug:

O firmware agora mostra logs detalhados:

```
[CONFIG] Carregando configuração de LittleFS...
[CONFIG] ✓ Configuração carregada:
[CONFIG]   SSID: 'JANELA AMARELA' (len=14)
[CONFIG]   PASS: 'senha123' (len=8)

╔════════════════════════════════════╗
║     CONECTANDO AO WiFi             ║
╚════════════════════════════════════╝
[WiFi] SSID: JANELA AMARELA
[WiFi] Senha: senha123
[WiFi] Comprimento SSID: 14
[WiFi] Comprimento Senha: 8
[WiFi] Iniciando conexão...
[WiFi] Status: 1 (SSID não encontrado)
[WiFi] Status: 1 (SSID não encontrado)
...
[ERROR] Timeout ao conectar WiFi (15s)
[WiFi] Possíveis causas:
  - SSID não existe
  - Senha incorreta
  - Sinal WiFi muito fraco
```

---

## ✅ Checklist:

### 1. **Verificar Config Carregada:**
```
Procure por logs de config:
[CONFIG] SSID: '...' (len=...)
[CONFIG] PASS: '...' (len=...)
```

✅ Se aparecer: config OK
❌ Se não aparecer: config vazia/corrompida

### 2. **Verificar SSID Correto:**
```bash
# Na mesma rede do ESP, liste redes:
# Windows: netsh wlan show networks
# Mac/Linux: iwlist scan | grep SSID
```

✅ Se encontrar "JANELA AMARELA": rede OK
❌ Se não encontrar: rede desligada ou muito fraca

### 3. **Verificar Senha:**
```bash
# Tente se conectar manualmente com essa senha
# A senha deve estar correta
```

### 4. **Verificar Distância:**
- Aproximar ESP do roteador WiFi
- Melhorar posição da antena

---

## 📋 Estados WiFi do ESP8266:

| Status | Nome | Significado |
|--------|------|-------------|
| 0 | WL_IDLE_STATUS | Procurando rede |
| 1 | WL_NO_SSID_AVAIL | SSID não encontrado ❌ |
| 2 | WL_SCAN_COMPLETED | Scan completo |
| 3 | WL_CONNECTED | Conectado ✅ |
| 4 | WL_CONNECT_FAILED | Falha na conexão |
| 5 | WL_CONNECTION_LOST | Conexão perdida |
| 6 | WL_DISCONNECTED | Desconectado |

---

## 🔧 Soluções por Causa:

### ❌ Se SSID está vazio:

```
[CONFIG] SSID: '' (len=0)
[ERROR] SSID vazio!
```

**Solução:**
1. Reconecte ao portal captivo (192.168.4.1)
2. Selecione a rede WiFi
3. Digite a senha
4. Clique conectar

### ❌ Se SSID não é encontrado:

```
[WiFi] Status: 1 (SSID não encontrado)
```

**Solução:**
1. Verificar se nome da rede está correto (case-sensitive)
2. Verificar se roteador está ligado
3. Verificar sinal WiFi (RSSI)
4. Aproximar ESP do roteador

### ❌ Se Senha incorreta:

ESP pode conectar se senha estiver errada, mas depois desconecta.
Logs mostrarão:
```
[WiFi] Status: 4 (Falha na conexão)
```

**Solução:**
1. Reconectar ao portal (192.168.4.1)
2. Verificar senha letra por letra
3. Cuidado com maiúsculas/minúsculas

---

## 🚀 Teste Completo:

### 1. Abrir Monitor:
```bash
platformio device monitor -b 115200
```

### 2. Procurar por Logs:

```
╔════════════════════════════════════╗
║        ESP INICIADO                ║
╚════════════════════════════════════╝

[BOOT] Config encontrada → modo operacional

[CONFIG] Carregando configuração de LittleFS...
[CONFIG] JSON bruto: {"ssid":"JANELA AMARELA","pass":"senha123"}
[CONFIG] ✓ Configuração carregada:
[CONFIG]   SSID: 'JANELA AMARELA' (len=14)
[CONFIG]   PASS: 'senha123' (len=8)

[BOOT] Tentando conectar ao WiFi: JANELA AMARELA

╔════════════════════════════════════╗
║     CONECTANDO AO WiFi             ║
╚════════════════════════════════════╝
[WiFi] SSID: JANELA AMARELA
[WiFi] Senha: senha123
[WiFi] Iniciando conexão...
[WiFi] Status: 0 (Idle) - Tentativa 1 (0.5s)
[WiFi] Status: 0 (Idle) - Tentativa 2 (1.0s)
[WiFi] Status: 3 (Conectado) - Tentativa 3 (1.5s)

[WiFi] ✓ Conectado com sucesso!
[WiFi] SSID: JANELA AMARELA
[WiFi] IP: 192.168.100.50
[WiFi] RSSI: -60 dBm
[WiFi] Gateway: 192.168.100.1
[WiFi] DNS: 192.168.100.1
```

---

## 📊 Análise dos Logs:

### ✅ Sucesso:
```
[WiFi] Status: 3 (Conectado)
[WiFi] ✓ Conectado com sucesso!
```

### ❌ SSID não encontrado:
```
[WiFi] Status: 1 (SSID não encontrado)
[ERROR] Timeout ao conectar WiFi (15s)
```
→ Verificar se SSID existe e está ligado

### ❌ Senha errada:
```
[WiFi] Status: 4 (Falha na conexão)
[ERROR] Timeout ao conectar WiFi (15s)
```
→ Reconectar ao portal com senha correta

---

## 🔄 Se Não Funcionar:

### 1. Reset Completo:

```bash
# Apagar toda config
# No monitor do ESP, enviar comando (se implementado):
# Ou manter o botão pressionado por 5s

# Manualmente (erase all):
platformio run --target erase
```

### 2. Reconectar ao Portal Captivo:

```bash
# O ESP entrará em AP mode se WiFi falhar
# Conecte novamente ao portal
http://192.168.4.1
```

### 3. Verificar Rede WiFi:

```bash
# Verificar que a rede está funcionando
# Conectar outro dispositivo
# Testar internet
```

---

## 💡 Dicas:

1. **Caracteres Especiais:**
   - SSID: "JANELA AMARELA" funciona bem
   - Evitar: `"`, `'`, `\`, `%` etc no SSID

2. **Senha com Caracteres:**
   - Se usar caracteres especiais, pode ter problema
   - Teste com senha simples primeiro

3. **Distância WiFi:**
   - ESP8266 tem antena fraca
   - Manter perto do roteador

4. **5GHz vs 2.4GHz:**
   - ESP8266 só suporta 2.4GHz
   - Verificar se roteador está em 2.4GHz

---

## 📞 Próximos Passos:

1. ✅ Fazer upload novo firmware
2. 📊 Monitorar logs no console serial
3. 🔍 Identificar qual é o problema
4. 🛠️ Aplicar solução apropriada
5. 🚀 Testar novamente

