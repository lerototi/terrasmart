# ✅ Próximos Passos - Debug de Conexão MQTT

## 🚀 O Que Fazer Agora:

Firmware já está no ESP com debug melhorado! 

### 1. **Abrir Monitor Serial** (Imediatamente)

```bash
platformio device monitor -b 115200
```

### 2. **Procure por estes Logs:**

```
╔════════════════════════════════════╗
║     CONFIGURAÇÃO MQTT              ║
╚════════════════════════════════════╝
[MQTT] Host: 10.0.0.84
[MQTT] Porta: 1883
[MQTT] Usuário: mqtt_addon
```

✅ Verifique se os dados estão corretos!

### 3. **Procure pela Tentativa de Conexão:**

```
[MQTT] ═══════════════════════════════════
[MQTT] Tentando conectar em: 10.0.0.84:1883
[MQTT] IP Local: 192.168.100.50
```

### 4. **Verifique o Resultado:**

- **✓ OK:** `[MQTT] ✓ Conectado com sucesso!`
- **✗ Erro:** `[MQTT] ✗ Falha na conexão - rc=-2`

---

## 🐛 Se Vir rc=-2:

### **CAUSA MAIS PROVÁVEL:** IP ou Porta incorretos

**O que fazer:**

1. **Descobrir IP real do servidor Home Assistant:**

```bash
# No computador com Home Assistant, execute:
hostname -I
```

2. **Verificar se está em `config.h`:**

```cpp
#define MQTT_HOST "10.0.0.84"  // Seu IP aqui
```

3. **Se diferente, atualizar e upload:**

```bash
# Editar config.h
# Salvar
# Então:
platformio run --target upload
```

---

## 🧪 Teste Rápido de Conectividade:

Sem alterar código, apenas teste se consegue pingar o servidor:

```bash
# De outro computador NA MESMA REDE do ESP:
ping 10.0.0.84

# Se responder: ✓ rede OK
# Se não responder: ✗ IP errado ou firewall
```

---

## 📊 Logs Esperados para Sucesso:

```
=== ESP iniciado ===
[BOOT] Config encontrada → modo operacional
[CONFIG] SSID: JANELA AMARELA
[WIFI] Conectado: JANELA AMARELA, IP: 192.168.100.50

╔════════════════════════════════════╗
║     CONFIGURAÇÃO MQTT              ║
╚════════════════════════════════════╝
[MQTT] Host: 10.0.0.84
[MQTT] Porta: 1883
[MQTT] Usuário: mqtt_addon

[MQTT] ═══════════════════════════════════
[MQTT] Tentando conectar em: 10.0.0.84:1883 (usuário: mqtt_addon)
[MQTT] IP Local: 192.168.100.50
[MQTT] ✓ Conectado com sucesso!
[MQTT] Broker: 10.0.0.84:1883
[MQTT] ✓ Inscrito em: addon/esp/esp_sensor_01/cmd

[TELEMETRY] ✓ {"temperature":25.3,"uptime":5,...}
```

---

## 🔍 Arquivos de Referência:

- `MQTT_DEBUG_AVANCADO.md` - Debug completo
- `MQTT_CONEXAO_DEBUG.md` - Diagnóstico e soluções
- `config.h` - Configurações MQTT
- `src/mqtt_manager.cpp` - Código de conexão

---

## 📋 Resumo das Mudanças:

✅ Adicionado logs detalhados na `setupMQTT()`
✅ Adicionado logs informativos na conexão
✅ Adicionado verificação de WiFi
✅ Adicionado função de teste `testMQTTConnection()`
✅ Mensagens de erro específicas por código de erro

---

## 🎯 Ação Imediata:

```bash
# 1. Monitorar:
platformio device monitor -b 115200

# 2. Verificar logs
# 3. Anotar IP/Porta do broker
# 4. Se diferente, atualizar config.h
# 5. Upload
# 6. Testar novamente
```

---

**Firmware pronto para debug! 🚀**

