# Suporte ao Campo Trigger

## 📋 Visão Geral

O add-on agora suporta o campo `trigger` no payload de telemetria, permitindo identificar o motivo da atualização enviada pelo dispositivo ESP.

## 🎯 Valores de Trigger

O campo `trigger` pode ter os seguintes valores:

- **`heartbeat`**: Atualização periódica de rotina (keep-alive)
- **`change_detected`**: Atualização enviada porque houve mudança significativa nos valores

## 📦 Estrutura do Payload

```json
{
  "device": {
    "id": "esp_sensor_01",
    "type": "DHT22 Temperature & Humidity Sensor",
    "uptime": 3245,
    "heap": 40496,
    "rssi": -67,
    "ip": "10.0.0.84"
  },
  "readings": [
    {"type": "temperature", "value": 24.3, "unit": "°C"},
    {"type": "humidity", "value": 65.0, "unit": "%"}
  ],
  "timestamp": "2026-01-08T14:24:28-03:00",
  "trigger": "change_detected"
}
```

## 🔄 Como Funciona

### 1. **Recepção do Payload**
O `telemetryHandler` recebe o payload e extrai o campo `trigger`.

### 2. **Processamento**
- Valida se o campo `trigger` está presente
- Atualiza o registry com o último trigger e timestamp
- Registra o tipo de sensor "trigger" (primeira vez)

### 3. **MQTT Discovery**
- Na primeira vez que recebe um trigger, publica a configuração de discovery
- Sensor aparece no Home Assistant como "Tipo de Atualização"
- Ícone: `mdi:update`

### 4. **Publicação**
- Publica o valor do trigger no tópico: `addon/esp/{device_id}/trigger`
- Valor: `heartbeat` ou `change_detected`

### 5. **Dashboard**
- Sensor disponível no Home Assistant
- API REST expõe informações em `/devices`

## 📡 Tópicos MQTT

### Entrada (ESP → Add-on)
```
addon/esp/{device_id}/telemetry
```

### Saída (Add-on → Home Assistant)
```
addon/esp/{device_id}/trigger          # Valor do trigger
homeassistant/sensor/.../config         # MQTT Discovery
```

## 🏠 Home Assistant

### Sensor Criado

Um novo sensor será criado automaticamente no Home Assistant:

**Nome:** `esp_sensor_01 Tipo de Atualização`  
**Entity ID:** `sensor.esp_sensor_01_tipo_de_atualizacao`  
**Valores possíveis:**
- `heartbeat`
- `change_detected`

### Automação de Exemplo

```yaml
automation:
  - alias: "Notificar Mudanças Detectadas"
    trigger:
      - platform: state
        entity_id: sensor.esp_sensor_01_tipo_de_atualizacao
        to: "change_detected"
    action:
      - service: notify.mobile_app
        data:
          title: "Sensor Atualizado"
          message: "Mudança detectada no ESP Sensor 01"
```

### Card no Dashboard

```yaml
type: entities
entities:
  - entity: sensor.esp_sensor_01_temperatura
  - entity: sensor.esp_sensor_01_umidade
  - entity: sensor.esp_sensor_01_tipo_de_atualizacao
    name: Última Atualização
    icon: mdi:update
```

## 🌐 API HTTP

### Endpoint: GET /devices

Agora retorna informações sobre o trigger:

```json
{
  "devices": [
    {
      "id": "esp_sensor_01",
      "info": {
        "type": "DHT22 Temperature & Humidity Sensor",
        "uptime": 3245,
        "heap": 40496,
        "rssi": -67,
        "ip": "10.0.0.84"
      },
      "lastSeen": 1704729868000,
      "status": "online",
      "sensorTypes": ["temperature", "humidity", "trigger"],
      "lastTrigger": "change_detected",
      "lastTriggerTime": 1704729868000
    }
  ]
}
```

## 💡 Casos de Uso

### 1. **Otimização de Energia**
Dispositivos podem enviar:
- `heartbeat`: A cada 5 minutos (keep-alive)
- `change_detected`: Apenas quando valores mudam significativamente

### 2. **Diagnóstico**
Identificar se o dispositivo está:
- Enviando apenas heartbeats → Valores estáveis
- Enviando change_detected → Ambiente com variações

### 3. **Alertas Inteligentes**
Criar automações que respondem apenas a `change_detected`, ignorando heartbeats.

### 4. **Análise de Padrões**
Visualizar quando houve mudanças reais vs. atualizações periódicas.

## 🔧 Implementação no ESP

### Exemplo de Código (Arduino/ESP)

```cpp
void sendTelemetry(bool isChangeDetected) {
  StaticJsonDocument<512> doc;
  
  // Device info
  doc["device"]["id"] = "esp_sensor_01";
  doc["device"]["type"] = "DHT22 Temperature & Humidity Sensor";
  doc["device"]["uptime"] = millis() / 1000;
  doc["device"]["heap"] = ESP.getFreeHeap();
  doc["device"]["rssi"] = WiFi.RSSI();
  doc["device"]["ip"] = WiFi.localIP().toString();
  
  // Readings
  JsonArray readings = doc.createNestedArray("readings");
  JsonObject temp = readings.createNestedObject();
  temp["type"] = "temperature";
  temp["value"] = temperature;
  temp["unit"] = "°C";
  
  JsonObject hum = readings.createNestedObject();
  hum["type"] = "humidity";
  hum["value"] = humidity;
  hum["unit"] = "%";
  
  // Timestamp
  doc["timestamp"] = getISOTimestamp();
  
  // Trigger
  doc["trigger"] = isChangeDetected ? "change_detected" : "heartbeat";
  
  // Publish
  String output;
  serializeJson(doc, output);
  mqttClient.publish("addon/esp/esp_sensor_01/telemetry", output.c_str());
}

void loop() {
  // Ler sensores
  float newTemp = readTemperature();
  float newHum = readHumidity();
  
  // Verificar se houve mudança significativa
  bool changed = (abs(newTemp - lastTemp) > 0.5) || 
                 (abs(newHum - lastHum) > 2.0);
  
  // Enviar se mudou OU a cada 5 minutos
  if (changed || (millis() - lastSent > 300000)) {
    sendTelemetry(changed);
    lastSent = millis();
    lastTemp = newTemp;
    lastHum = newHum;
  }
}
```

## 📊 Visualização no Home Assistant

### Template Sensor para Contagem

```yaml
sensor:
  - platform: template
    sensors:
      esp_sensor_01_heartbeats:
        friendly_name: "Heartbeats Recebidos"
        value_template: >
          {{ states.sensor.esp_sensor_01_tipo_de_atualizacao.attributes.count 
             if states.sensor.esp_sensor_01_tipo_de_atualizacao.state == 'heartbeat' 
             else 0 }}
```

### Card de Status

```yaml
type: markdown
content: >
  ## 📡 ESP Sensor 01

  **Última Atualização:** {{ states('sensor.esp_sensor_01_tipo_de_atualizacao') }}

  **Temperatura:** {{ states('sensor.esp_sensor_01_temperatura') }}°C

  **Umidade:** {{ states('sensor.esp_sensor_01_umidade') }}%

  {% if is_state('sensor.esp_sensor_01_tipo_de_atualizacao', 'change_detected') %}
  🔔 Mudança detectada!
  {% else %}
  💚 Sistema estável
  {% endif %}
```

## ✅ Compatibilidade

- ✅ **Retrocompatível**: Se o ESP não enviar o campo `trigger`, funciona normalmente
- ✅ **Opcional**: Campo `trigger` é opcional no payload
- ✅ **Flexível**: Aceita qualquer valor de string no trigger

## 🐛 Debugging

### Ver Triggers no Log

Os logs mostrarão:

```json
{"level":"info","message":"Trigger recebido","deviceId":"esp_sensor_01","trigger":"change_detected"}
```

### Verificar no MQTT

Use um cliente MQTT para monitorar:

```bash
mosquitto_sub -h localhost -t "addon/esp/+/trigger" -v
```

Output esperado:
```
addon/esp/esp_sensor_01/trigger change_detected
addon/esp/esp_sensor_01/trigger heartbeat
```
