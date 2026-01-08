/**
 * Configurações de sensores para Home Assistant MQTT Discovery
 */

/**
 * Retorna a configuração de um tipo de sensor para o Home Assistant
 * @param {string} sensorType - Tipo do sensor (temperature, humidity, etc)
 * @param {string} unit - Unidade de medida
 * @returns {Object} Configuração do sensor
 */
function getSensorConfig(sensorType, unit) {
  const configs = {
    temperature: {
      domain: "sensor",
      friendlyName: "Temperatura",
      device_class: "temperature",
      unit_of_measurement: unit || "°C",
    },
    humidity: {
      domain: "sensor",
      friendlyName: "Umidade",
      device_class: "humidity",
      unit_of_measurement: unit || "%",
    },
    distance: {
      domain: "sensor",
      friendlyName: "Distância",
      device_class: "distance",
      unit_of_measurement: unit || "cm",
    },
    level: {
      domain: "sensor",
      friendlyName: "Nível",
      device_class: null,
      unit_of_measurement: unit || "%",
    },
    state: {
      domain: "binary_sensor",
      friendlyName: "Estado",
      device_class: "opening",
      payload_on: "1",
      payload_off: "0",
    },
    motion: {
      domain: "binary_sensor",
      friendlyName: "Movimento",
      device_class: "motion",
      payload_on: "1",
      payload_off: "0",
    },
    pressure: {
      domain: "sensor",
      friendlyName: "Pressão",
      device_class: "pressure",
      unit_of_measurement: unit || "hPa",
    },
    flow: {
      domain: "sensor",
      friendlyName: "Fluxo",
      device_class: null,
      unit_of_measurement: unit || "L/min",
    },
  };

  // Retornar configuração específica ou genérica
  return (
    configs[sensorType] || {
      domain: "sensor",
      friendlyName:
        sensorType.charAt(0).toUpperCase() + sensorType.slice(1),
      device_class: null,
      unit_of_measurement: unit || "",
    }
  );
}

module.exports = { getSensorConfig };
