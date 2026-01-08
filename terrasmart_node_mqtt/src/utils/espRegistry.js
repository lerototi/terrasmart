/**
 * Gerenciador do registry de dispositivos ESP
 */

class EspRegistry {
  constructor() {
    this.devices = {};
  }

  /**
   * Verifica se um dispositivo existe no registry
   * @param {string} deviceId - ID do dispositivo
   * @returns {boolean}
   */
  exists(deviceId) {
    return !!this.devices[deviceId];
  }

  /**
   * Obtém um dispositivo do registry
   * @param {string} deviceId - ID do dispositivo
   * @returns {Object|null}
   */
  get(deviceId) {
    return this.devices[deviceId] || null;
  }

  /**
   * Registra ou atualiza um dispositivo
   * @param {string} deviceId - ID do dispositivo
   * @param {Object} data - Dados do dispositivo
   */
  set(deviceId, data) {
    if (!this.devices[deviceId]) {
      this.devices[deviceId] = {
        discovered: false,
        lastSeen: null,
        deviceInfo: {},
        sensorTypes: new Set(),
        lastStatus: null,
        lastStatusTime: 0,
        ...data,
      };
    } else {
      this.devices[deviceId] = {
        ...this.devices[deviceId],
        ...data,
      };
    }
  }

  /**
   * Atualiza o timestamp de última comunicação
   * @param {string} deviceId - ID do dispositivo
   */
  updateLastSeen(deviceId) {
    if (this.devices[deviceId]) {
      this.devices[deviceId].lastSeen = Date.now();
    }
  }

  /**
   * Adiciona um tipo de sensor ao dispositivo
   * @param {string} deviceId - ID do dispositivo
   * @param {string} sensorType - Tipo de sensor
   * @returns {boolean} true se é um novo tipo
   */
  addSensorType(deviceId, sensorType) {
    if (!this.devices[deviceId]) {
      return false;
    }

    const isNew = !this.devices[deviceId].sensorTypes.has(sensorType);
    if (isNew) {
      this.devices[deviceId].sensorTypes.add(sensorType);
    }
    return isNew;
  }

  /**
   * Atualiza informações do dispositivo
   * @param {string} deviceId - ID do dispositivo
   * @param {Object} deviceInfo - Informações do dispositivo
   */
  updateDeviceInfo(deviceId, deviceInfo) {
    if (this.devices[deviceId]) {
      this.devices[deviceId].deviceInfo = deviceInfo;
    }
  }

  /**
   * Retorna lista de todos os IDs de dispositivos
   * @returns {string[]}
   */
  getAllDeviceIds() {
    return Object.keys(this.devices);
  }

  /**
   * Marca dispositivo como descoberto
   * @param {string} deviceId - ID do dispositivo
   */
  markAsDiscovered(deviceId) {
    if (this.devices[deviceId]) {
      this.devices[deviceId].discovered = true;
    }
  }
}

module.exports = new EspRegistry();
