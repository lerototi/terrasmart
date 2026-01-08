/**
 * Constantes da aplicação
 */

module.exports = {
  // Identificação do Add-on
  DEVICE_ID: "terrasmart_addon",
  DEVICE_NAME: "Terrasmart Add-on",

  // Tópicos MQTT - Disponibilidade
  AVAILABILITY_TOPIC: "addon/availability",
  STATUS_TOPIC: "addon/status",

  // Tópicos MQTT - ESP (Legado)
  ESP_TELEMETRY_TOPIC: "addon/esp/telemetry",
  ESP_TEMPERATURE_TOPIC: "addon/esp/temperature",

  // Tópicos MQTT - ESP (Novo Padrão)
  ESP_BASE_TOPIC: "addon/esp",

  // Tópicos MQTT - Comandos
  CMD_TOPIC: "addon/cmd",
  RESP_TOPIC: "addon/resp",

  // Configurações de Debounce
  STATUS_DEBOUNCE_TIME: 5000, // 5 segundos

  // Paths
  CONFIG_PATH: "/data/options.json",
};
