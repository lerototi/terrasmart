#include <Arduino.h>
#include "terrasmart_mqtt_gateway_config.h"

#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>

const char *fdrs_runtime_ap_ssid = nullptr;
const char *fdrs_runtime_ap_password = "terrasmart";
#endif

#if defined(TERRASMART_OTA_MQTT)
void terrasSmartOtaServiceUart();
#endif

#if defined(TERRASMART_OTA_MQTT)
inline void terrasSmartOtaLoop();
#endif

#include "terrasmart_ota.h"
#include <fdrs_gateway.h>

#if !defined(LED_BUILTIN)
#define LED_BUILTIN 2
#endif

namespace {
constexpr uint8_t STATUS_LED = LED_BUILTIN;
constexpr unsigned long MQTT_STATUS_INTERVAL = 30000;
unsigned long lastStatusChange = 0;
bool statusLedState = false;
unsigned long lastMqttStatus = 0;
bool mqttStatusOnline = false;

#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
constexpr char CONFIG_NAMESPACE[] = "terrasmart";
constexpr char AP_PASSWORD[] = FDRS_AP_PASSWORD;
constexpr unsigned long WIFI_RETRY_INTERVAL = 30000;

Preferences preferences;
DNSServer dnsServer;
WebServer server(80);
String wifiSsid;
String wifiPassword;
String mqttHost;
String mqttUser;
String mqttPassword;
uint16_t mqttPort = 1883;
bool runtimeConfigured = false;
unsigned long lastWifiRetry = 0;
constexpr uint8_t CONFIG_RESET_PIN = 0;
unsigned long configResetStarted = 0;
bool configResetHandled = false;
bool networkInitialized = false;
bool captivePortalStarted = false;
bool mdnsStarted = false;
constexpr char MDNS_HOSTNAME[] = "terrasmart-gateway";

void applyConfiguration();
void reconfigureNetwork();

String apSsid;

String htmlEscape(const String &value) {
  String escaped = value;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  escaped.replace("\"", "&quot;");
  return escaped;
}

void buildApIdentity() {
  // Read the chip identity without touching the Wi-Fi/LWIP stack. At this
  // point setup() has not called beginFDRS() yet.
  const uint32_t chipSuffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFF);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04lX", static_cast<unsigned long>(chipSuffix));
  apSsid = String("TerraSmart-Setup-") + suffix;
  fdrs_runtime_ap_ssid = apSsid.c_str();
  fdrs_runtime_ap_password = AP_PASSWORD;
}

void saveConfiguration(bool configured) {
  preferences.putBool("configured", configured);
  preferences.putString("wifi_ssid", wifiSsid);
  preferences.putString("wifi_pass", wifiPassword);
  preferences.putString("mqtt_host", mqttHost);
  preferences.putUShort("mqtt_port", mqttPort);
  preferences.putString("mqtt_user", mqttUser);
  preferences.putString("mqtt_pass", mqttPassword);
  runtimeConfigured = configured;
}

void loadConfiguration() {
  preferences.begin(CONFIG_NAMESPACE, false);

  if (!preferences.isKey("configured")) {
    // Migrate the existing compile-time configuration on first boot.
    wifiSsid = WIFI_SSID;
    wifiPassword = WIFI_PASS;
    mqttHost = MQTT_ADDR;
    mqttPort = MQTT_PORT;
    mqttUser = MQTT_USER;
    mqttPassword = MQTT_PASS;
    saveConfiguration(true);
  } else {
    runtimeConfigured = preferences.getBool("configured", false);
    wifiSsid = preferences.getString("wifi_ssid", "");
    wifiPassword = preferences.getString("wifi_pass", "");
    mqttHost = preferences.getString("mqtt_host", "");
    mqttPort = preferences.getUShort("mqtt_port", MQTT_PORT);
    mqttUser = preferences.getString("mqtt_user", "");
    mqttPassword = preferences.getString("mqtt_pass", "");
  }

  if (!runtimeConfigured) {
    wifiSsid = "";
    wifiPassword = "";
    mqttHost = MQTT_ADDR;
    mqttPort = MQTT_PORT;
    mqttUser = MQTT_USER;
    mqttPassword = MQTT_PASS;
  }
}

void resetConfiguration() {
  preferences.clear();
  runtimeConfigured = false;
  wifiSsid = "";
  wifiPassword = "";
  mqttHost = MQTT_ADDR;
  mqttUser = MQTT_USER;
  mqttPassword = MQTT_PASS;
  mqttPort = MQTT_PORT;
}

void handlePhysicalReset() {
  pinMode(CONFIG_RESET_PIN, INPUT_PULLUP);
  if (digitalRead(CONFIG_RESET_PIN) != LOW) {
    return;
  }

  if (configResetStarted == 0) {
    configResetStarted = millis();
  }
  if (!configResetHandled && millis() - configResetStarted >= 3000) {
    resetConfiguration();
    saveConfiguration(false);
    reconfigureNetwork();
    configResetHandled = true;
  }
}

String jsonEscape(const String &value) {
  String escaped = value;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  escaped.replace("\n", "\\n");
  escaped.replace("\r", "\\r");
  return escaped;
}

void applyConfiguration() {
  ssid = wifiSsid.c_str();
  password = wifiPassword.c_str();
  mqtt_server = mqttHost.c_str();
  mqtt_port = mqttPort;
  mqtt_user = mqttUser.length() == 0 ? nullptr : mqttUser.c_str();
  mqtt_pass = mqttPassword.length() == 0 ? nullptr : mqttPassword.c_str();
}

void reconfigureNetwork() {
  applyConfiguration();
  if (!networkInitialized) {
    return;
  }

  client.disconnect();
  WiFi.mode(WIFI_AP_STA);
  if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    WiFi.softAP(fdrs_runtime_ap_ssid, fdrs_runtime_ap_password);
  }
  WiFi.disconnect(false, false);
  if (runtimeConfigured && wifiSsid.length() > 0) {
    WiFi.begin(ssid, password);
  }
}

void maintainMdns() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mdnsStarted && MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      mdnsStarted = true;
    }
    return;
  }

  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }
}

String networkStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    return "connected";
  }
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    return "setup_ap";
  }
  return "disconnected";
}

String statusJson() {
  String json = "{";
  json += "\"configured\":" + String(runtimeConfigured ? "true" : "false");
  json += ",\"network\":\"" + networkStatus() + "\"";
  json += ",\"ssid\":\"" + jsonEscape(wifiSsid) + "\"";
  json += ",\"sta_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"sta_ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"ap_enabled\":" + String(WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA ? "true" : "false");
  json += ",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"";
  json += ",\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  json += ",\"hostname\":\"" + String(MDNS_HOSTNAME) + ".local\"";
  json += ",\"mdns\":" + String(mdnsStarted ? "true" : "false");
  json += ",\"mqtt_connected\":" + String(client.connected() ? "true" : "false");
  json += ",\"mqtt_host\":\"" + jsonEscape(mqttHost) + "\"";
  json += ",\"mqtt_port\":" + String(mqttPort);
  json += "}";
  return json;
}

String wifiNetworksJson() {
  const wifi_mode_t previousMode = WiFi.getMode();
  if (previousMode == WIFI_AP) {
    WiFi.mode(WIFI_AP_STA);
  }

  const int networkCount = WiFi.scanNetworks(false, true);
  String json = "[";
  for (int i = 0; i < networkCount; ++i) {
    if (i > 0) {
      json += ",";
    }
    json += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i));
    json += "\",\"rssi\":" + String(WiFi.RSSI(i));
    json += ",\"secure\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true");
    json += "}";
  }
  json += "]";
  WiFi.scanDelete();

  if (networkCount < 0) {
    return "[]";
  }
  return json;
}

String page() {
  String body;
  body.reserve(5000);
  body += F("<!doctype html><html lang='pt-BR'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  body += F("<title>TerraSmart Gateway</title><style>body{font-family:system-ui;max-width:720px;margin:2rem auto;padding:0 1rem;background:#f5f7f2;color:#172018}label{display:block;margin-top:1rem;font-weight:600}input{box-sizing:border-box;width:100%;padding:.65rem;border:1px solid #aab5a8;border-radius:6px;font-size:1rem}button{margin-top:1.25rem;padding:.7rem 1rem;border:0;border-radius:6px;background:#176b45;color:white;font-weight:700}button.danger{background:#a52e2e}.card{background:white;padding:1rem 1.25rem;margin:1rem 0;border-radius:10px;box-shadow:0 1px 4px #0002}code{word-break:break-all}</style>");
  body += F("<h1>TerraSmart Gateway</h1><div class='card'><h2>Status</h2><pre id='status'>");
  body += statusJson();
  body += F("</pre><p>AP de configuracao: <code>");
  body += htmlEscape(apSsid);
  body += F("</code><br>Senha do AP: <code>");
  body += AP_PASSWORD;
  body += F("</code></p><p>AP permanente: <code>http://192.168.4.1/</code><br>mDNS STA: <code>http://terrasmart-gateway.local/</code></p></div><div class='card'><h2>Configuracao</h2><form method='post' action='/save'>");
  body += F("<label>Rede Wi-Fi<select id='wifi_ssid' name='wifi_ssid' required><option value=''>Buscando redes...</option></select></label><button type='button' onclick='scanWifi()'>Atualizar redes</button><label>Senha Wi-Fi<input name='wifi_pass' type='password' value='");
  body += htmlEscape(wifiPassword);
  body += F("'></label><label>Endereco MQTT<input name='mqtt_host' required value='");
  body += htmlEscape(mqttHost);
  body += F("'></label><label>Porta MQTT<input name='mqtt_port' type='number' min='1' max='65535' required value='");
  body += String(mqttPort);
  body += F("'></label><label>Usuario MQTT<input name='mqtt_user' value='");
  body += htmlEscape(mqttUser);
  body += F("'></label><label>Senha MQTT<input name='mqtt_pass' type='password' value='");
  body += htmlEscape(mqttPassword);
  body += F("'></label><button type='submit'>Salvar e reiniciar</button></form><form method='post' action='/reset'><button class='danger' type='submit'>Apagar configuracao e voltar ao AP</button></form></div><script>const selectedWifi=");
  body += "\"" + jsonEscape(wifiSsid) + "\"";
  body += F(";function scanWifi(){const s=document.getElementById('wifi_ssid');s.innerHTML=\"<option>Buscando redes...</option>\";fetch('/api/networks').then(r=>r.json()).then(ns=>{s.innerHTML='';if(!ns.length)s.innerHTML=\"<option value=\\\"\\\">Nenhuma rede encontrada</option>\";ns.forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+' dBm)'+(n.secure?' [senha]':' [aberta]');if(n.ssid===selectedWifi)o.selected=true;s.appendChild(o)})}).catch(()=>{s.innerHTML=\"<option value=\\\"\\\">Falha ao buscar redes</option>\"})}scanWifi();setInterval(()=>fetch('/api/status').then(r=>r.text()).then(t=>status.textContent=t),3000)</script></html>");
  return body;
}

void handleSave() {
  if (server.arg("wifi_ssid").length() == 0 || server.arg("mqtt_host").length() == 0) {
    server.send(400, "text/plain", "SSID e endereco MQTT sao obrigatorios.");
    return;
  }
  const long requestedPort = server.arg("mqtt_port").toInt();
  if (requestedPort < 1 || requestedPort > 65535) {
    server.send(400, "text/plain", "Porta MQTT invalida.");
    return;
  }
  wifiSsid = server.arg("wifi_ssid");
  wifiPassword = server.arg("wifi_pass");
  mqttHost = server.arg("mqtt_host");
  mqttPort = static_cast<uint16_t>(requestedPort);
  mqttUser = server.arg("mqtt_user");
  mqttPassword = server.arg("mqtt_pass");
  saveConfiguration(true);
  server.send(200, "text/html", "<p>Configuracao salva. O AP continua ativo enquanto o gateway tenta conectar ao Wi-Fi.</p><p><a href='/'>Voltar</a></p>");
  reconfigureNetwork();
}

void handleReset() {
  resetConfiguration();
  saveConfiguration(false);
  server.send(200, "text/html", "<p>Configuracao apagada. O AP continua ativo e o STA foi desconectado.</p><p><a href='/'>Voltar</a></p>");
  reconfigureNetwork();
}

void startWebServer() {
  server.on("/", HTTP_GET, []() { server.send(200, "text/html; charset=utf-8", page()); });
  server.on("/api/status", HTTP_GET, []() { server.send(200, "application/json", statusJson()); });
  server.on("/api/networks", HTTP_GET, []() { server.send(200, "application/json", wifiNetworksJson()); });
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset", HTTP_POST, handleReset);
  // Captive-portal probes use different URLs on Android, Apple, Windows, and
  // other clients. Redirect all HTTP requests to the local setup page.
  server.on("/generate_204", HTTP_GET, []() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", " captive portal"); });
  server.on("/gen_204", HTTP_GET, []() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", " captive portal"); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", " captive portal"); });
  server.on("/connecttest.txt", HTTP_GET, []() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", " captive portal"); });
  server.on("/ncsi.txt", HTTP_GET, []() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", " captive portal"); });
  server.on("/redirect", HTTP_GET, []() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", " captive portal"); });
  server.onNotFound([]() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", " captive portal"); });
  server.begin();
}

void maintainCaptivePortal() {
  const bool apActive = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
  if (!apActive || WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    captivePortalStarted = false;
    return;
  }

  if (!captivePortalStarted) {
    dnsServer.start(53, "*", WiFi.softAPIP());
    captivePortalStarted = true;
  }
  dnsServer.processNextRequest();
}

void maintainWifi() {
  if (!runtimeConfigured || WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(fdrs_runtime_ap_ssid, fdrs_runtime_ap_password);
  }
  if (millis() - lastWifiRetry >= WIFI_RETRY_INTERVAL) {
    lastWifiRetry = millis();
    WiFi.begin(ssid, password);
  }
}
#endif

String mqttStatusPayload(const char *event) {
  String json = "{";
  json += "\"event\":\"" + String(event) + "\"";
  json += ",\"uptime_s\":" + String(millis() / 1000UL);
  json += ",\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  json += ",\"mqtt_connected\":" + String(client.connected() ? "true" : "false");

#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  json += ",\"configured\":" + String(runtimeConfigured ? "true" : "false");
  json += ",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"";
  json += ",\"mqtt_host\":\"" + jsonEscape(mqttHost) + "\"";
  json += ",\"mqtt_port\":" + String(mqttPort);
#endif

  json += "}";
  return json;
}

void maintainMqttStatus() {
  if (!client.connected()) {
    mqttStatusOnline = false;
    return;
  }

  const unsigned long now = millis();
  if (mqttStatusOnline && now - lastMqttStatus < MQTT_STATUS_INTERVAL) {
    return;
  }

  const char *event = mqttStatusOnline ? "heartbeat" : "online";
  const String payload = mqttStatusPayload(event);
  if (client.publish(TOPIC_STATUS, payload.c_str(), true)) {
    lastMqttStatus = now;
    mqttStatusOnline = true;
  }
}

void setStatusLed(bool on) {
  // The built-in LED on common ESP boards is active-low.
  digitalWrite(STATUS_LED, on ? LOW : HIGH);
}

void updateStatusLed() {
  if (WiFi.status() == WL_CONNECTED && client.connected()) {
    setStatusLed(true);
    return;
  }

#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  const bool setupApActive = WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
  const bool staConnected = WiFi.status() == WL_CONNECTED;
  const unsigned long interval = staConnected ? 1000 : (setupApActive ? 500 : 250);
#else
  const unsigned long interval = WiFi.status() == WL_CONNECTED ? 1000 : 250;
#endif
  if (millis() - lastStatusChange >= interval) {
    lastStatusChange = millis();
    statusLedState = !statusLedState;
    setStatusLed(statusLedState);
  }
}

void startupPattern() {
  for (uint8_t i = 0; i < 3; ++i) {
    setStatusLed(true);
    delay(150);
    setStatusLed(false);
    delay(150);
  }
}
}

void setup() {
  pinMode(STATUS_LED, OUTPUT);
  setStatusLed(false);
  startupPattern();
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  buildApIdentity();
  loadConfiguration();
  handlePhysicalReset();
  applyConfiguration();
#endif
  beginFDRS();
#if defined(TERRASMART_OTA_MQTT)
  Serial1.setTimeout(20);
#endif
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  networkInitialized = true;
  startWebServer();
#endif
}

void loop() {
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  handlePhysicalReset();
  maintainCaptivePortal();
  server.handleClient();
  maintainWifi();
  maintainMdns();
#endif
  loopFDRS();
#if defined(TERRASMART_OTA_MQTT)
  terrasSmartOtaLoop();
#endif
  maintainMqttStatus();

  // The dedicated FDRS UART is reserved for JSON, so use the built-in LED
  // for status. On ESP8266 this is the main UART and cannot be monitored
  // while the gateway link is connected.
  updateStatusLed();
}
