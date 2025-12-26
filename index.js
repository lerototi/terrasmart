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

  client.publish("addon/status", "online", { retain: true });
  client.subscribe("addon/cmd");

  // heartbeat
  setInterval(() => {
    client.publish(
      "addon/heartbeat",
      JSON.stringify({ ts: Date.now() })
    );
  }, 5000);
});

client.on("message", (topic, payload) => {
  if (topic === "addon/cmd") {
    const msg = payload.toString();
    console.log("Comando recebido:", msg);

    if (msg === "ping") {
      client.publish("addon/resp", "pong");
    }
  }
});