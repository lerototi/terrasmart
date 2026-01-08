const logger = require("../utils/logger");
const espRegistry = require("../utils/espRegistry");
const mqttService = require("../services/mqttService");
const discoveryService = require("../services/discoveryService");
const { ESP_BASE_TOPIC, STATUS_DEBOUNCE_TIME } = require("../config/constants");

/**
 * Handler para processar mudanças de status dos dispositivos ESP
 */
class StatusHandler {
  /**
   * Processa uma atualização de status
   * @param {string} deviceId - ID do dispositivo
   * @param {string} status - Status (online/offline)
   */
  handle(deviceId, status) {
    const now = Date.now();

    // Registrar dispositivo se não existir
    if (!espRegistry.exists(deviceId)) {
      espRegistry.set(deviceId, {});
      discoveryService.publishStatusDiscovery(deviceId);
    }

    const device = espRegistry.get(deviceId);

    // === DEBOUNCE: Evitar flood de logs ===
    if (
      device.lastStatus === status &&
      now - device.lastStatusTime < STATUS_DEBOUNCE_TIME
    ) {
      // Atualizar apenas o timestamp sem logar
      espRegistry.updateLastSeen(deviceId);
      return; // Silenciosamente ignorar duplicatas
    }

    // Detectar se houve mudança real de status
    const isStatusChange = device.lastStatus !== status;

    // Log apropriado baseado no tipo de evento
    if (isStatusChange) {
      logger.info("Mudança de status do ESP", {
        deviceId,
        previousStatus: device.lastStatus || "unknown",
        newStatus: status,
      });
    } else {
      logger.debug("Heartbeat de status do ESP", { deviceId, status });
    }

    // Atualizar registry
    espRegistry.set(deviceId, {
      status,
      lastStatus: status,
      lastStatusTime: now,
    });
    espRegistry.updateLastSeen(deviceId);

    // Publicar status no tópico apenas se mudou
    if (isStatusChange) {
      mqttService.publish(
        `${ESP_BASE_TOPIC}/${deviceId}/status`,
        status,
        { retain: true }
      );
    }
  }
}

module.exports = new StatusHandler();
