const logger = require("../utils/logger");
const espRegistry = require("../utils/espRegistry");
const mqttService = require("../services/mqttService");
const discoveryService = require("../services/discoveryService");
const { ESP_BASE_TOPIC, ESP_TEMPERATURE_TOPIC } = require("../config/constants");

/**
 * Handler para processar mensagens de telemetria dos dispositivos ESP
 */
class TelemetryHandler {
  /**
   * Processa uma mensagem de telemetria
   * @param {string} deviceId - ID do dispositivo
   * @param {string} payload - Payload JSON da telemetria
   */
  handle(deviceId, payload) {
    let data;

    try {
      data = JSON.parse(payload);
    } catch (e) {
      logger.error("Telemetria inválida", { payload });
      return;
    }

    // Validar estrutura básica do payload
    if (!data.device || !data.readings || !Array.isArray(data.readings)) {
      logger.error("Estrutura de telemetria inválida", { deviceId, payload });
      return;
    }

    const device = data.device;
    const readings = data.readings;

    // Registrar ou atualizar ESP
    if (!espRegistry.exists(deviceId)) {
      espRegistry.set(deviceId, {});
      logger.info("Novo ESP detectado", {
        deviceId,
        type: device.type,
        ip: device.ip,
      });
      espRegistry.markAsDiscovered(deviceId);
    }

    // Atualizar informações do dispositivo
    espRegistry.updateLastSeen(deviceId);
    espRegistry.updateDeviceInfo(deviceId, {
      type: device.type,
      uptime: device.uptime,
      heap: device.heap,
      rssi: device.rssi,
      ip: device.ip,
    });

    // Processar cada leitura dinamicamente
    readings.forEach((reading) => {
      this._processReading(deviceId, reading, device.type);
    });

    // Publicar informações do dispositivo
    this._publishDeviceInfo(deviceId);

    // LEGADO: manter compatibilidade com temperatura
    this._handleLegacyTemperature(deviceId, readings);
  }

  /**
   * Processa uma leitura individual de sensor
   * @param {string} deviceId - ID do dispositivo
   * @param {Object} reading - Leitura do sensor
   * @param {string} deviceType - Tipo do dispositivo
   * @private
   */
  _processReading(deviceId, reading, deviceType) {
    const { type, value, unit } = reading;

    if (!type || value === undefined) {
      return;
    }

    logger.info("Leitura recebida", {
      deviceId,
      readingType: type,
      value,
      unit: unit || "N/A",
    });

    // Registrar tipo de sensor descoberto
    const isNewSensorType = espRegistry.addSensorType(deviceId, type);
    if (isNewSensorType) {
      // Publicar discovery para este novo tipo de sensor
      discoveryService.publishSensorDiscovery(deviceId, type, unit, deviceType);
    }

    // Publicar valor no tópico específico do sensor
    const sensorTopic = `${ESP_BASE_TOPIC}/${deviceId}/${type}`;
    mqttService.publish(sensorTopic, value.toString(), { retain: true });
  }

  /**
   * Publica informações do dispositivo em um tópico dedicado
   * @param {string} deviceId - ID do dispositivo
   * @private
   */
  _publishDeviceInfo(deviceId) {
    const device = espRegistry.get(deviceId);
    if (!device || !device.deviceInfo) {
      return;
    }

    const deviceInfoTopic = `${ESP_BASE_TOPIC}/${deviceId}/device_info`;
    mqttService.publish(
      deviceInfoTopic,
      JSON.stringify(device.deviceInfo),
      { retain: true }
    );
  }

  /**
   * Mantém compatibilidade com formato legado de temperatura
   * @param {string} deviceId - ID do dispositivo
   * @param {Array} readings - Array de leituras
   * @private
   */
  _handleLegacyTemperature(deviceId, readings) {
    const tempReading = readings.find((r) => r.type === "temperature");
    if (tempReading && deviceId === "legacy_esp") {
      mqttService.publish(
        ESP_TEMPERATURE_TOPIC,
        tempReading.value.toString(),
        { retain: true }
      );
    }
  }
}

module.exports = new TelemetryHandler();
