const mqtt = require("mqtt");
const logger = require("../utils/logger");
const { AVAILABILITY_TOPIC } = require("../config/constants");

/**
 * Serviço de gerenciamento de conexão MQTT
 */
class MqttService {
  constructor() {
    this.client = null;
    this.config = null;
  }

  /**
   * Conecta ao broker MQTT
   * @param {Object} config - Configuração da conexão
   * @param {Function} onConnect - Callback quando conectar
   * @param {Function} onMessage - Callback para mensagens recebidas
   */
  connect(config, onConnect, onMessage) {
    this.config = config;
    const mqttUrl = `mqtt://${config.mqtt_host}:${config.mqtt_port}`;

    this.client = mqtt.connect(mqttUrl, {
      username: config.mqtt_user,
      password: config.mqtt_password,
      will: {
        topic: AVAILABILITY_TOPIC,
        payload: "offline",
        retain: true,
      },
    });

    this.client.on("connect", () => {
      logger.info("Conectado ao MQTT");
      if (onConnect) onConnect(this.client);
    });

    this.client.on("message", (topic, payload) => {
      if (onMessage) onMessage(topic, payload);
    });

    this.client.on("error", (error) => {
      logger.error("Erro no MQTT", { error: error.message });
    });

    this.client.on("close", () => {
      logger.warn("Conexão MQTT fechada");
    });

    this.client.on("reconnect", () => {
      logger.info("Reconectando ao MQTT");
    });
  }

  /**
   * Publica uma mensagem em um tópico
   * @param {string} topic - Tópico MQTT
   * @param {string} payload - Payload da mensagem
   * @param {Object} options - Opções de publicação
   */
  publish(topic, payload, options = {}) {
    if (!this.client) {
      logger.error("Cliente MQTT não conectado");
      return;
    }

    this.client.publish(topic, payload, options);
  }

  /**
   * Inscreve em um ou mais tópicos
   * @param {string|string[]} topics - Tópico(s) para inscrição
   */
  subscribe(topics) {
    if (!this.client) {
      logger.error("Cliente MQTT não conectado");
      return;
    }

    this.client.subscribe(topics, (err) => {
      if (err) {
        logger.error("Erro ao inscrever em tópicos", { error: err.message });
      } else {
        logger.info("Inscrito em tópicos", { topics });
      }
    });
  }

  /**
   * Desconecta do broker MQTT
   */
  disconnect() {
    if (this.client) {
      this.client.end();
      logger.info("Desconectado do MQTT");
    }
  }

  /**
   * Retorna o cliente MQTT
   * @returns {Object}
   */
  getClient() {
    return this.client;
  }
}

module.exports = new MqttService();
