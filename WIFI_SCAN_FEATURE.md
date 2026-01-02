# 📡 Portal Captivo - Listagem de Redes Wi-Fi

## ✨ Novas Funcionalidades:

### 1. **Scan Automático de Redes**
- Ao abrir a página, o ESP faz um scan automático das redes disponíveis
- A lista é atualizada a cada 30 segundos
- Mostra o sinal (RSSI) de cada rede

### 2. **Interface Moderna**
- Seletor de redes em vez de input de texto
- Design responsivo e profissional
- Gradiente de cores (roxo/azul)
- Indicador de carregamento durante o scan

### 3. **Rota JSON para Scan**
- Endpoint: `GET /scan`
- Retorna JSON com lista de redes:
```json
{
  "networks": [
    {"ssid": "MinhaRede", "rssi": -45, "sec": 2},
    {"ssid": "OutraRede", "rssi": -72, "sec": 2}
  ]
}
```

### 4. **Logs Detalhados**
```
[SCAN] Iniciando varredura de redes Wi-Fi...
[SCAN] 1. MinhaRede (-45 dBm)
[SCAN] 2. OutraRede (-72 dBm)
[SCAN] Total de redes encontradas: 2
```

---

## 🎯 Fluxo de Uso:

1. Usuário acessa `http://192.168.4.1`
2. JavaScript carrega e faz requisição para `/scan`
3. ESP8266 faz scan de redes e retorna JSON
4. Interface popula o seletor `<select>` com as redes
5. Usuário seleciona a rede desejada
6. Usuário digita a senha
7. Clica em "Conectar"
8. Enviando para `/save` via POST
9. ESP salva e reinicia em modo operacional

---

## 📊 Resposta do Endpoint `/scan`:

### Formato JSON:
```json
{
  "networks": [
    {
      "ssid": "Nome da Rede",
      "rssi": -50,
      "sec": 2
    }
  ]
}
```

### Valores de Segurança (sec):
- `0` - Open (Aberta)
- `1` - WEP
- `2` - WPA/WPA2
- `3` - WPA2/WPA3
- `4` - WPA3

### Valores de RSSI (força do sinal):
- `-30` a `-50` - Excelente
- `-50` a `-70` - Bom
- `-70` a `-80` - Aceitável
- `-80` a `-90` - Fraco
- `< -90` - Muito Fraco

---

## 🔄 JavaScript Automático:

O JavaScript na página:
1. ✅ Carrega redes ao abrir
2. ✅ Atualiza a cada 30 segundos
3. ✅ Mostra spinner de carregamento
4. ✅ Trata erros
5. ✅ Seleciona primeira rede automaticamente

---

## 💾 Código da Função de Scan:

```cpp
void handleScan() {
  Serial.println("[SCAN] Iniciando varredura de redes Wi-Fi...");
  
  // Fazer scan de redes
  int n = WiFi.scanNetworks();
  
  String json = "{\"networks\":[";
  
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      int sec = WiFi.encryptionType(i);
      
      // Escapar aspas no SSID
      ssid.replace("\"", "\\\"");
      
      json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(rssi) + ",\"sec\":" + String(sec) + "}";
      
      Serial.printf("[SCAN] %d. %s (%d dBm)\n", i+1, WiFi.SSID(i).c_str(), rssi);
    }
  }
  
  json += "]}";
  
  server.send(200, "application/json", json);
  Serial.printf("[SCAN] Total de redes encontradas: %d\n", n);
}
```

---

## 🧪 Como Testar:

1. **Abra o portal:**
   ```
   http://192.168.4.1
   ```

2. **Observe o console do navegador:**
   - Abra DevTools (F12)
   - Vá para Network
   - Procure pela requisição `/scan`
   - Verifique a resposta JSON

3. **Verifique os logs do ESP:**
   ```bash
   platformio device monitor -b 115200
   ```

4. **Selecione a rede e teste:**
   - Escolha sua rede Wi-Fi na lista
   - Digitar a senha
   - Clique "Conectar"
   - O ESP deve reiniciar e conectar

---

## ⚠️ Possíveis Problemas:

### ❌ "Nenhuma rede encontrada"
- O scan pode levar alguns segundos
- Espere 5-10 segundos
- A página atualiza automaticamente

### ❌ Redes aparecem, mas com sinal muito fraco
- Pode ser problema de RF do AP do ESP
- Aproxime-se do dispositivo

### ❌ SSID não aparece na lista
- Pode estar escondido (hidden SSID)
- Será necessário digitar manualmente (future improvement)

---

## 🚀 Melhorias Futuras:

1. ✏️ Suporte para redes ocultas (hidden SSID)
2. 🔒 Mostrar ícone de cadeado para redes seguras
3. 📶 Barra de força de sinal visual
4. 💾 Lembrar última rede conectada
5. ⏱️ Timeout de carregamento
6. 🌐 Suporte a 5GHz vs 2.4GHz

---

## 📝 Notas:

- O scan consome mais memória e CPU
- Portanto, é feito apenas quando solicitado (sob demanda)
- A página mantém a lista em cache por 30 segundos
- Sem necessidade de rescan a cada requisição

