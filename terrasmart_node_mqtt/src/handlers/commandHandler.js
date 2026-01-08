const logger = require("../utils/logger");
const mqttService = require("../services/mqttService");
const { RESP_TOPIC } = require("../config/constants");

/**
 * Handler para processar comandos recebidos
 */
class CommandHandler {
  /**
   * Processa um comando recebido
   * @param {string} payload - Payload do comando
   */
  handle(payload) {
    logger.info("Comando recebido", { payload });

    // Processar comandos conhecidos
    switch (payload) {
      case "ping":
        this._handlePing();
        break;

      default:
        logger.warn("Comando desconhecido", { payload });
    }
  }

  /**
   * Responde a um comando ping
   * @private
   */
  _handlePing() {
    mqttService.publish(RESP_TOPIC, "pong");
    logger.info("Respondido comando ping");
  }
}

module.exports = new CommandHandler();
