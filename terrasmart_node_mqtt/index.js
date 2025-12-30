const fs = require("fs");
const express = require("express");
const mqtt = require("mqtt");

const CONFIG_PATH = "/data/options.json";

// === LER CONFIG ===
let config = {};
if (fs.existsSync(CONFIG_PATH)) {
  config = JSON.parse(fs.readFileSync(CONFIG_PATH));
}

// === EXPRESS (continua vivo) ===
const app = express();
app.get("/", (req, res) => {
  res.json({ status: "ok", mensagem: config.mensagem });
});
app.listen(config.porta, () => {
  console.log(`HTTP ativo na porta ${config.porta}`);
});

// === MQTT ===
const mqttUrl = `mqtt://${config.mqtt_host}:${config.mqtt_port}`;

const client = mqtt.connect(mqttUrl, {
  username: config.mqtt_user,
  password: config.mqtt_password,
  will: {
    topic: "addon/status",
    payload: "offline",
    retain: true,
  },
});

client.on("connect", () => {
  console.log("Conectado ao MQTT");

  client.subscribe("addon/esp/telemetry");

  client.publish("addon/status", "online", { retain: true });
  client.subscribe("addon/cmd", (err) => {
    if (!err) {
      console.log("Inscrito em addon/cmd");
  }

    if(err) {
      console.error("Erro ao se inscrever no tópico addon/cmd:", err);
      client.publish("addon/resp", `error: ${err}`);
    }
});

  // heartbeat
  setInterval(() => {
    client.publish(
      "addon/heartbeat",
      JSON.stringify({ ts: Date.now() })
    );
  }, 5000);
});

client.on("message", (topic, payload) => {

  if (topic === "addon/esp/telemetry") {
    let data;

    try {
      data = JSON.parse(payload.toString());
    } catch (e) {
      console.error("Telemetria inválida:", data.toString());
      return;
    }

    if (typeof data.temperature === "number") {
      client.publish(
        "addon/esp/temperature",
        data.temperature.toString(),
        { retain: true }
      );
    }
  }

  if (topic === "addon/cmd") {
    const msg = payload.toString();
    console.log(`MQTT recebido: ${topic}: ${msg}`);
  
      if (msg === "ping") {
        client.publish("addon/resp", "pong");
      }
  }
});