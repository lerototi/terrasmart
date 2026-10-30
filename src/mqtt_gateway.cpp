#include <Arduino.h>
#include "terrasmart_mqtt_gateway_config.h"
#include "terrasmart_mqtt_identity.h"

const char *terrasSmartMqttClientId();
const char *terrasSmartMqttStatusTopic();
const char *terrasSmartMqttCommandTopic();
const char *terrasSmartMqttGatewayId();
const char *terrasSmartMqttOfflinePayload();
bool terrasSmartMqttCommandTopicMatches(const char *topic);
void terrasSmartMqttHandleGatewayCommand(const uint8_t *message,
                                         unsigned int length);

#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
#include <Preferences.h>
#endif
#if defined(FDRS_RUNTIME_CONFIG)
#include <DNSServer.h>
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
#include <ESPmDNS.h>
#include <WebServer.h>
#else
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#endif
#endif

#if defined(FDRS_RUNTIME_CONFIG)
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

#if defined(FDRS_RUNTIME_CONFIG) && defined(TERRASMART_OTA_MQTT)
void terrasSmartMqttHandleGatewayCommand(const uint8_t *message,
                                         unsigned int length);
#endif

#if !defined(LED_BUILTIN)
#define LED_BUILTIN 2
#endif

namespace {
constexpr uint8_t STATUS_LED = LED_BUILTIN;
constexpr unsigned long MQTT_STATUS_INTERVAL = 30000;
constexpr unsigned long OTA_STATUS_LED_BLINK_INTERVAL_MS = 400;
unsigned long lastStatusLedToggle = 0;
bool statusLedState = false;
bool otaLedActive = false;
unsigned long lastMqttStatus = 0;
bool mqttStatusOnline = false;
bool legacyStatusCleared = false;
bool commandTopicSubscribed = false;
bool mqttIdentityInitialized = false;
char mqttClientId[16] = {};
char mqttStatusTopic[64] = {};
char mqttCommandTopic[64] = {};
char mqttGatewayId[16] = {};
char mqttOfflinePayload[56] = {};

#if defined(FDRS_RUNTIME_CONFIG)
constexpr char CONFIG_NAMESPACE[] = "terrasmart";
constexpr char AP_PASSWORD[] = FDRS_AP_PASSWORD;
constexpr unsigned long WIFI_RETRY_INTERVAL = 30000;

#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
Preferences preferences;
WebServer server(80);
#else
constexpr size_t EEPROM_CONFIG_SIZE = 512;
constexpr uint32_t EEPROM_CONFIG_MAGIC = 0x54534346; // "TSCF"
struct PersistedConfig {
  uint32_t magic;
  uint8_t configured;
  char wifiSsid[33];
  char wifiPassword[65];
  char mqttHost[65];
  uint16_t mqttPort;
  char mqttUser[33];
  char mqttPassword[65];
  uint32_t checksum;
};
ESP8266WebServer server(80);
#endif
DNSServer dnsServer;
String wifiSsid;
String wifiPassword;
String mqttHost;
String mqttUser;
String mqttPassword;
uint16_t mqttPort = 1883;
bool runtimeConfigured = false;
unsigned long lastWifiRetry = 0;
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
constexpr uint8_t CONFIG_RESET_PIN = 0;
unsigned long configResetStarted = 0;
bool configResetHandled = false;
#endif
bool networkInitialized = false;
bool captivePortalStarted = false;
bool mdnsStarted = false;
constexpr char MDNS_HOSTNAME[] = "terrasmart-gateway";

void applyConfiguration();
void reconfigureNetwork();

String apSsid;

#if defined(ESP8266)
uint8_t wifiMac[6] = {};
uint32_t configChecksum(const PersistedConfig &config) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&config);
  uint32_t checksum = 2166136261UL;
  for (size_t i = 0; i < offsetof(PersistedConfig, checksum); ++i) {
    checksum = (checksum ^ bytes[i]) * 16777619UL;
  }
  return checksum;
}

bool copyField(char *destination, size_t capacity, const String &value) {
  if (value.length() >= capacity) return false;
  memset(destination, 0, capacity);
  value.toCharArray(destination, capacity);
  return true;
}
#endif

String htmlEscape(const String &value) {
  String escaped = value;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  escaped.replace("\"", "&quot;");
  return escaped;
}

void buildApIdentity() {
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  const uint32_t chipSuffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFF);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04lX", static_cast<unsigned long>(chipSuffix));
#else
  WiFi.macAddress(wifiMac);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02X%02X", wifiMac[4], wifiMac[5]);
#endif
  apSsid = String("TerraSmart-Setup-") + suffix;
  fdrs_runtime_ap_ssid = apSsid.c_str();
  fdrs_runtime_ap_password = AP_PASSWORD;
}

bool saveConfiguration(bool configured) {
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  const bool saved =
      preferences.putBool("configured", configured) > 0 &&
      preferences.putString("wifi_ssid", wifiSsid) > 0 &&
      preferences.putString("wifi_pass", wifiPassword) > 0 &&
      preferences.putString("mqtt_host", mqttHost) > 0 &&
      preferences.putUShort("mqtt_port", mqttPort) > 0 &&
      preferences.putString("mqtt_user", mqttUser) > 0 &&
      preferences.putString("mqtt_pass", mqttPassword) > 0;
  if (!saved) return false;
#elif defined(ESP8266)
  PersistedConfig saved = {};
  saved.magic = EEPROM_CONFIG_MAGIC;
  saved.configured = configured ? 1 : 0;
  saved.mqttPort = mqttPort;
  const String savedWifiSsid = configured ? wifiSsid : "";
  const String savedWifiPassword = configured ? wifiPassword : "";
  if (!copyField(saved.wifiSsid, sizeof(saved.wifiSsid), savedWifiSsid) ||
      !copyField(saved.wifiPassword, sizeof(saved.wifiPassword),
                 savedWifiPassword) ||
      !copyField(saved.mqttHost, sizeof(saved.mqttHost), mqttHost) ||
      !copyField(saved.mqttUser, sizeof(saved.mqttUser), mqttUser) ||
      !copyField(saved.mqttPassword, sizeof(saved.mqttPassword), mqttPassword))
    return false;
  saved.checksum = configChecksum(saved);
  EEPROM.put(0, saved);
  if (!EEPROM.commit()) return false;
#endif
  runtimeConfigured = configured;
  return true;
}

void loadConfiguration() {
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
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
#elif defined(ESP8266)
  EEPROM.begin(EEPROM_CONFIG_SIZE);
  PersistedConfig saved = {};
  EEPROM.get(0, saved);
  const bool valid = saved.magic == EEPROM_CONFIG_MAGIC &&
                     saved.checksum == configChecksum(saved) &&
                     saved.mqttPort > 0;
  if (valid) {
    runtimeConfigured = saved.configured == 1;
    wifiSsid = runtimeConfigured ? saved.wifiSsid : "";
    wifiPassword = saved.wifiPassword;
    mqttHost = saved.mqttHost;
    mqttPort = saved.mqttPort;
    mqttUser = saved.mqttUser;
    mqttPassword = saved.mqttPassword;
  } else {
    runtimeConfigured = false;
    wifiSsid = WIFI_SSID;
    wifiPassword = WIFI_PASS;
    mqttHost = MQTT_ADDR;
    mqttPort = MQTT_PORT;
    mqttUser = MQTT_USER;
    mqttPassword = MQTT_PASS;
    if (wifiSsid.length() && mqttHost.length()) saveConfiguration(true);
    else saveConfiguration(false);
  }
#endif

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
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  preferences.clear();
#elif defined(ESP8266)
  PersistedConfig empty = {};
  EEPROM.put(0, empty);
  EEPROM.commit();
#endif
  runtimeConfigured = false;
  wifiSsid = "";
  wifiPassword = "";
  mqttHost = MQTT_ADDR;
  mqttUser = MQTT_USER;
  mqttPassword = MQTT_PASS;
  mqttPort = MQTT_PORT;
}

void handlePhysicalReset() {
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
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
#endif
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
  if (!runtimeConfigured) {
    wifiSsid = "";
    applyConfiguration();
  }
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
  json += "\"gateway_id\":\"" + String(terrasSmartMqttGatewayId()) + "\"";
  json += ",\"command_topic\":\"" + String(terrasSmartMqttCommandTopic()) + "\"";
  json += ",\"configured\":" + String(runtimeConfigured ? "true" : "false");
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
  json += ",\"mac\":\"" + WiFi.macAddress() + "\"";
  json += "}";
  return json;
}

String wifiNetworksJson() {
  const auto previousMode = WiFi.getMode();
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
#if defined(ESP8266)
    const bool secure = WiFi.encryptionType(i) != AUTH_OPEN;
#else
    const bool secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
#endif
    json += ",\"secure\":" + String(secure ? "true" : "false");
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
  body.reserve(5700);
  body += F("<!doctype html><html lang='pt-BR'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  body += F("<title>TerraSmart Gateway</title><style>body{font-family:system-ui;max-width:720px;margin:2rem auto;padding:0 1rem;background:#f5f7f2;color:#172018}label{display:block;margin-top:1rem;font-weight:600}input{box-sizing:border-box;width:100%;padding:.65rem;border:1px solid #aab5a8;border-radius:6px;font-size:1rem}button{margin-top:1.25rem;padding:.7rem 1rem;border:0;border-radius:6px;background:#176b45;color:white;font-weight:700}button.danger{background:#a52e2e}.card{background:white;padding:1rem 1.25rem;margin:1rem 0;border-radius:10px;box-shadow:0 1px 4px #0002}code{word-break:break-all}</style>");
  body += F("<h1>TerraSmart Gateway</h1><div class='card'><h2>Status</h2><p>ID MQTT: <code>");
  body += terrasSmartMqttGatewayId();
  body += F("</code><br>MAC Wi-Fi: <code>");
  body += WiFi.macAddress();
  body += F("</code><br>Topico de comandos FDRS: <code>");
  body += terrasSmartMqttCommandTopic();
  body += F("</code></p><pre id='status'>");
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
  const String requestedWifiSsid = server.arg("wifi_ssid");
  const String requestedWifiPassword = server.arg("wifi_pass");
  const String requestedMqttHost = server.arg("mqtt_host");
  const String requestedMqttUser = server.arg("mqtt_user");
  const String requestedMqttPassword = server.arg("mqtt_pass");
#if defined(ESP8266)
  PersistedConfig validation = {};
  const bool fieldsFit =
      copyField(validation.wifiSsid, sizeof(validation.wifiSsid),
                requestedWifiSsid) &&
      copyField(validation.wifiPassword, sizeof(validation.wifiPassword),
                requestedWifiPassword) &&
      copyField(validation.mqttHost, sizeof(validation.mqttHost),
                requestedMqttHost) &&
      copyField(validation.mqttUser, sizeof(validation.mqttUser),
                requestedMqttUser) &&
      copyField(validation.mqttPassword, sizeof(validation.mqttPassword),
                requestedMqttPassword);
  if (!fieldsFit) {
    server.send(400, "text/plain", "Um ou mais campos excedem o limite permitido.");
    return;
  }
#endif
  const String previousWifiSsid = wifiSsid;
  const String previousWifiPassword = wifiPassword;
  const String previousMqttHost = mqttHost;
  const String previousMqttUser = mqttUser;
  const String previousMqttPassword = mqttPassword;
  const uint16_t previousMqttPort = mqttPort;
  const bool previousRuntimeConfigured = runtimeConfigured;
  wifiSsid = requestedWifiSsid;
  wifiPassword = requestedWifiPassword;
  mqttHost = requestedMqttHost;
  mqttPort = static_cast<uint16_t>(requestedPort);
  mqttUser = requestedMqttUser;
  mqttPassword = requestedMqttPassword;
  if (!saveConfiguration(true)) {
    wifiSsid = previousWifiSsid;
    wifiPassword = previousWifiPassword;
    mqttHost = previousMqttHost;
    mqttPort = previousMqttPort;
    mqttUser = previousMqttUser;
    mqttPassword = previousMqttPassword;
    runtimeConfigured = previousRuntimeConfigured;
    server.send(500, "text/plain", "Falha ao salvar a configuracao.");
    return;
  }
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
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(fdrs_runtime_ap_ssid, fdrs_runtime_ap_password);
  }
  if (runtimeConfigured && wifiSsid.length() > 0 &&
      millis() - lastWifiRetry >= WIFI_RETRY_INTERVAL) {
    lastWifiRetry = millis();
    WiFi.begin(ssid, password);
  }
}
#endif

void initializeMqttIdentity() {
  if (mqttIdentityInitialized) {
    return;
  }

  uint8_t mac[6] = {};
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  WiFi.macAddress(mac);
#else
  memcpy(mac, wifiMac, sizeof(mac));
#endif
  if (!terrasSmart::formatMqttClientId(mac, mqttClientId,
                                       sizeof(mqttClientId)) ||
      !terrasSmart::formatMqttStatusTopic(mac, mqttStatusTopic,
                                          sizeof(mqttStatusTopic)) ||
      !terrasSmart::formatMqttCommandTopic(mac, mqttCommandTopic,
                                           sizeof(mqttCommandTopic)) ||
      !terrasSmart::formatMqttGatewayId(mac, mqttGatewayId,
                                        sizeof(mqttGatewayId))) {
    mqttClientId[0] = '\0';
    mqttStatusTopic[0] = '\0';
    mqttCommandTopic[0] = '\0';
    mqttGatewayId[0] = '\0';
    mqttOfflinePayload[0] = '\0';
    return;
  }
  if (!terrasSmart::formatMqttOfflinePayload(
          mqttGatewayId, mqttOfflinePayload, sizeof(mqttOfflinePayload))) {
    mqttIdentityInitialized = false;
    mqttClientId[0] = '\0';
    mqttStatusTopic[0] = '\0';
    mqttCommandTopic[0] = '\0';
    mqttGatewayId[0] = '\0';
    mqttOfflinePayload[0] = '\0';
    return;
  }
  mqttIdentityInitialized = true;
}

String mqttStatusPayload(const char *event) {
  initializeMqttIdentity();
  String json = "{";
  json += "\"event\":\"" + String(event) + "\"";
  json += ",\"gateway_id\":\"" + String(mqttGatewayId) + "\"";
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
  initializeMqttIdentity();
  if (!mqttIdentityInitialized) {
    mqttStatusOnline = false;
    return;
  }
  if (!client.connected()) {
    mqttStatusOnline = false;
    commandTopicSubscribed = false;
    return;
  }

  if (!legacyStatusCleared &&
      client.publish("terrasmart/status", "", true)) {
    legacyStatusCleared = true;
  }

  if (!commandTopicSubscribed) {
    commandTopicSubscribed = client.subscribe(terrasSmartMqttCommandTopic());
#if defined(ESP8266)
    if (commandTopicSubscribed && !legacyStatusCleared) {
      client.unsubscribe(TOPIC_COMMAND);
    }
#endif
  }

  const unsigned long now = millis();
  if (mqttStatusOnline && now - lastMqttStatus < MQTT_STATUS_INTERVAL) {
    return;
  }

  const char *event = mqttStatusOnline ? "heartbeat" : "online";
  const String payload = mqttStatusPayload(event);
  if (client.publish(terrasSmartMqttStatusTopic(), payload.c_str(), true)) {
    lastMqttStatus = now;
    mqttStatusOnline = true;
  }
}

void setStatusLed(bool on) {
#if defined(ESP8266)
  // The D1 mini Pro's built-in LED is active-low.
  digitalWrite(STATUS_LED, on ? LOW : HIGH);
#else
  digitalWrite(STATUS_LED, on ? HIGH : LOW);
#endif
}

void updateStatusLed() {
  const unsigned long now = millis();
  const bool updating = terrasSmartOtaUpdateInProgress();
  if (updating) {
    otaLedActive = true;
    if (now - lastStatusLedToggle >= OTA_STATUS_LED_BLINK_INTERVAL_MS) {
      lastStatusLedToggle = now;
      statusLedState = !statusLedState;
      setStatusLed(statusLedState);
    }
    return;
  }

  otaLedActive = false;
  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  const bool mqttConnected = client.connected();
  if (wifiConnected && mqttConnected) {
    statusLedState = true;
    setStatusLed(true);
    return;
  }

  const unsigned long blinkInterval =
      wifiConnected ? 800 :
      (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) ? 300 : 150;
  if (now - lastStatusLedToggle >= blinkInterval) {
    lastStatusLedToggle = now;
    statusLedState = !statusLedState;
    setStatusLed(statusLedState);
  }
}
}

const char *terrasSmartMqttClientId() {
  initializeMqttIdentity();
  return mqttIdentityInitialized ? mqttClientId : "";
}

const char *terrasSmartMqttStatusTopic() {
  initializeMqttIdentity();
  return mqttIdentityInitialized ? mqttStatusTopic : "";
}

const char *terrasSmartMqttCommandTopic() {
  initializeMqttIdentity();
  return mqttIdentityInitialized ? mqttCommandTopic : "";
}

bool terrasSmartMqttCommandTopicMatches(const char *topic) {
  if (topic == nullptr) return false;
  initializeMqttIdentity();
  return mqttIdentityInitialized && strcmp(topic, mqttCommandTopic) == 0;
}

const char *terrasSmartMqttGatewayId() {
  initializeMqttIdentity();
  return mqttIdentityInitialized ? mqttGatewayId : "";
}

const char *terrasSmartMqttOfflinePayload() {
  initializeMqttIdentity();
  return mqttIdentityInitialized ? mqttOfflinePayload : "{\"event\":\"offline\"}";
}

void terrasSmartOtaServiceStatusLed() {
  updateStatusLed();
}

void setup() {
  pinMode(STATUS_LED, OUTPUT);
  setStatusLed(false);
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  buildApIdentity();
  loadConfiguration();
  handlePhysicalReset();
  applyConfiguration();
#elif defined(ESP8266) && defined(FDRS_RUNTIME_CONFIG)
  WiFi.mode(WIFI_STA);
  WiFi.macAddress(wifiMac);
  buildApIdentity();
  loadConfiguration();
  applyConfiguration();
#endif
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  // Keep complete hex-encoded OTA UART frames in the RX ring if loopFDRS()
  // is briefly busy while the peer transmits.
  Serial1.setRxBufferSize(1024);
#endif
  beginFDRS();
#if defined(TERRASMART_OTA_MQTT)
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  Serial1.setTimeout(20);
#else
  Serial.setTimeout(20);
#endif
#endif
#if defined(FDRS_RUNTIME_CONFIG)
  networkInitialized = true;
  startWebServer();
#endif
}

void loop() {
#if defined(FDRS_RUNTIME_CONFIG)
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  handlePhysicalReset();
#endif
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

void terrasSmartMqttHandleGatewayCommand(const uint8_t *message,
                                         unsigned int length) {
  if (message == nullptr || length == 0) return;
  JsonDocument doc;
  if (deserializeJson(doc, message, length)) return;
  const JsonArray readings = doc.as<JsonArray>();
  if (readings.isNull() || readings.size() == 0 || readings.size() > 255) return;

  for (size_t i = 0; i < readings.size(); ++i) {
    JsonObjectConst reading = readings[i].as<JsonObjectConst>();
    if (!reading["id"].is<uint16_t>() || !reading["type"].is<uint8_t>() ||
        !reading["data"].is<float>()) {
      return;
    }
  }

  for (size_t i = 0; i < readings.size(); ++i) {
    theData[i].id = readings[i]["id"].as<uint16_t>();
    theData[i].t = readings[i]["type"].as<uint8_t>();
    theData[i].d = readings[i]["data"].as<float>();
  }
  ln = static_cast<uint8_t>(readings.size());
  newData = event_mqtt;
}
