const fs = require("fs");
const express = require("express");
const mqtt = require("mqtt");

/* ===============================
 * CONFIGURAÇÃO
 * =============================== */

const CONFIG_PATH = "/data/options.json";

const DEVICE_ID = "terrasmart_addon";
const DEVICE_NAME = "Terrasmart Add-on";

const AVAILABILITY_TOPIC = "addon/availability";
const STATUS_TOPIC = "addon/status";

// LEGADO (mantido)
const ESP_TELEMETRY_TOPIC = "addon/esp/telemetry";
const ESP_TEMPERATURE_TOPIC = "addon/esp/temperature";

// NOVO PADRÃO
const ESP_BASE_TOPIC = "addon/esp";

const CMD_TOPIC = "addon/cmd";
const RESP_TOPIC = "addon/resp";

/* ===============================
 * REGISTRY DE ESPs
 * =============================== */

const espRegistry = {};

/* ===============================
 * LOG HELPER
 * =============================== */

function log(level, message, extra = {}) {
  console.log(
    JSON.stringify({
      level,
      message,
      time: new Date().toISOString(),
      ...extra,
    })
  );
}

/* ===============================
 * LER CONFIG
 * =============================== */

let config = {};
if (fs.existsSync(CONFIG_PATH)) {
  config = JSON.parse(fs.readFileSync(CONFIG_PATH));
  log("info", "Config carregada", config);
} else {
  log("error", "Arquivo de configuração não encontrado");
}

/* ===============================
 * EXPRESS (KEEP ALIVE)
 * =============================== */

const app = express();

app.get("/", (req, res) => {
  res.json({
    status: "ok",
    addon: DEVICE_NAME,
    devices: Object.keys(espRegistry),
  });
});

app.listen(config.porta, () => {
  log("info", "HTTP ativo", { porta: config.porta });
});

/* ===============================
 * MQTT
 * =============================== */

const mqttUrl = `mqtt://${config.mqtt_host}:${config.mqtt_port}`;

const client = mqtt.connect(mqttUrl, {
  username: config.mqtt_user,
  password: config.mqtt_password,
  will: {
    topic: AVAILABILITY_TOPIC,
    payload: "offline",
    retain: true,
  },
});

/* ===============================
 * MQTT CONNECT
 * =============================== */

client.on("connect", () => {
  log("info", "Conectado ao MQTT");

  client.subscribe([
  ESP_TELEMETRY_TOPIC,              // legado
  `${ESP_BASE_TOPIC}/+/telemetry`,  // novo
  `${ESP_BASE_TOPIC}/+/status`,     // 👈 novo
  CMD_TOPIC,
]);

  client.publish(AVAILABILITY_TOPIC, "online", { retain: true });
  client.publish(STATUS_TOPIC, "online", { retain: true });

  publishDiscovery(); // legado (mantido)
  startHeartbeat();
});

/* ===============================
 * MQTT MESSAGE
 * =============================== */

client.on("message", (topic, payload) => {
  const msg = payload.toString();

  // NOVO PADRÃO: addon/esp/<device_id>/telemetry
  if (topic.startsWith(`${ESP_BASE_TOPIC}/`) && topic.endsWith("/telemetry")) {
    const parts = topic.split("/");
    const deviceId = parts[2];
    handleEspTelemetry(deviceId, msg);
    return;
  }

  // STATUS DO ESP
  if (topic.startsWith(`${ESP_BASE_TOPIC}/`) && topic.endsWith("/status")) {
    const parts = topic.split("/");
    const deviceId = parts[2];
    const status = msg;

    handleEspStatus(deviceId, status);
    return;
  }

  // LEGADO: addon/esp/telemetry
  if (topic === ESP_TELEMETRY_TOPIC) {
    handleEspTelemetry("legacy_esp", msg);
    return;
  }

  if (topic === CMD_TOPIC) {
    handleCommand(msg);
  }
});

/* ===============================
 * HANDLERS
 * =============================== */

function handleEspTelemetry(deviceId, payload) {
  let data;

  try {
    data = JSON.parse(payload);
  } catch (e) {
    log("error", "Telemetria inválida", { payload });
    return;
  }

  // Validar estrutura básica do payload
  if (!data.device || !data.readings || !Array.isArray(data.readings)) {
    log("error", "Estrutura de telemetria inválida", { deviceId, payload });
    return;
  }

  const device = data.device;
  const readings = data.readings;

  // Registrar ou atualizar ESP
  if (!espRegistry[deviceId]) {
    espRegistry[deviceId] = {
      discovered: false,
      lastSeen: null,
      deviceInfo: {},
      sensorTypes: new Set(),
    };

    log("info", "Novo ESP detectado", { 
      deviceId, 
      type: device.type,
      ip: device.ip 
    });

    espRegistry[deviceId].discovered = true;
  }

  // Atualizar informações do dispositivo
  espRegistry[deviceId].lastSeen = Date.now();
  espRegistry[deviceId].deviceInfo = {
    type: device.type,
    uptime: device.uptime,
    heap: device.heap,
    rssi: device.rssi,
    ip: device.ip,
  };

  // Processar cada leitura dinamicamente
  readings.forEach((reading) => {
    const { type, value, unit } = reading;

    if (type && value !== undefined) {
      log("info", "Leitura recebida", {
        deviceId,
        readingType: type,
        value,
        unit: unit || "N/A",
      });

      // Registrar tipo de sensor descoberto
      if (!espRegistry[deviceId].sensorTypes.has(type)) {
        espRegistry[deviceId].sensorTypes.add(type);
        
        // Publicar discovery para este novo tipo de sensor
        publishSensorDiscovery(deviceId, type, unit, device.type);
      }

      // Publicar valor no tópico específico do sensor
      const sensorTopic = `${ESP_BASE_TOPIC}/${deviceId}/${type}`;
      client.publish(sensorTopic, value.toString(), { retain: true });
    }
  });

  // Publicar informações do dispositivo (opcional)
  const deviceInfoTopic = `${ESP_BASE_TOPIC}/${deviceId}/device_info`;
  client.publish(
    deviceInfoTopic, 
    JSON.stringify(espRegistry[deviceId].deviceInfo),
    { retain: true }
  );

  // LEGADO: manter compatibilidade com temperatura
  const tempReading = readings.find(r => r.type === "temperature");
  if (tempReading && deviceId === "legacy_esp") {
    client.publish(
      ESP_TEMPERATURE_TOPIC,
      tempReading.value.toString(),
      { retain: true }
    );
  }
}

function handleEspStatus(deviceId, status) {
  // === DEBOUNCE: Evitar flood de logs ===
  const now = Date.now();
  
  if (!espRegistry[deviceId]) {
    espRegistry[deviceId] = {
      discovered: false,
      lastSeen: null,
      deviceInfo: {},
      sensorTypes: new Set(),
      lastStatus: null,
      lastStatusTime: 0,
    };
    
    // Publicar discovery de status na primeira vez
    publishEspStatusDiscovery(deviceId);
  }

  // Se recebeu o mesmo status há menos de 5 segundos, ignorar
  if (espRegistry[deviceId].lastStatus === status &&
      (now - espRegistry[deviceId].lastStatusTime) < 5000) {
    // Atualizar apenas o timestamp sem logar
    espRegistry[deviceId].lastSeen = now;
    return; // Silenciosamente ignorar duplicatas
  }

  // Log apenas se for uma mudança real ou passou tempo suficiente
  const isStatusChange = espRegistry[deviceId].lastStatus !== status;
  
  if (isStatusChange) {
    log("info", "Mudança de status do ESP", { 
      deviceId, 
      previousStatus: espRegistry[deviceId].lastStatus || "unknown",
      newStatus: status 
    });
  } else {
    log("debug", "Heartbeat de status do ESP", { deviceId, status });
  }

  espRegistry[deviceId].status = status;
  espRegistry[deviceId].lastStatus = status;
  espRegistry[deviceId].lastStatusTime = now;
  espRegistry[deviceId].lastSeen = now;
  
  // Publicar status no tópico apenas se mudou
  if (isStatusChange) {
    client.publish(
      `${ESP_BASE_TOPIC}/${deviceId}/status`,
      status,
      { retain: true }
    );
  }
}

function handleCommand(payload) {
  log("info", "Comando recebido", { payload });

  if (payload === "ping") {
    client.publish(RESP_TOPIC, "pong");
  }
}

/* ===============================
 * DISCOVERY (LEGADO)
 * =============================== */

function publishDiscovery() {
  log("info", "Publicando MQTT Discovery (legado)");

  client.publish(
    `homeassistant/sensor/${DEVICE_ID}/esp_temperature/config`,
    JSON.stringify({
      name: "Temperatura ESP (legado)",
      state_topic: ESP_TEMPERATURE_TOPIC,
      unit_of_measurement: "°C",
      device_class: "temperature",
      availability_topic: AVAILABILITY_TOPIC,
      payload_available: "online",
      payload_not_available: "offline",
      unique_id: "terrasmart_esp_temperature",
      device: {
        identifiers: [DEVICE_ID],
        name: DEVICE_NAME,
        manufacturer: "Terrasmart",
        model: "ESP Gateway",
      },
    }),
    { retain: true }
  );
}

/* ===============================
 * DISCOVERY DINÂMICO POR ESP
 * =============================== */

function publishEspDiscovery(deviceId) {
  const uniqueId = `terrasmart_${deviceId}_temperature`;
  const discoveryTopic = `homeassistant/sensor/${uniqueId}/config`;
  const stateTopic = `${ESP_BASE_TOPIC}/${deviceId}/temperature`;

  log("info", "Publicando discovery do ESP", {
    deviceId,
    discoveryTopic,
    stateTopic,
  });

  client.publish(
    discoveryTopic,
    JSON.stringify({
      name: `Temperatura ${deviceId}`,
      state_topic: stateTopic,
      unit_of_measurement: "°C",
      device_class: "temperature",
      availability_topic: AVAILABILITY_TOPIC,
      payload_available: "online",
      payload_not_available: "offline",
      unique_id: uniqueId,
      device: {
        identifiers: [deviceId],
        name: `ESP ${deviceId}`,
        manufacturer: "Terrasmart",
        model: "ESP Sensor",
        via_device: DEVICE_ID,
      },
    }),
    { retain: true }
  );
}

/* ===============================
 * DISCOVERY DINÂMICO POR SENSOR
 * =============================== */

function publishSensorDiscovery(deviceId, sensorType, unit, deviceType) {
  const uniqueId = `terrasmart_${deviceId}_${sensorType}`;
  const stateTopic = `${ESP_BASE_TOPIC}/${deviceId}/${sensorType}`;
  
  // Mapeamento de tipos de sensor para device_class e domain do Home Assistant
  const sensorConfig = getSensorConfig(sensorType, unit);
  
  const discoveryTopic = `homeassistant/${sensorConfig.domain}/${uniqueId}/config`;

  log("info", "Publicando discovery de sensor", {
    deviceId,
    sensorType,
    domain: sensorConfig.domain,
    discoveryTopic,
  });

  const config = {
    name: `${deviceId} ${sensorConfig.friendlyName}`,
    state_topic: stateTopic,
    availability_topic: AVAILABILITY_TOPIC,
    payload_available: "online",
    payload_not_available: "offline",
    unique_id: uniqueId,
    device: {
      identifiers: [deviceId],
      name: `ESP ${deviceId}`,
      manufacturer: "Terrasmart",
      model: deviceType || "ESP Sensor",
      via_device: DEVICE_ID,
    },
  };

  // Adicionar propriedades específicas do tipo de sensor
  if (sensorConfig.unit_of_measurement) {
    config.unit_of_measurement = sensorConfig.unit_of_measurement;
  }
  
  if (sensorConfig.device_class) {
    config.device_class = sensorConfig.device_class;
  }

  if (sensorConfig.domain === "binary_sensor") {
    config.payload_on = sensorConfig.payload_on || "1";
    config.payload_off = sensorConfig.payload_off || "0";
  }

  client.publish(discoveryTopic, JSON.stringify(config), { retain: true });
}

/* ===============================
 * CONFIGURAÇÃO DE SENSORES
 * =============================== */

function getSensorConfig(sensorType, unit) {
  const configs = {
    temperature: {
      domain: "sensor",
      friendlyName: "Temperatura",
      device_class: "temperature",
      unit_of_measurement: unit || "°C",
    },
    humidity: {
      domain: "sensor",
      friendlyName: "Umidade",
      device_class: "humidity",
      unit_of_measurement: unit || "%",
    },
    distance: {
      domain: "sensor",
      friendlyName: "Distância",
      device_class: "distance",
      unit_of_measurement: unit || "cm",
    },
    level: {
      domain: "sensor",
      friendlyName: "Nível",
      device_class: null,
      unit_of_measurement: unit || "%",
    },
    state: {
      domain: "binary_sensor",
      friendlyName: "Estado",
      device_class: "opening",
      payload_on: "1",
      payload_off: "0",
    },
    motion: {
      domain: "binary_sensor",
      friendlyName: "Movimento",
      device_class: "motion",
      payload_on: "1",
      payload_off: "0",
    },
    pressure: {
      domain: "sensor",
      friendlyName: "Pressão",
      device_class: "pressure",
      unit_of_measurement: unit || "hPa",
    },
    flow: {
      domain: "sensor",
      friendlyName: "Fluxo",
      device_class: null,
      unit_of_measurement: unit || "L/min",
    },
  };

  // Retornar configuração específica ou genérica
  return configs[sensorType] || {
    domain: "sensor",
    friendlyName: sensorType.charAt(0).toUpperCase() + sensorType.slice(1),
    device_class: null,
    unit_of_measurement: unit || "",
  };
}

function publishEspStatusDiscovery(deviceId) {
  const uniqueId = `terrasmart_${deviceId}_status`;
  const discoveryTopic = `homeassistant/binary_sensor/${uniqueId}/config`;
  const stateTopic = `${ESP_BASE_TOPIC}/${deviceId}/status`;

  log("info", "Publicando discovery de status", { deviceId });

  client.publish(
    discoveryTopic,
    JSON.stringify({
      name: `ESP ${deviceId} Online`,
      state_topic: stateTopic,
      payload_on: "online",
      payload_off: "offline",
      device_class: "connectivity",
      unique_id: uniqueId,
      device: {
        identifiers: [deviceId],
        name: `ESP ${deviceId}`,
        via_device: DEVICE_ID,
      },
    }),
    { retain: true }
  );
}



/* ===============================
 * HEARTBEAT
 * =============================== */

function startHeartbeat() {
  setInterval(() => {
    client.publish(
      "addon/heartbeat",
      JSON.stringify({ ts: Date.now() })
    );
  }, 5000);
}

/* ===============================
 * SHUTDOWN
 * =============================== */

function shutdown() {
  log("info", "Add-on desligando");
  client.publish(AVAILABILITY_TOPIC, "offline", { retain: true }, () => {
    process.exit(0);
  });
}

process.on("SIGTERM", shutdown);
process.on("SIGINT", shutdown);
