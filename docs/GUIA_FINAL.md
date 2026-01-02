# 🎉 Portal Captivo Completo - Guia Final

## ✨ Status: PRONTO PARA TESTE COMPLETO ✨

---

## 📋 Funcionalidades Implementadas:

### ✅ Modo AP com Portal Captivo
- Ativa automaticamente quando não há configuração salva
- IP: `192.168.4.1`
- SSID: `terrasmart-setup`

### ✅ Scan Automático de Redes
- Endpoint: `GET /scan`
- Retorna JSON com lista de redes
- Atualiza a cada 30 segundos automaticamente
- JavaScript front-end faz fetch

### ✅ Seletor de Redes
- Dropdown com redes encontradas
- Mostra força de sinal (RSSI em dBm)
- Interface responsiva e amigável

### ✅ Configuração Salva
- Salva em LittleFS
- Arquivo: `/config.json`
- Reinicia em modo operacional

### ✅ Modo Operacional
- Conecta ao Wi-Fi salvo
- Inicia MQTT
- Envia telemetria

---

## 🚀 Fluxo Completo de Uso:

```
1. ESP inicia sem configuração
   └─> Modo AP + Portal Captivo ativado

2. Usuário conecta ao Wi-Fi "terrasmart-setup"
   └─> Recebe IP 192.168.4.x

3. Acessa http://192.168.4.1
   └─> Página carrega HTML + CSS + JavaScript

4. JavaScript executa fetch('/scan')
   └─> ESP faz WiFi.scanNetworks()
   └─> Retorna JSON com redes

5. Usuário seleciona rede e digita senha
   └─> POST /save

6. ESP salva em LittleFS
   └─> Reinicia

7. ESP inicia com configuração
   └─> Modo operacional
   └─> Conecta ao Wi-Fi
   └─> Inicia MQTT
```

---

## 📊 Arquivos Afetados:

### `src/captive/captive_portal.cpp`
```
✅ HTML completo com CSS e JavaScript
✅ Função handlePortal() - serve HTML
✅ Função handleScan() - faz scan de redes
✅ Função handleSave() - salva configuração
✅ Função startCaptivePortal() - inicia AP
✅ Função captiveLoop() - processa requisições
```

### `src/main.cpp`
```
✅ Verifica hasValidConfig()
✅ Ativa AP mode se não tiver config
✅ Conecta ao Wi-Fi se tiver config
✅ Loop separado para modo AP
```

### `src/config/config_manager.cpp`
```
✅ loadConfig() - carrega do LittleFS
✅ saveConfig() - salva em LittleFS
✅ hasValidConfig() - verifica existência
```

---

## 🧪 Como Testar:

### Passo 1: Verificar o Upload
```bash
cd /home/leroto/Documents/PlatformIO/Projects/terraSmart_device
platformio run --target upload
```
✅ Esperado: `[SUCCESS] Took ...`

### Passo 2: Monitore os Logs
```bash
platformio device monitor -b 115200
```

### Passo 3: Procure por estes logs:
```
=== ESP iniciado ===
[BOOT] Sem config → modo AP + Portal Captivo
╔════════════════════════════════════╗
║     MODO AP - PORTAL CAPTIVO       ║
╚════════════════════════════════════╝
[AP] SSID: terrasmart-setup
[AP] IP: 192.168.4.1
```

### Passo 4: Conecte ao Wi-Fi
1. No seu celular/PC, procure pela rede `terrasmart-setup`
2. Conecte (sem senha)
3. Abra navegador
4. Acesse `http://192.168.4.1`

### Passo 5: Use o Portal
1. Aguarde o scan das redes (pode levar 5-10 segundos)
2. Selecione sua rede no dropdown
3. Digite a senha
4. Clique "Conectar"
5. Observe o log: `[CAPTIVE] Config salva: SSID=...`
6. ESP reinicia

### Passo 6: Modo Operacional
```
[BOOT] Config encontrada → modo operacional
[CONFIG] SSID: SuaRede
[WIFI] Conectado: SuaRede, IP: 192.168.x.x
```

---

## 📱 O que você verá no Navegador:

### 1️⃣ Abrindo a página:
```
🌐 TerraSmart Setup
Configure seu dispositivo para conectar à Internet

[Spinner] Escaneando redes...
```

### 2️⃣ Redes Carregadas:
```
📡 Selecione sua Rede Wi-Fi:
┌─────────────────────────┐
│ MinhaRede (-45 dBm)  ▼ │
│ OutraRede (-72 dBm)    │
│ TerceiraRede (-85 dBm) │
└─────────────────────────┘

🔐 Senha do Wi-Fi:
┌─────────────────────────┐
│ [Digitar senha]         │
└─────────────────────────┘

[Conectar]
```

### 3️⃣ Após clicar Conectar:
```
✓ Configuração Salva!
Reiniciando o dispositivo...
Conecte-se à sua rede Wi-Fi
```

---

## 🔍 Monitoramento em Tempo Real:

Ao conectar e usar o portal:

```
[HTTP] Requisição: /
[HTTP] Client IP: 192.168.4.2
[SCAN] Iniciando varredura de redes Wi-Fi...
[SCAN] 1. MinhaRede (-45 dBm)
[SCAN] 2. OutraRede (-72 dBm)
[SCAN] Total de redes encontradas: 2
[HTTP] Requisição: /save
[CAPTIVE] Config salva: SSID=MinhaRede
```

---

## ⚠️ Troubleshooting:

### ❌ Erro: "ERR_CONNECTION_ABORTED"
- [ ] Verificar se está conectado ao `terrasmart-setup`
- [ ] Aguardar o scan completar
- [ ] Tentar acessar em navegador diferente
- [ ] Limpar cache do navegador

### ❌ Erro: "Nenhuma rede encontrada"
- [ ] Aguardar 10 segundos
- [ ] A página atualiza automaticamente
- [ ] Se persistir, pode estar fora do alcance de redes

### ❌ Página não abre
- [ ] Verificar IP: 192.168.4.1
- [ ] Tentar: http://setup.local
- [ ] Reiniciar o roteador AP do ESP

### ❌ Config não salva
- [ ] Verificar logs: `[CAPTIVE] Config salva:`
- [ ] Verificar LittleFS: `/config.json`
- [ ] Apagar arquivo `/config.json` e tentar novamente

### ❌ ESP não conecta ao Wi-Fi
- [ ] Verificar senha (case-sensitive)
- [ ] Verificar SSID correto
- [ ] Tentar rede em 2.4GHz (não 5GHz)
- [ ] Aproximar do roteador

---

## 📊 Estrutura JSON do Scan:

```json
{
  "networks": [
    {
      "ssid": "MinhaRede",
      "rssi": -45,
      "sec": 2
    },
    {
      "ssid": "OutraRede",
      "rssi": -72,
      "sec": 2
    }
  ]
}
```

### Legenda:
- **ssid**: Nome da rede
- **rssi**: Força do sinal em dBm
  - `-30 a -50`: Excelente
  - `-50 a -70`: Bom
  - `-70 a -80`: Aceitável
  - `-80 a -90`: Fraco
- **sec**: Tipo de segurança
  - `0`: Open (aberto)
  - `1`: WEP
  - `2`: WPA/WPA2
  - `3`: WPA2/WPA3
  - `4`: WPA3

---

## 🎯 Próximas Melhorias (Optional):

- [ ] Suporte para redes ocultas (hidden SSID)
- [ ] Ícone visual de força de sinal
- [ ] Salvar última rede conectada
- [ ] Timeout inteligente de scan
- [ ] Suporte a múltiplas configurações
- [ ] Reset de configuração
- [ ] Informações do dispositivo

---

## 📝 Resumo Técnico:

| Componente | Detalhes |
|-----------|----------|
| **AP SSID** | `terrasmart-setup` |
| **IP** | `192.168.4.1` |
| **Gateway** | `192.168.4.1` |
| **Subnet** | `255.255.255.0` |
| **DNS Port** | `53` |
| **HTTP Port** | `80` |
| **Scan Interval** | `30 segundos` |
| **Storage** | `LittleFS (/config.json)` |
| **Board** | `ESP8266 (D1 Mini)` |

---

## 🚀 Comandos Úteis:

```bash
# Compilar
platformio run

# Upload
platformio run --target upload

# Monitor
platformio device monitor -b 115200

# Limpar cache
platformio run --target clean

# Build verbose
platformio run --verbose
```

---

## ✅ Checklist de Verificação:

- [ ] Código compila sem erros
- [ ] Upload realizado com sucesso
- [ ] ESP inicia em modo AP sem config
- [ ] Portal captivo abre em http://192.168.4.1
- [ ] Scan de redes funciona
- [ ] Lista de redes aparece
- [ ] Pode selecionar rede
- [ ] Pode digitar senha
- [ ] Pode clicar Conectar
- [ ] Config é salva
- [ ] ESP reinicia
- [ ] ESP conecta ao Wi-Fi
- [ ] Modo operacional inicia
- [ ] MQTT conecta (se configurado)

---

## 📞 Suporte:

Se algo não funcionar:

1. ✅ Verificar logs do ESP
2. ✅ Procurar por erros de compilação
3. ✅ Verificar conexão USB
4. ✅ Tentar reset do dispositivo
5. ✅ Limpar LittleFS (apagar /config.json)

Boa sorte! 🍀

