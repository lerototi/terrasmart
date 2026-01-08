/**
 * Logger centralizado para a aplicação
 */

const LOG_LEVELS = {
  debug: "debug",
  info: "info",
  warn: "warn",
  error: "error",
};

/**
 * Registra uma mensagem de log em formato JSON
 * @param {string} level - Nível do log (debug, info, warn, error)
 * @param {string} message - Mensagem descritiva
 * @param {Object} extra - Dados adicionais
 */
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

module.exports = {
  debug: (message, extra) => log(LOG_LEVELS.debug, message, extra),
  info: (message, extra) => log(LOG_LEVELS.info, message, extra),
  warn: (message, extra) => log(LOG_LEVELS.warn, message, extra),
  error: (message, extra) => log(LOG_LEVELS.error, message, extra),
};
