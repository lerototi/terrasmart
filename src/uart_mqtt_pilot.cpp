#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#else
#error "UART/MQTT pilot requires ESP8266 or ESP32"
#endif
#include <PubSubClient.h>
#include <math.h>
#include "terrasmart_uart_v1.h"
#include "terrasmart_data_v1.h"
#include "terrasmart_discovery_packet.h"
#include "terrasmart_mqtt_identity.h"

// Credentials are local build inputs. This pilot does not read or overwrite the
// existing gateway configuration and deliberately does not implement OTA.
#if __has_include("terrasmart_pilot_secrets.h")
#include "terrasmart_pilot_secrets.h"
#endif
#if defined(PILOT_USE_SHARED_SECRETS) && __has_include("terrasmart_secrets.h")
#include "terrasmart_secrets.h"
#define PILOT_WIFI_SSID WIFI_SSID
#define PILOT_WIFI_PASSWORD WIFI_PASS
#define PILOT_MQTT_HOST MQTT_ADDR
#define PILOT_MQTT_PORT MQTT_PORT
#define PILOT_MQTT_USER MQTT_USER
#define PILOT_MQTT_PASSWORD MQTT_PASS
#endif
#ifndef PILOT_WIFI_SSID
#define PILOT_WIFI_SSID ""
#define PILOT_WIFI_PASSWORD ""
#define PILOT_MQTT_HOST ""
#define PILOT_MQTT_PORT 1883
#define PILOT_MQTT_USER ""
#define PILOT_MQTT_PASSWORD ""
#endif
#ifndef FDRS_AP_PASSWORD
#define FDRS_AP_PASSWORD "configure-me"
#endif

#if defined(ESP32) || defined(ESP8266)
#define FDRS_RUNTIME_CONFIG
#if defined(ESP32)
#define ESP32_MQTT_GATEWAY_FIRMWARE
#endif
#define TERRASMART_PILOT_PORTAL
#ifndef WIFI_SSID
#define WIFI_SSID PILOT_WIFI_SSID
#define WIFI_PASS PILOT_WIFI_PASSWORD
#define MQTT_ADDR PILOT_MQTT_HOST
#define MQTT_PORT PILOT_MQTT_PORT
#define MQTT_USER PILOT_MQTT_USER
#define MQTT_PASS PILOT_MQTT_PASSWORD
#endif
WiFiClient wifiClient;
PubSubClient client(wifiClient);
const char *ssid = nullptr, *password = nullptr, *mqtt_server = nullptr;
const char *mqtt_user = nullptr, *mqtt_pass = nullptr;
int mqtt_port = 1883;
char pilotGatewayId[16] = {};
const char *terrasSmartMqttGatewayId() { return pilotGatewayId; }
#include "terrasmart_mqtt_gateway_shared.inc"
#endif

namespace {
constexpr uint8_t expectedRadioMac[6] = {PILOT_GATEWAY_MAC_0, PILOT_GATEWAY_MAC_1,
    PILOT_GATEWAY_MAC_2, PILOT_GATEWAY_MAC_3, PILOT_GATEWAY_MAC_4, PILOT_GATEWAY_MAC_5};
constexpr uint8_t QUEUE_SIZE = 8;
struct Item { terras_smart_data_v1::Frame frame; };
Item queue[QUEUE_SIZE];
uint8_t head = 0, tail = 0;
uint32_t dropped = 0, invalid = 0, framingRejected = 0, duplicate = 0, published = 0;
uint32_t uartBytes = 0, uartMarkers = 0;
uint32_t lastReport = 0, nextMqttAttempt = 0;
uint8_t ownMac[6] = {};
char clientId[16] = {}, gatewayId[16] = {};
PubSubClient &mqtt = client;
#if defined(PILOT_USB_UART_TEST)
HardwareSerial &pilotUart = Serial;
#elif defined(ESP32)
HardwareSerial pilotUart(1); // RX GPIO16, TX GPIO17
#else
SoftwareSerial pilotUart(D6, D7); // RX GPIO12, TX GPIO13
#endif
terras_smart_uart_v1::Parser parser;

// Limited recent-message cache: records only after successful publish.
struct Key { bool used; uint8_t source[6]; uint32_t boot, seq; };
Key cache[16] = {};
uint8_t nextKey = 0;
bool isDuplicate(const terras_smart_data_v1::Frame &f) {
  for (const Key &k : cache)
    if (k.used && k.boot == f.bootId && k.seq == f.sequence && !memcmp(k.source, f.source, 6)) return true;
  for (uint8_t i = tail; i != head; i = (i + 1) % QUEUE_SIZE) {
    const auto &q = queue[i].frame;
    if (q.bootId == f.bootId && q.sequence == f.sequence && !memcmp(q.source, f.source, 6)) return true;
  }
  return false;
}
void receivePayload(const uint8_t *payload) {
  terras_smart_data_v1::Frame frame = {};
  if (memcmp(payload, expectedRadioMac, 6) ||
      !terras_smart_data_v1::decode(payload + 6, terras_smart_data_v1::FRAME_SIZE,
          TERRASMART_DISCOVERY_NETWORK_ID, expectedRadioMac, frame) || !isfinite(frame.reading.data)) {
    ++invalid; return;
  }
  if (isDuplicate(frame)) { ++duplicate; return; }
  uint8_t next = (head + 1) % QUEUE_SIZE;
  if (next == tail) { ++dropped; return; }
  queue[head].frame = frame; head = next;
}
void readUart() {
  uint8_t payload[terras_smart_uart_v1::PAYLOAD_SIZE];
  while (pilotUart.available()) {
    const char byte = static_cast<char>(pilotUart.read());
    ++uartBytes;
    if (byte == '!') ++uartMarkers;
    auto result = parser.feedResult(byte, millis(), payload);
    if (result == terras_smart_uart_v1::Parser::Result::FRAME) receivePayload(payload);
    else if (result == terras_smart_uart_v1::Parser::Result::REJECTED) ++framingRejected;
  }
  parser.expire(millis());
}
void publishPending() {
  if (!mqtt.connected() || head == tail) return;
  const auto &f = queue[tail].frame;
  char value[64];
  // Nine significant decimal digits round-trip a binary32 value.
  snprintf(value, sizeof(value), "%.9g", static_cast<double>(f.reading.data));
  char json[320];
  int n = snprintf(json, sizeof(json),
      "{\"schema_version\":1,\"node_id\":\"node-%02X%02X%02X%02X%02X%02X\","
      "\"gateway_id\":\"%s\",\"radio_gateway_id\":\"rgw-%02X%02X%02X%02X%02X%02X\","
      "\"boot_id\":%lu,\"sequence\":%lu,\"hops\":0,"
      "\"reading\":{\"id\":%u,\"type\":%u,\"data\":%s}}",
      f.source[0], f.source[1], f.source[2], f.source[3], f.source[4], f.source[5], gatewayId,
      expectedRadioMac[0], expectedRadioMac[1], expectedRadioMac[2], expectedRadioMac[3], expectedRadioMac[4], expectedRadioMac[5],
      static_cast<unsigned long>(f.bootId), static_cast<unsigned long>(f.sequence),
      f.reading.id, f.reading.type, value);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(json)) { ++invalid; tail = (tail + 1) % QUEUE_SIZE; return; }
  if (!mqtt.publish("terrasmart/data/v1", json, false)) return;
  Key &k = cache[nextKey]; k.used = true; memcpy(k.source, f.source, 6);
  k.boot = f.bootId; k.seq = f.sequence; nextKey = (nextKey + 1) % 16;
  tail = (tail + 1) % QUEUE_SIZE; ++published;
  Serial.print("MQTT published: "); Serial.println(json);
}
} // namespace

void setup() {
  Serial.begin(115200); delay(100);
  Serial.println("\nTerraSmart isolated UART/MQTT data v1 pilot");
#if defined(ESP32)
  WiFi.mode(WIFI_AP_STA);
#else
  WiFi.mode(WIFI_STA);
#endif
#if defined(ESP32)
  if (esp_wifi_get_mac(WIFI_IF_STA, ownMac) != ESP_OK) {
    Serial.println("Failed to read gateway STA MAC"); return;
  }
#else
  WiFi.macAddress(ownMac);
#endif
  terrasSmart::formatMqttClientId(ownMac, clientId, sizeof(clientId));
  terrasSmart::formatMqttGatewayId(ownMac, gatewayId, sizeof(gatewayId));
  memcpy(pilotGatewayId, gatewayId, sizeof(gatewayId));
#if defined(ESP8266)
  memcpy(wifiMac, ownMac, sizeof(ownMac));
#endif
#if !defined(PILOT_USB_UART_TEST)
#if defined(ESP32)
  pilotUart.begin(9600, SERIAL_8N1, 16, 17);
#else
  pilotUart.begin(9600);
#endif
#endif
  mqtt.setBufferSize(512); mqtt.setSocketTimeout(1);
  buildApIdentity(); loadConfiguration(); applyConfiguration();
  networkInitialized = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(fdrs_runtime_ap_ssid, fdrs_runtime_ap_password);
  if (runtimeConfigured && wifiSsid.length()) WiFi.begin(ssid, password);
  startWebServer();
  Serial.printf("Setup AP: %s IP: %s\n", fdrs_runtime_ap_ssid, WiFi.softAPIP().toString().c_str());
  Serial.print("Gateway ID: "); Serial.println(gatewayId);
}
void loop() {
  readUart();
#if defined(ESP32)
  handlePhysicalReset();
#endif
  maintainCaptivePortal(); server.handleClient(); maintainWifi(); maintainMdns();
  uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED && !mqtt.connected() && runtimeConfigured && mqttHost.length() &&
      static_cast<int32_t>(now - nextMqttAttempt) >= 0) {
    nextMqttAttempt = now + 5000;
    if (mqttUser.length()) mqtt.connect(clientId, mqttUser.c_str(), mqttPassword.c_str());
    else mqtt.connect(clientId);
  }
  mqtt.loop(); publishPending();
  if (now - lastReport >= 5000) {
    lastReport = now;
    Serial.printf("WiFi=%d MQTT=%d queued=%u published=%lu uart_bytes=%lu uart_markers=%lu invalid=%lu framing_rejected=%lu duplicate=%lu dropped=%lu\n",
        WiFi.status() == WL_CONNECTED, mqtt.connected(), (head + QUEUE_SIZE - tail) % QUEUE_SIZE,
        static_cast<unsigned long>(published), static_cast<unsigned long>(uartBytes),
        static_cast<unsigned long>(uartMarkers), static_cast<unsigned long>(invalid),
        static_cast<unsigned long>(framingRejected), static_cast<unsigned long>(duplicate), static_cast<unsigned long>(dropped));
  }
  delay(1);
}
