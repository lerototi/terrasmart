# Como Testar Fase 2 - Portal Captivo Interativo

## 🚀 Quick Start

### 1. Upload do Firmware

```bash
# Faça upload para o ESP8266
platformio run -e wemos_d1_mini32 --target upload
```

### 2. Monitorar Serial

```bash
# Em outro terminal
platformio device monitor -b 115200
```

Você verá algo como:
```
[AP] SSID: terrasmart-setup
[AP] IP: 192.168.4.1
[AP] Conecte ao Wi-Fi 'terrasmart-setup'
[AP] Acesse: http://192.168.4.1
[AP] NOVO FLUXO: Step 1 (WiFi) -> Step 2 (MQTT) -> Step 3 (Confirmar)
```

---

## 📱 Acessar o Portal

### Em seu smartphone ou notebook:

1. **Procure por redes WiFi**
   - Nome: `terrasmart-setup`
   - Sem senha

2. **Conecte-se à rede**
   - WiFi aberto (não há autenticação)

3. **Abra um navegador**
   - Acesse: `http://192.168.4.1`
   - OU: `http://setup.local`

---

## 🌐 Step 1: Configurar WiFi

### Na página:
```
📡 Configurar WiFi
Selecione sua rede WiFi e digite a senha

[Progress: 1 ○ ○]

▼ Selecionar sua Rede WiFi:
  MyNetwork (-45dBm)
  OtherNet (-65dBm)

🔐 Senha do Wi-Fi:
  [••••••••••••]

[Recarregar]  [Testar e Continuar →]
```

### Ação do usuário:
1. Clique no dropdown para selecionar sua rede WiFi
2. Digite a senha corretamente
3. Clique **"Testar e Continuar →"**

### O que acontece no backend:
```
1. JavaScript faz POST /api/wifi/test
2. ESP8266 desconecta de "terrasmart-setup"
3. Tenta conectar com SSID + Senha fornecidos
4. Aguarda até 10 segundos (20 tentativas)
5. Se conectou: Retorna {"success": true}
6. Se falhou: Retorna {"success": false, "error": "..."}
```

### Se sucesso:
- ✅ Página mostra mensagem verde "WiFi testado com sucesso!"
- ✅ Após 1.5 segundos, navega para Step 2
- ✅ WiFi configurado e salvo no SetupManager

### Se falha:
- ❌ Mostra erro em vermelho
- ❌ Usuário pode tentar novamente ou voltar

---

## 🔐 Step 2: Configurar MQTT

### Na página:
```
📨 Configurar MQTT
Configure o servidor MQTT para telemetria

[Progress: ✓ 2 ○]

📨 Host MQTT:
  [mqtt.example.com]

🔌 Porta:
  [1883]

👤 Usuário (opcional):
  [myusername]

🔑 Senha (opcional):
  [••••••••••]

[← Voltar]  [Testar e Continuar →]
```

### Ação do usuário:

**Opção A: Use seu servidor MQTT real**
1. Digite Host MQTT (ex: mqtt.example.com)
2. Digite Porta (ex: 1883)
3. Digite Usuário (ex: myusername)
4. Digite Senha (ex: mypassword)
5. Clique **"Testar e Continuar →"**

**Opção B: Use valores padrão (sem servidor)**
1. Host: deixe em branco ou padrão
2. Porta: 1883
3. Usuário: deixe em branco
4. Senha: deixe em branco
5. Clique **"Testar e Continuar →"**

### O que acontece no backend:
```
1. JavaScript faz POST /api/mqtt/test
2. Backend valida:
   ├─ Host não vazio
   ├─ Porta entre 1-65535
   ├─ User e Pass opcionais
3. Se válido: Salva em SetupManager
4. Se inválido: Retorna erro
```

### Se sucesso:
- ✅ Mostra "MQTT testado com sucesso!"
- ✅ Após 1.5 segundos, navega para Step 3
- ✅ MQTT configurado e salvo

### Se falha:
- ❌ Mostra erro descritivo
- ❌ Pode voltar ou tentar novamente

---

## ✅ Step 3: Confirmar Configuração

### Na página:
```
✅ Confirmar Configuração
Verifique as configurações e finalize

[Progress: ✓ ✓ 3]

┌─────────────────────────────────┐
│ WiFi SSID:                      │
│ MyNetwork                       │
│                                 │
│ MQTT Host:                      │
│ mqtt.example.com                │
│                                 │
│ MQTT Port:                      │
│ 1883                            │
│                                 │
│ Status:                         │
│ Pronto para finalizar           │
└─────────────────────────────────┘

[← Voltar]  [Finalizar e Reiniciar ✓]

✅ Todas as configurações foram testadas
```

### Ação do usuário:
1. Verifique as informações exibidas
2. Clique **"Finalizar e Reiniciar ✓"**

### O que acontece no backend:
```
1. JavaScript faz GET /api/setup/summary (para carregar dados)
2. Página exibe resumo
3. Usuário clica "Finalizar"
4. JavaScript faz POST /api/setup/complete
5. Backend:
   ├─ Marca SetupManager como SETUP_COMPLETE
   ├─ Aguarda 2 segundos
   ├─ Chama ESP.restart()
6. ESP8266 reinicia
7. AP desliga automaticamente
8. Dispositivo conecta ao WiFi em modo operacional
```

### Resultado:
- ✅ Página exibe "Sucesso! Seu dispositivo será reiniciado..."
- ✅ ESP8266 reinicia
- ✅ Sai do modo AP
- ✅ Conecta ao WiFi real
- ✅ **Setup completo!**

---

## 📊 Monitorando no Serial

Abra o serial monitor e veja as mensagens:

```
[API] GET /api/scan
[API] Encontradas 3 redes

[API] POST /api/wifi/test - Payload: {"ssid":"MyNetwork","pass":"password"}
[API] Testando WiFi - SSID: MyNetwork
[API] WiFi conectado com sucesso!

[API] POST /api/mqtt/test - Payload: {"host":"mqtt.example.com","port":1883,...}
[API] Testando MQTT - Host: mqtt.example.com:1883

[API] GET /api/setup/summary
[API] POST /api/setup/complete
[SETUP] Setup marcado como completo!
```

---

## 🔧 Troubleshooting

### Problema: "Nenhuma rede WiFi encontrada"
- **Causa**: Nenhuma rede disponível durante scan
- **Solução**: Clique "Recarregar" para tentar novamente
- **Backend**: `GET /api/scan` retorna lista vazia

### Problema: "Falha ao conectar WiFi"
- **Causa**: SSID ou senha incorretos
- **Solução**: Volte para Step 1 e tente novamente
- **Backend**: WiFi.begin() falhou após 10 segundos

### Problema: "Porta deve estar entre 1-65535"
- **Causa**: Número de porta inválido
- **Solução**: Digite um número de 1 a 65535
- **Backend**: Validação `if (port < 1 || port > 65535)`

### Problema: "Erro ao testar MQTT"
- **Causa**: Servidor MQTT não respondendo (normal em testes)
- **Solução**: Use valores padrão ou deixe em branco
- **Backend**: Validação apenas (sem conexão real ainda)

### Problema: Página não carrega
- **Causa**: Pode estar fora do alcance do WiFi
- **Solução**: Reconecte ao `terrasmart-setup` e tente `http://192.168.4.1`
- **Backend**: DNS redirect para 192.168.4.1

---

## 💾 Dados Salvos

Após completar os 3 steps:

```cpp
// SetupManager em memória:
SetupConfig {
  wifiSsid: "MyNetwork"
  wifiPass: "password"
  wifiConfigured: true
  
  mqttHost: "mqtt.example.com"
  mqttPort: 1883
  mqttUser: "myuser"
  mqttPass: "mypass"
  mqttConfigured: true
  
  currentSetupState: SETUP_COMPLETE
  currentOpState: OPERATIONAL_NORMAL
}
```

**Nota**: Os dados estão em RAM durante toda sessão de AP. Na Fase 4, serão salvos em LittleFS.

---

## 📐 Arquitetura de Requisições

```
Browser (Mobile/PC)
  ↓
WiFi: terrasmart-setup (AP)
  ↓
  ├─ GET http://192.168.4.1/
  ├─ GET http://192.168.4.1/step/wifi
  ├─ GET http://192.168.4.1/api/scan
  ├─ POST http://192.168.4.1/api/wifi/test
  ├─ GET http://192.168.4.1/step/mqtt
  ├─ POST http://192.168.4.1/api/mqtt/test
  ├─ GET http://192.168.4.1/step/confirm
  ├─ GET http://192.168.4.1/api/setup/summary
  └─ POST http://192.168.4.1/api/setup/complete
  ↓
ESP8266WebServer
  ├─ Handlers para rotas
  ├─ Parsing de JSON
  ├─ Lógica de teste WiFi/MQTT
  ├─ SetupManager.save*()
  └─ ESP.restart()
```

---

## 🎯 Checklist de Teste Completo

- [ ] ESP8266 faz boot com AP ligado
- [ ] "terrasmart-setup" aparece em WiFi disponível
- [ ] Consegue conectar ao AP
- [ ] http://192.168.4.1 carrega página Step 1
- [ ] GET /api/scan lista redes
- [ ] Seleciona rede e digita senha
- [ ] "Testar e Continuar" testa WiFi real
- [ ] Navega para Step 2
- [ ] Preenche dados MQTT
- [ ] "Testar e Continuar" valida e salva
- [ ] Navega para Step 3
- [ ] Mostra resumo com dados corretos
- [ ] "Finalizar" chama /api/setup/complete
- [ ] Página mostra "Sucesso"
- [ ] ESP8266 reinicia
- [ ] AP desliga
- [ ] Serial mostra mensagens corretas

---

## 📚 Documentação Adicional

- `docs/PHASE2_IMPLEMENTATION.md` - Documentação técnica completa
- `src/captive/captive_portal.cpp` - Código dos handlers
- `include/setup_manager.h` - API de SetupManager
- `docs/SETUP_FLOW.md` - Diagrama geral de fluxo

---

## ✨ Pronto para Fase 3?

Após confirmar que tudo funciona, podemos começar **Fase 3: Integração Main Loop**.

Isso envolverá integrar o SetupManager ao main.cpp e criar:
- Boot flow automático
- Reconexão com retry
- Fallback ao AP

