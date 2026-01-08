const express = require("express");
const logger = require("../utils/logger");
const espRegistry = require("../utils/espRegistry");
const { DEVICE_NAME } = require("../config/constants");

/**
 * Servidor HTTP para keep-alive e status
 */
class HttpServer {
  constructor() {
    this.app = express();
    this.server = null;
    this._setupRoutes();
  }

  /**
   * Configura as rotas do servidor
   * @private
   */
  _setupRoutes() {
    // Rota de status principal
    this.app.get("/", (req, res) => {
      res.json({
        status: "ok",
        addon: DEVICE_NAME,
        devices: espRegistry.getAllDeviceIds(),
      });
    });

    // Rota de health check
    this.app.get("/health", (req, res) => {
      res.json({
        status: "healthy",
        uptime: process.uptime(),
        timestamp: new Date().toISOString(),
      });
    });

    // Rota para listar dispositivos
    this.app.get("/devices", (req, res) => {
      const devices = espRegistry.getAllDeviceIds().map((id) => {
        const device = espRegistry.get(id);
        return {
          id,
          info: device.deviceInfo,
          lastSeen: device.lastSeen,
          status: device.status,
          sensorTypes: Array.from(device.sensorTypes || []),
        };
      });

      res.json({ devices });
    });
  }

  /**
   * Inicia o servidor HTTP
   * @param {number} port - Porta para escutar
   */
  start(port) {
    this.server = this.app.listen(port, () => {
      logger.info("HTTP server ativo", { porta: port });
    });
  }

  /**
   * Para o servidor HTTP
   */
  stop() {
    if (this.server) {
      this.server.close(() => {
        logger.info("HTTP server parado");
      });
    }
  }
}

module.exports = new HttpServer();
