#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "captive_portal.h"
#include "../config/config_manager.h"

ESP8266WebServer server(80);
DNSServer dnsServer;

const char* AP_SSID = "terrasmart-setup";

const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>TerraSmart Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: white;
      padding: 40px;
      border-radius: 12px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.2);
      max-width: 500px;
      width: 100%;
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 28px;
    }
    .subtitle {
      color: #666;
      margin-bottom: 30px;
      font-size: 14px;
    }
    .form-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      color: #333;
      font-weight: 600;
      margin-bottom: 8px;
      font-size: 14px;
    }
    select, input {
      width: 100%;
      padding: 12px;
      border: 2px solid #e0e0e0;
      border-radius: 6px;
      font-size: 14px;
      transition: border-color 0.3s;
      font-family: inherit;
    }
    select:focus, input:focus {
      outline: none;
      border-color: #667eea;
    }
    select {
      background-color: white;
      cursor: pointer;
      appearance: none;
      background-image: url("data:image/svg+xml;charset=UTF-8,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='%23667eea' stroke-width='2'%3e%3cpolyline points='6 9 12 15 18 9'%3e%3c/polyline%3e%3c/svg%3e");
      background-repeat: no-repeat;
      background-position: right 10px center;
      background-size: 20px;
      padding-right: 35px;
    }
    .loading {
      display: none;
      text-align: center;
      color: #667eea;
      font-size: 14px;
      margin-bottom: 20px;
    }
    .spinner {
      display: inline-block;
      width: 12px;
      height: 12px;
      border: 2px solid #f3f3f3;
      border-top: 2px solid #667eea;
      border-radius: 50%;
      animation: spin 1s linear infinite;
      margin-right: 8px;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    .wifi-icon {
      margin-right: 5px;
    }
    button {
      width: 100%;
      padding: 14px;
      margin-top: 10px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      border-radius: 6px;
      cursor: pointer;
      font-size: 16px;
      font-weight: 600;
      transition: transform 0.2s, box-shadow 0.2s;
    }
    button:hover {
      transform: translateY(-2px);
      box-shadow: 0 5px 20px rgba(102, 126, 234, 0.4);
    }
    button:active {
      transform: translateY(0);
    }
    .info {
      background: #f0f4ff;
      border-left: 4px solid #667eea;
      padding: 12px;
      margin-top: 20px;
      border-radius: 4px;
      font-size: 12px;
      color: #555;
    }
    .error {
      display: none;
      background: #fee;
      border-left: 4px solid #f44;
      padding: 12px;
      margin-bottom: 20px;
      border-radius: 4px;
      color: #c00;
      font-size: 14px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🌐 TerraSmart Setup</h1>
    <p class="subtitle">Configure seu dispositivo para conectar à Internet</p>
    
    <div class="error" id="error"></div>
    <div class="loading" id="loading">
      <span class="spinner"></span>Escaneando redes...
    </div>
    
    <form method="POST" action="/save" id="form">
      <div class="form-group">
        <label for="ssid">
          <span class="wifi-icon">📡</span>Selecione sua Rede Wi-Fi:
        </label>
        <select id="ssid" name="ssid" required onchange="updateSignal()">
          <option value="">Carregando redes disponíveis...</option>
        </select>
      </div>
      
      <div class="form-group">
        <label for="pass">
          <span class="wifi-icon">🔐</span>Senha do Wi-Fi:
        </label>
        <input type="password" id="pass" name="pass" required placeholder="Digite a senha">
      </div>
      
      <button type="submit">Conectar</button>
    </form>
    
    <div class="info">
      ℹ️ Seu dispositivo é <strong>TerraSmart Setup</strong>. Após conectar, será sincronizado com sua rede.
    </div>
  </div>

  <script>
    let networks = [];
    
    async function loadNetworks() {
      document.getElementById('loading').style.display = 'block';
      try {
        const response = await fetch('/scan');
        const data = await response.json();
        networks = data.networks || [];
        
        const select = document.getElementById('ssid');
        select.innerHTML = networks.length > 0 
          ? networks.map(net => `<option value="${net.ssid}">${net.ssid} (${net.rssi}dBm)</option>`).join('')
          : '<option value="">Nenhuma rede encontrada</option>';
        
        if (networks.length === 0) {
          document.getElementById('error').textContent = 'Nenhuma rede Wi-Fi encontrada. Tente novamente.';
          document.getElementById('error').style.display = 'block';
        }
      } catch (e) {
        document.getElementById('error').textContent = 'Erro ao escanear redes: ' + e.message;
        document.getElementById('error').style.display = 'block';
        document.getElementById('ssid').innerHTML = '<option value="">Erro ao carregar redes</option>';
      }
      document.getElementById('loading').style.display = 'none';
    }
    
    function updateSignal() {
      const select = document.getElementById('ssid');
      const selectedSsid = select.value;
      // Aqui você pode adicionar lógica para atualizar o sinal, se necessário
    }
    
    // Carregar redes ao abrir a página
    window.addEventListener('load', loadNetworks);
    
    // Recarregar redes a cada 30 segundos
    setInterval(loadNetworks, 30000);
  </script>
</body>
</html>
)rawliteral";

void handlePortal() {
  Serial.printf("[HTTP] Requisição: %s\n", server.uri().c_str());
  Serial.printf("[HTTP] Client IP: %s\n", server.client().remoteIP().toString().c_str());
  server.send(200, "text/html", PORTAL_HTML);
}

void handleScan() {
  Serial.println("[SCAN] Iniciando varredura de redes Wi-Fi...");
  
  // Fazer scan de redes
  int n = WiFi.scanNetworks();
  
  String json = "{\"networks\":[";
  
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      int sec = WiFi.encryptionType(i);
      
      // Escapar aspas no SSID
      ssid.replace("\"", "\\\"");
      
      json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(rssi) + ",\"sec\":" + String(sec) + "}";
      
      Serial.printf("[SCAN] %d. %s (%d dBm)\n", i+1, WiFi.SSID(i).c_str(), rssi);
    }
  }
  
  json += "]}";
  
  server.send(200, "application/json", json);
  Serial.printf("[SCAN] Total de redes encontradas: %d\n", n);
}

void handleSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  if (ssid.length() == 0) {
    server.send(400, "text/html", "Erro: SSID não pode estar vazio!");
    return;
  }

  DeviceConfig cfg;
  cfg.wifiSsid = ssid;
  cfg.wifiPass = pass;

  if (saveConfig(cfg)) {
    Serial.printf("[CAPTIVE] Config salva: SSID=%s\n", ssid.c_str());
    server.send(200, "text/html", R"(
      <html>
      <head><meta charset="UTF-8"></head>
      <body style="text-align:center; padding:50px;">
        <h2>✓ Configuração Salva!</h2>
        <p>Reiniciando o dispositivo...</p>
        <p style="color:#666; font-size:12px;">Conecte-se à sua rede Wi-Fi</p>
      </body>
      </html>
    )");
    delay(2000);
    ESP.restart();
  } else {
    server.send(500, "text/html", "Erro ao salvar configuração!");
  }
}

void startCaptivePortal() {
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  
  // Configurar IP estático para o AP
  IPAddress localIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(localIP, gateway, subnet);
  
  WiFi.softAP(AP_SSID);

  delay(1000);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║     MODO AP - PORTAL CAPTIVO       ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.printf("[AP] SSID: %s\n", AP_SSID);
  Serial.printf("[AP] IP: 192.168.4.1\n");
  Serial.println("[AP] Gateway: 192.168.4.1");
  Serial.println("[AP] Conecte ao Wi-Fi 'terrasmart-setup'");
  Serial.println("[AP] Acesse: http://192.168.4.1 ou http://setup.local\n");

  // Iniciar DNS Server - captura TODAS as requisições DNS
  dnsServer.start(53, "*", localIP);

  server.on("/", handlePortal);
  server.on("/scan", handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/generate_204", handlePortal);  // Para Android
  server.on("/fwlink", handlePortal);        // Para Windows
  server.on("/connecttest.txt", handlePortal); // Para Windows
  server.on("/hotspot-detect.html", handlePortal); // Para iOS
  server.on("/canonical.html", handlePortal); // Para iOS
  server.onNotFound(handlePortal);
  
  server.begin();
  Serial.println("[AP] Servidor Web iniciado na porta 80\n");
}

void captiveLoop() {
  // Processar requisições DNS
  dnsServer.processNextRequest();
  
  // Processar requisições HTTP
  server.handleClient();
  
  // Dar tempo para outros processos
  delay(2);
}