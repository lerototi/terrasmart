# Estrutura de Telemetria - Terrasmart

## 📋 Visão Geral

Este documento descreve a estrutura de payload de telemetria utilizada pelos dispositivos ESP e como o add-on processa essas informações de forma dinâmica.

## 🔧 Estrutura do Payload

O payload de telemetria é dividido em três partes principais:

### 1. **device** (Informações do Dispositivo)
Informações sobre o hardware e status do ESP:

```json
{
  "device": {
    "id": "esp_sensor_01",           // Identificador único do dispositivo
    "type": "DHT22 Sensor",          // Tipo/modelo do dispositivo
    "uptime": 161,                   // Tempo ligado em segundos
    "heap": 39424,                   // Memória livre em bytes
    "rssi": -47,                     // Sinal WiFi em dBm
    "ip": "10.0.0.245"              // Endereço IP
  }
}
```

### 2. **readings** (Leituras dos Sensores)
Array com as leituras de todos os sensores conectados:

```json
{
  "readings": [
    {
      "type": "temperature",         // Tipo de leitura
      "value": 24.1,                // Valor medido
      "unit": "°C"                  // Unidade de medida
    },
    {
      "type": "humidity",
      "value": 81.8,
      "unit": "%"
    }
  ]
}
```

### 3. **timestamp** (Momento da Leitura)
```json
{
  "timestamp": "2026-01-07T12:30:45.000Z"
}
```

## 📊 Exemplos Completos

### Sensor de Temperatura e Umidade (DHT22)
```json
{
  "device": {
    "id": "esp_sensor_01",
    "type": "DHT22 Temperature & Humidity Sensor",
    "uptime": 161,
    "heap": 39424,
    "rssi": -47,
    "ip": "10.0.0.245"
  },
  "readings": [
    {"type": "temperature", "value": 24.1, "unit": "°C"},
    {"type": "humidity", "value": 81.8, "unit": "%"}
  ],
  "timestamp": "2026-01-07T12:30:45.000Z"
}
```

### Sensor Ultrassônico (Nível de Reservatório)
```json
{
  "device": {
    "id": "esp_tank_01",
    "type": "JSN-SR04M",
    "uptime": 7200,
    "heap": 43120,
    "rssi": -72,
    "ip": "192.168.1.101"
  },
  "readings": [
    {"type": "distance", "value": 45.2, "unit": "cm"},
    {"type": "level", "value": 78.5, "unit": "%"}
  ],
  "timestamp": "2026-01-07T12:30:45.000Z"
}
```

### Sensor Binário (Porta/Janela)
```json
{
  "device": {
    "id": "esp_door_01",
    "type": "Reed Switch",
    "uptime": 86400,
    "heap": 48000,
    "rssi": -65,
    "ip": "192.168.1.102"
  },
  "readings": [
    {"type": "state", "value": 1, "unit": ""}
  ],
  "timestamp": "2026-01-07T12:30:45.000Z"
}
```

## 🎯 Tipos de Sensores Suportados

O add-on suporta automaticamente diversos tipos de sensores:

| Tipo | Domain | Device Class | Unidade Padrão |
|------|--------|--------------|----------------|
| `temperature` | sensor | temperature | °C |
| `humidity` | sensor | humidity | % |
| `distance` | sensor | distance | cm |
| `level` | sensor | - | % |
| `state` | binary_sensor | opening | - |
| `motion` | binary_sensor | motion | - |
| `pressure` | sensor | pressure | hPa |
| `flow` | sensor | - | L/min |

### Adicionar Novos Tipos

Para adicionar novos tipos de sensores, edite a função `getSensorConfig()` em `index.js`:

```javascript
function getSensorConfig(sensorType, unit) {
  const configs = {
    // ... sensores existentes ...
    
    novo_tipo: {
      domain: "sensor",              // ou "binary_sensor"
      friendlyName: "Nome Amigável",
      device_class: "classe_ha",     // ou null
      unit_of_measurement: unit || "unidade",
    },
  };
  
  return configs[sensorType] || { /* config genérica */ };
}
```

## 🔄 Fluxo de Processamento

1. **Recepção**: Add-on recebe payload no tópico `addon/esp/{device_id}/telemetry`
2. **Validação**: Verifica estrutura do JSON e presença dos campos obrigatórios
3. **Registro**: 
   - Se é um novo ESP, cria entrada no registry
   - Atualiza informações do dispositivo
4. **Discovery Automático**:
   - Para cada tipo de leitura detectado pela primeira vez
   - Publica configuração MQTT Discovery no Home Assistant
5. **Publicação de Valores**:
   - Cada leitura é publicada em `addon/esp/{device_id}/{sensor_type}`
   - Informações do dispositivo em `addon/esp/{device_id}/device_info`

## 📡 Tópicos MQTT

### Entrada (ESP → Add-on)
- `addon/esp/{device_id}/telemetry` - Payload de telemetria
- `addon/esp/{device_id}/status` - Status online/offline

### Saída (Add-on → Home Assistant)
- `addon/esp/{device_id}/{sensor_type}` - Valor de cada sensor
- `addon/esp/{device_id}/device_info` - Informações do dispositivo
- `homeassistant/{domain}/{unique_id}/config` - MQTT Discovery

## ✅ Vantagens desta Estrutura

1. **Flexibilidade**: Suporta qualquer tipo de sensor sem alterar código
2. **Escalabilidade**: Múltiplas leituras em um único payload
3. **Rastreabilidade**: Informações do dispositivo sempre presentes
4. **Auto-discovery**: Sensores aparecem automaticamente no Home Assistant
5. **Diagnóstico**: Dados de uptime, heap, rssi e IP disponíveis

## 🐛 Troubleshooting

### Sensor não aparece no Home Assistant
- Verifique se o payload está na estrutura correta
- Confira os logs do add-on para erros de parsing
- Verifique se o MQTT Discovery está habilitado no HA

### Valores não atualizam
- Confirme que o tópico está sendo publicado com `retain: true`
- Verifique a conectividade MQTT do ESP
- Confira o campo `timestamp` está no formato ISO 8601

### Novo tipo de sensor não reconhecido
- Adicione o tipo na função `getSensorConfig()`
- Reinicie o add-on após modificação
- Remova e adicione o dispositivo no HA se necessário
