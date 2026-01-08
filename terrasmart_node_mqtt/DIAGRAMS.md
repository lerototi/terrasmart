# Diagrama de Arquitetura

## Fluxo de Dados

```
┌─────────────────────────────────────────────────────────────────┐
│                        ESP Devices                               │
│  (Enviando telemetria, status, respondendo comandos)            │
└────────────────────────┬────────────────────────────────────────┘
                         │ MQTT
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                    MQTT Broker                                   │
│             (Mosquitto / Home Assistant)                         │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                  Terrasmart Add-on                               │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │              index.js (Orquestrador)                   │    │
│  └──────────────┬───────────────────┬─────────────────────┘    │
│                 │                   │                           │
│                 ▼                   ▼                           │
│  ┌──────────────────────┐  ┌─────────────────────┐            │
│  │   MQTT Service       │  │   HTTP Server       │            │
│  │  (Conexão MQTT)      │  │  (Keep-alive/API)   │            │
│  └──────────┬───────────┘  └─────────────────────┘            │
│             │                                                   │
│             ▼                                                   │
│  ┌──────────────────────────────────────────────────────┐     │
│  │           Message Router                              │     │
│  │  (Analisa tópico e roteia para handler correto)     │     │
│  └──────────┬────────────────┬──────────────┬───────────┘     │
│             │                │              │                  │
│             ▼                ▼              ▼                  │
│  ┌─────────────────┐ ┌─────────────┐ ┌──────────────┐       │
│  │ Telemetry       │ │  Status     │ │  Command     │       │
│  │ Handler         │ │  Handler    │ │  Handler     │       │
│  │ (Sensores)      │ │ (Debounce)  │ │ (Ping/etc)   │       │
│  └────────┬────────┘ └──────┬──────┘ └──────┬───────┘       │
│           │                 │                │                │
│           │                 │                │                │
│           └─────────────────┴────────────────┘                │
│                             ▼                                 │
│           ┌──────────────────────────────────┐               │
│           │      ESP Registry                │               │
│           │  (Gerencia dispositivos)         │               │
│           └──────────────────────────────────┘               │
│                             ▼                                 │
│           ┌──────────────────────────────────┐               │
│           │   Discovery Service              │               │
│           │  (MQTT Discovery - HA)           │               │
│           └──────────────────────────────────┘               │
│                             │                                 │
└─────────────────────────────┼─────────────────────────────────┘
                              │
                              ▼
                ┌──────────────────────────────┐
                │     Home Assistant           │
                │  (Auto-descoberta sensores)  │
                └──────────────────────────────┘
```

## Módulos e Interações

```
┌─────────────────────────────────────────────────────────────────┐
│                          CONFIG                                  │
├─────────────────────────────────────────────────────────────────┤
│  • constants.js      → Todas as constantes                      │
│  • index.js          → Carrega config do arquivo                │
│  • sensorConfigs.js  → Mapeia tipos de sensores                 │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │ usa
                              │
┌─────────────────────────────┴───────────────────────────────────┐
│                          UTILS                                   │
├─────────────────────────────────────────────────────────────────┤
│  • logger.js         → Log estruturado em JSON                  │
│  • espRegistry.js    → Singleton: gerencia ESPs                 │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │ usa
                              │
┌─────────────────────────────┴───────────────────────────────────┐
│                        SERVICES                                  │
├─────────────────────────────────────────────────────────────────┤
│  • mqttService.js       → Gerencia conexão MQTT                 │
│  • discoveryService.js  → Publica MQTT Discovery                │
│  • httpServer.js        → API REST + Keep-alive                 │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │ usa
                              │
┌─────────────────────────────┴───────────────────────────────────┐
│                        HANDLERS                                  │
├─────────────────────────────────────────────────────────────────┤
│  • messageRouter.js     → Roteia msgs MQTT                      │
│  • telemetryHandler.js  → Processa telemetria                   │
│  • statusHandler.js     → Processa status (debounce)            │
│  • commandHandler.js    → Processa comandos                     │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │ orquestra
                              │
                    ┌─────────┴─────────┐
                    │    index.js       │
                    │  (Entry Point)    │
                    └───────────────────┘
```

## Fluxo de Telemetria

```
ESP Device
   │
   │ Publica: addon/esp/esp_01/telemetry
   │ Payload: { device: {...}, readings: [...] }
   │
   ▼
MQTT Broker
   │
   ▼
MQTT Service (recebe)
   │
   ▼
Message Router (analisa tópico)
   │
   ├─ Identifica: telemetry
   │
   ▼
Telemetry Handler
   │
   ├─ Valida JSON
   ├─ Registra device no Registry
   ├─ Processa cada reading
   │   │
   │   ├─ Detecta novo tipo de sensor?
   │   │   └─► Discovery Service → Publica HA Discovery
   │   │
   │   └─ Publica valor: addon/esp/esp_01/temperature
   │
   └─ Publica device_info: addon/esp/esp_01/device_info
```

## Fluxo de Status (com Debounce)

```
ESP Device
   │
   │ Publica repetidamente: addon/esp/esp_01/status = "online"
   │
   ▼
MQTT Broker
   │
   ▼
Message Router
   │
   ▼
Status Handler
   │
   ├─ Verifica debounce (5 segundos)
   │   │
   │   ├─ Mesmo status < 5s? → Ignora (sem log)
   │   │
   │   └─ Status mudou OU > 5s?
   │       │
   │       ├─ Status mudou? → Log: "Mudança de status"
   │       │                → Publica MQTT
   │       │
   │       └─ Heartbeat? → Log: "Heartbeat" (debug)
   │
   └─ Atualiza Registry
```

## APIs HTTP Disponíveis

```
HTTP Client
   │
   ├─ GET /
   │   └─► { status, addon, devices: [...] }
   │
   ├─ GET /health
   │   └─► { status: "healthy", uptime, timestamp }
   │
   └─ GET /devices
       └─► { devices: [{ id, info, lastSeen, status, sensorTypes }] }
```
