/**
 * Terrasmart Add-on - Main Entry Point
 * 
 * Este é o ponto de entrada principal do add-on.
 * Orquestra a inicialização de todos os serviços e handlers.
 */

const { loadConfig } = require("./src/config");
const logger = require("./src/utils/logger");
const mqttService = require("./src/services/mqttService");
const httpServer = require("./src/services/httpServer");
const discoveryService = require("./src/services/discoveryService");
const messageRouter = require("./src/handlers/messageRouter");
const {
  AVAILABILITY_TOPIC,
  STATUS_TOPIC,
  ESP_TELEMETRY_TOPIC,
  ESP_BASE_TOPIC,
  CMD_TOPIC,
} = require("./src/config/constants");

/**
 * Classe principal da aplicação
 */
class TerrasmartAddon {
  constructor() {
    this.config = null;
  }

  /**
   * Inicializa a aplicação
   */
  async init() {
    try {
      // Carregar configuração
      this.config = loadConfig();

      // Iniciar servidor HTTP
      httpServer.start(this.config.porta);

      // Conectar ao MQTT
      this._connectMqtt();

      // Configurar handlers de shutdown
      this._setupShutdownHandlers();

      logger.info("Terrasmart Add-on iniciado com sucesso");
    } catch (error) {
      logger.error("Erro ao iniciar add-on", { error: error.message });
      process.exit(1);
    }
  }

  /**
   * Conecta ao broker MQTT e configura callbacks
   * @private
   */
  _connectMqtt() {
    mqttService.connect(
      this.config,
      // Callback onConnect
      () => {
        this._onMqttConnect();
      },
      // Callback onMessage
      (topic, payload) => {
        messageRouter.route(topic, payload);
      }
    );
  }

  /**
   * Executado quando conecta ao MQTT
   * @private
   */
  _onMqttConnect() {
    // Inscrever em tópicos
    mqttService.subscribe([
      ESP_TELEMETRY_TOPIC, // legado
      `${ESP_BASE_TOPIC}/+/telemetry`, // novo padrão
      `${ESP_BASE_TOPIC}/+/status`, // status dos ESPs
      CMD_TOPIC, // comandos
    ]);

    // Publicar disponibilidade
    mqttService.publish(AVAILABILITY_TOPIC, "online", { retain: true });
    mqttService.publish(STATUS_TOPIC, "online", { retain: true });

    // Publicar discovery legado
    discoveryService.publishLegacyDiscovery();

    // Iniciar heartbeat
    this._startHeartbeat();
  }

  /**
   * Inicia o heartbeat periódico
   * @private
   */
  _startHeartbeat() {
    setInterval(() => {
      mqttService.publish(
        "addon/heartbeat",
        JSON.stringify({ ts: Date.now() })
      );
    }, 5000);
  }

  /**
   * Configura handlers para shutdown gracioso
   * @private
   */
  _setupShutdownHandlers() {
    const shutdown = () => {
      logger.info("Add-on desligando");

      // Publicar status offline
      mqttService.publish(AVAILABILITY_TOPIC, "offline", { retain: true });

      // Aguardar publicação antes de sair
      setTimeout(() => {
        mqttService.disconnect();
        httpServer.stop();
        process.exit(0);
      }, 500);
    };

    process.on("SIGTERM", shutdown);
    process.on("SIGINT", shutdown);
  }
}

// Iniciar aplicação
const addon = new TerrasmartAddon();
addon.init();
