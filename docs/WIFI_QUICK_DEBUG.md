# 🚀 WiFi Debug - Ação Imediata

## ⚠️ Erro Recebido:

```
[MQTT] ✗ WiFi não conectado (status: 1)
```

**Status 1 = SSID não encontrado**

---

## 🎯 O Que Fazer Agora:

### 1. **Abrir Monitor Serial:**
```bash
platformio device monitor -b 115200
```

### 2. **Procurar por Este Log:**
```
[CONFIG] Carregando configuração de LittleFS...
[CONFIG] ✓ Configuração carregada:
[CONFIG]   SSID: '???' (len=...)
[CONFIG]   PASS: '???' (len=...)
```

### 3. **Anotar SSID e Senha Carregados**
- Verificar se estão corretos
- Verificar se não estão vazios

### 4. **Procurar por:**
```
[WiFi] Status: X
```

**Significado:**
- `0` = Procurando (normal)
- `1` = SSID não encontrado ❌
- `3` = Conectado ✅
- `4` = Falha na conexão

---

## 🔍 Se Status = 1 (SSID Não Encontrado):

**Causa Mais Provável:**
- SSID salvo está errado/vazio
- Roteador desligado
- Rede em 5GHz (ESP só suporta 2.4GHz)

**Ação:**
1. Verificar se SSID está em log
2. Reconectar ao portal (192.168.4.1)
3. Selecionar rede correta
4. Digitar senha correta
5. Upload novamente

---

## 🔍 Se Status = 4 (Falha na Conexão):

**Causa Mais Provável:**
- Senha incorreta

**Ação:**
1. Verificar senha no log
2. Reconectar ao portal (192.168.4.1)
3. Digitar senha com cuidado
4. Upload novamente

---

## ✅ Se Status = 3 (Conectado):

```
[WiFi] ✓ Conectado com sucesso!
[WiFi] SSID: JANELA AMARELA
[WiFi] IP: 192.168.100.50
[WiFi] RSSI: -60 dBm
```

Então o WiFi está OK! 🎉

Próximo: Testar MQTT

---

## 📱 Novo Firmware Inclui:

✅ Debug completo de config carregada
✅ Debug de cada tentativa WiFi
✅ Significado de cada status
✅ Sugestões de ação por erro

---

## 📞 Documento Completo:

Ver `WIFI_DEBUG_STATUS1.md` para mais detalhes e soluções.

