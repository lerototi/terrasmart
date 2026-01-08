const telemetryHandler = require("./telemetryHandler");
const statusHandler = require("./statusHandler");
const commandHandler = require("./commandHandler");
const {
  ESP_TELEMETRY_TOPIC,
  ESP_BASE_TOPIC,
  CMD_TOPIC,
} = require("../config/constants");

/**
 * Router de mensagens MQTT
 * Direciona mensagens recebidas para os handlers apropriados
 */
class MessageRouter {
  /**
   * Roteia uma mensagem MQTT para o handler apropriado
   * @param {string} topic - Tópico da mensagem
   * @param {Buffer} payload - Payload da mensagem
   */
  route(topic, payload) {
    const msg = payload.toString();

    // NOVO PADRÃO: addon/esp/<device_id>/telemetry
    if (topic.startsWith(`${ESP_BASE_TOPIC}/`) && topic.endsWith("/telemetry")) {
      const parts = topic.split("/");
      const deviceId = parts[2];
      telemetryHandler.handle(deviceId, msg);
      return;
    }

    // STATUS DO ESP: addon/esp/<device_id>/status
    if (topic.startsWith(`${ESP_BASE_TOPIC}/`) && topic.endsWith("/status")) {
      const parts = topic.split("/");
      const deviceId = parts[2];
      statusHandler.handle(deviceId, msg);
      return;
    }

    // LEGADO: addon/esp/telemetry
    if (topic === ESP_TELEMETRY_TOPIC) {
      telemetryHandler.handle("legacy_esp", msg);
      return;
    }

    // COMANDOS: addon/cmd
    if (topic === CMD_TOPIC) {
      commandHandler.handle(msg);
      return;
    }
  }
}

module.exports = new MessageRouter();
