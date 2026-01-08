const fs = require("fs");
const { CONFIG_PATH } = require("./constants");
const logger = require("../utils/logger");

/**
 * Carrega e retorna a configuração do add-on
 * @returns {Object} Configuração carregada
 */
function loadConfig() {
  let config = {};

  try {
    if (fs.existsSync(CONFIG_PATH)) {
      const fileContent = fs.readFileSync(CONFIG_PATH, "utf8");
      config = JSON.parse(fileContent);
      logger.info("Config carregada", config);
    } else {
      logger.error("Arquivo de configuração não encontrado", {
        path: CONFIG_PATH,
      });
      // Valores padrão para desenvolvimento
      config = {
        porta: 8099,
        mqtt_host: "localhost",
        mqtt_port: 1883,
        mqtt_user: "",
        mqtt_password: "",
      };
    }
  } catch (error) {
    logger.error("Erro ao carregar configuração", {
      error: error.message,
      path: CONFIG_PATH,
    });
  }

  return config;
}

module.exports = { loadConfig };
