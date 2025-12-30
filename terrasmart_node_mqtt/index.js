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
const ESP_TELEMETRY_TOPIC = "addon/esp/telemetry";
const ESP_TEMPERATURE_TOPIC = "addon/esp/temperature";
const CMD_TOPIC = "addon/cmd";
const RESP_TOPIC = "addon/resp";

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

  client.subscribe([ESP_TELEMETRY_TOPIC, CMD_TOPIC]);

  client.publish(AVAILABILITY_TOPIC, "online", { retain: true });
  client.publish(STATUS_TOPIC, "online", { retain: true });

  publishDiscovery();

  startHeartbeat();
});

/* ===============================
 * MQTT MESSAGE
 * =============================== */

client.on("message", (topic, payload) => {
  const msg = payload.toString();

  if (topic === ESP_TELEMETRY_TOPIC) {
    handleEspTelemetry(msg);
  }

  if (topic === CMD_TOPIC) {
    handleCommand(msg);
  }
});

/* ===============================
 * HANDLERS
 * =============================== */

function handleEspTelemetry(payload) {
  let data;

  try {
    data = JSON.parse(payload);
  } catch (e) {
    log("error", "Telemetria inválida", { payload });
    return;
  }

  if (typeof data.temperature === "number") {
    log("info", "Temperatura recebida", { value: data.temperature });

    client.publish(
      ESP_TEMPERATURE_TOPIC,
      data.temperature.toString(),
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
 * DISCOVERY
 * =============================== */

function publishDiscovery() {
  log("info", "Publicando MQTT Discovery");

  client.publish(
    `homeassistant/sensor/${DEVICE_ID}/esp_temperature/config`,
    JSON.stringify({
      name: "Temperatura ESP",
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
