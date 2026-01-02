# 🚀 Quick Start - Testar Telemetria Agora

## ⏱️ 5 Minutos para Funcionar:

### 1. Upload Pronto ✅
Firmware já foi enviado para o ESP!

### 2. Abrir Monitor (30s)
```bash
platformio device monitor -b 115200
```

Procure por:
```
[MQTT] OK ✓
[TELEMETRY] ✓
```

### 3. Abrir outro Terminal (30s)
```bash
mosquitto_sub -h 10.0.0.84 -u mqtt_addon -P 1234 \
  -t "addon/esp/esp_sensor_01/telemetry"
```

### 4. Aguardar 5-10 Segundos ✅

Esperado:
```json
{"temperature":25.3,"uptime":5,"heap":41124,"rssi":-60}
{"temperature":28.1,"uptime":10,"heap":41108,"rssi":-60}
```

🎉 **PRONTO!** Telemetria funcionando!

---

## 🐛 Se Não Funcionar:

### Erro: Timeout
```
[MQTT] Timeout na conexão (rc=-4)
```
**Ação:** `telnet 10.0.0.84 1883`

### Erro: Usuário/Senha
```
[MQTT] Usuário/Senha incorretos (rc=4)
```
**Ação:** Verificar `config.h`

### Erro: Desconectado
```
[TELEMETRY] ✗ Não enviado - MQTT desconectado
```
**Ação:** Verificar se `[MQTT] OK ✓` aparece

---

## 📋 O Que Mudou:

| Item | Antes | Depois |
|------|-------|--------|
| Loop MQTT | Bloqueante ❌ | Não-bloqueante ✅ |
| Telemetria | Não funciona ❌ | Funciona ✅ |
| Debug | Genérico ❌ | Específico ✅ |

---

## 🎯 Checklist:

- [ ] Serial monitor mostrando logs
- [ ] `[MQTT] OK ✓` presente
- [ ] `[TELEMETRY] ✓` a cada 5s
- [ ] Broker MQTT recebendo dados
- [ ] Telemetria no topico esperado

---

## 📞 Arquivos Úteis:

- `TELEMETRY_SUMMARY.md` - Resumo técnico
- `TELEMETRY_TEST_GUIDE.md` - Teste completo
- `MQTT_TELEMETRY_DEBUG.md` - Debug detalhado

---

✅ **Tudo pronto para testar!**

