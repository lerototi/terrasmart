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
  publishEspStatusDiscovery(deviceId);
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

  if (!espRegistry[deviceId]) {
    espRegistry[deviceId] = {
      discovered: false,
      lastSeen: null,
    };

    log("info", "Novo ESP detectado", { deviceId });

    publishEspDiscovery(deviceId);
    espRegistry[deviceId].discovered = true;
  }

  espRegistry[deviceId].lastSeen = Date.now();

  if (typeof data.temperature === "number") {
    log("info", "Temperatura recebida", {
      deviceId,
      value: data.temperature,
    });

    // NOVO TÓPICO POR DEVICE
    client.publish(
      `${ESP_BASE_TOPIC}/${deviceId}/temperature`,
      data.temperature.toString(),
      { retain: true }
    );

    // LEGADO (mantido)
    if (deviceId === "legacy_esp") {
      client.publish(
        ESP_TEMPERATURE_TOPIC,
        data.temperature.toString(),
        { retain: true }
      );
    }
  }
}

function handleEspStatus(deviceId, status) {
  log("info", "Status do ESP", { deviceId, status });

  if (!espRegistry[deviceId]) {
    espRegistry[deviceId] = {
      discovered: false,
      lastSeen: null,
    };
  }

  espRegistry[deviceId].status = status;
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
