const mqttService = require("./mqttService");
const logger = require("../utils/logger");
const { getSensorConfig } = require("../config/sensorConfigs");
const {
  DEVICE_ID,
  DEVICE_NAME,
  AVAILABILITY_TOPIC,
  ESP_BASE_TOPIC,
  ESP_TEMPERATURE_TOPIC,
} = require("../config/constants");

/**
 * Serviço de publicação de descoberta automática (MQTT Discovery)
 */
class DiscoveryService {
  /**
   * Publica discovery para sensor legado de temperatura
   */
  publishLegacyDiscovery() {
    logger.info("Publicando MQTT Discovery (legado)");

    mqttService.publish(
      `homeassistant/sensor/${DEVICE_ID}/esp_temperature/config`,
      JSON.stringify({
        name: "Temperatura ESP (legado)",
        state_topic: ESP_TEMPERATURE_TOPIC,
        unit_of_measurement: "°C",
        device_class: "temperature",
        availability_topic: AVAILABILITY_TOPIC,
        payload_available: "online",
        payload_not_available: "offline",
        unique_id: "terrasmart_esp_temperature",
        device: {
          identifiers: [DEVICE_ID],
          name: DEVICE_NAME,
          manufacturer: "Terrasmart",
          model: "ESP Gateway",
        },
      }),
      { retain: true }
    );
  }

  /**
   * Publica discovery dinâmico para um sensor específico
   * @param {string} deviceId - ID do dispositivo ESP
   * @param {string} sensorType - Tipo do sensor
   * @param {string} unit - Unidade de medida
   * @param {string} deviceType - Tipo do dispositivo
   */
  publishSensorDiscovery(deviceId, sensorType, unit, deviceType) {
    const uniqueId = `terrasmart_${deviceId}_${sensorType}`;
    const stateTopic = `${ESP_BASE_TOPIC}/${deviceId}/${sensorType}`;

    const sensorConfig = getSensorConfig(sensorType, unit);
    const discoveryTopic = `homeassistant/${sensorConfig.domain}/${uniqueId}/config`;

    logger.info("Publicando discovery de sensor", {
      deviceId,
      sensorType,
      domain: sensorConfig.domain,
      discoveryTopic,
    });

    const config = {
      name: `${deviceId} ${sensorConfig.friendlyName}`,
      state_topic: stateTopic,
      availability_topic: AVAILABILITY_TOPIC,
      payload_available: "online",
      payload_not_available: "offline",
      unique_id: uniqueId,
      device: {
        identifiers: [deviceId],
        name: `ESP ${deviceId}`,
        manufacturer: "Terrasmart",
        model: deviceType || "ESP Sensor",
        via_device: DEVICE_ID,
      },
    };

    // Adicionar propriedades específicas do tipo de sensor
    if (sensorConfig.unit_of_measurement) {
      config.unit_of_measurement = sensorConfig.unit_of_measurement;
    }

    if (sensorConfig.device_class) {
      config.device_class = sensorConfig.device_class;
    }

    if (sensorConfig.domain === "binary_sensor") {
      config.payload_on = sensorConfig.payload_on || "1";
      config.payload_off = sensorConfig.payload_off || "0";
    }

    mqttService.publish(discoveryTopic, JSON.stringify(config), {
      retain: true,
    });
  }

  /**
   * Publica discovery para o sensor de status de um ESP
   * @param {string} deviceId - ID do dispositivo ESP
   */
  publishStatusDiscovery(deviceId) {
    const uniqueId = `terrasmart_${deviceId}_status`;
    const discoveryTopic = `homeassistant/binary_sensor/${uniqueId}/config`;
    const stateTopic = `${ESP_BASE_TOPIC}/${deviceId}/status`;

    logger.info("Publicando discovery de status", { deviceId });

    mqttService.publish(
      discoveryTopic,
      JSON.stringify({
        name: `ESP ${deviceId} Online`,
        state_topic: stateTopic,
        payload_on: "online",
        payload_off: "offline",
        device_class: "connectivity",
        unique_id: uniqueId,
        device: {
          identifiers: [deviceId],
          name: `ESP ${deviceId}`,
          via_device: DEVICE_ID,
        },
      }),
      { retain: true }
    );
  }
}

module.exports = new DiscoveryService();
