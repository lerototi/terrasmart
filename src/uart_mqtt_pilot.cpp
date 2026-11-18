#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <Schedule.h>
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
#include "terrasmart_pilot_ota.h"
#include "terrasmart_pilot_uart_lines.h"

// Credentials are local build inputs; pilot configuration remains isolated.
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
#define TERRASMART_OTA_MQTT
#define TERRASMART_PILOT_OTA
#ifndef WIFI_SSID
#define WIFI_SSID PILOT_WIFI_SSID
#define WIFI_PASS PILOT_WIFI_PASSWORD
#define MQTT_ADDR PILOT_MQTT_HOST
#define MQTT_PORT PILOT_MQTT_PORT
#define MQTT_USER PILOT_MQTT_USER
#define MQTT_PASS PILOT_MQTT_PASSWORD
#endif
#if defined(ESP8266)
void servicePilotUart();
// PubSubClient's CONNACK wait polls available() without yielding. The regular
// scheduler handles yielding TCP/DNS waits; this hook covers that busy wait.
class PilotWiFiClient : public WiFiClient {
 public:
  int available() override { servicePilotUart(); return WiFiClient::available(); }
};
PilotWiFiClient wifiClient;
#else
WiFiClient wifiClient;
#endif
PubSubClient client(wifiClient);
const char *ssid = nullptr, *password = nullptr, *mqtt_server = nullptr;
const char *mqtt_user = nullptr, *mqtt_pass = nullptr;
int mqtt_port = 1883;
char pilotGatewayId[16] = {};
const char *terrasSmartMqttGatewayId() { return pilotGatewayId; }
void terrasSmartPilotOtaPauseTelemetry(bool paused);
void terrasSmartOtaServiceStatusLed() {}
#include "terrasmart_ota.h"
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
uint32_t uartBadTelemetry = 0, uartUnknown = 0, uartOverflow = 0;
uint32_t otaDiscarded = 0;
bool telemetryPaused = false;
bool otaSubscribed = false;
char pendingOtaCommand[512];
size_t pendingOtaLength = 0;
uint32_t lastReport = 0, nextMqttAttempt = 0;
bool uartServiceReady = false;
#if defined(ESP32)
portMUX_TYPE queueLock = portMUX_INITIALIZER_UNLOCKED;
void lockQueue() { portENTER_CRITICAL(&queueLock); }
void unlockQueue() { portEXIT_CRITICAL(&queueLock); }
#else
// The ESP8266 scheduler runs on the same cooperative stack. Queue operations
// below do not yield, so the recurrent reader cannot interrupt them.
void lockQueue() {}
void unlockQueue() {}
bool uartReading = false;
#endif
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
terrasSmartPilotUart::Parser parser;
TerraSmartOta::Packet pilotAck = {};
bool pilotAckReady = false;
uint32_t expectedProbe = 0;
bool probeReady = false;

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
    lockQueue(); ++invalid; unlockQueue(); return;
  }
  lockQueue();
  if (telemetryPaused) { ++otaDiscarded; unlockQueue(); return; }
  if (isDuplicate(frame)) { ++duplicate; unlockQueue(); return; }
  uint8_t next = (head + 1) % QUEUE_SIZE;
  if (next == tail) { ++dropped; unlockQueue(); return; }
  queue[head].frame = frame; head = next;
  unlockQueue();
}
void readUart() {
#if defined(ESP8266)
  if (uartReading) return;
  uartReading = true;
#endif
  uint8_t payload[terras_smart_uart_v1::PAYLOAD_SIZE];
  // Bound work per invocation so a continuously busy link cannot starve the
  // network or the ESP8266 recurrent scheduler.
  unsigned count = 0;
  while (count++ < 256 && pilotUart.available()) {
    const char byte = static_cast<char>(pilotUart.read());
    lockQueue();
    ++uartBytes;
    if (byte == '!') ++uartMarkers;
    unlockQueue();
    auto result = parser.feed(byte, millis());
    if (result == terrasSmartPilotUart::Kind::TELEMETRY) {
      if (terras_smart_uart_v1::decode(parser.line(), parser.size(), payload)) receivePayload(payload);
      else { lockQueue(); ++framingRejected; ++uartBadTelemetry; unlockQueue(); }
    } else if (result == terrasSmartPilotUart::Kind::OTA) {
      TerraSmartOta::Packet packet;
      if (TerraSmartOta::fromUartLine(String(parser.line()), packet) && packet.type == TerraSmartOta::Type::Ack) {
        lockQueue(); pilotAck = packet; pilotAckReady = true; unlockQueue();
      }
    } else if (result == terrasSmartPilotUart::Kind::READY) {
      unsigned long nonce; unsigned mac[6]; char end;
      if (sscanf(parser.line(), "!TSREADY:%8lx:%2x%2x%2x%2x%2x%2x%c", &nonce,
                 &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5], &end) == 8 && end == '\n') {
        bool matches = true;
        for (unsigned i = 0; i < 6; ++i) matches = matches && mac[i] == expectedRadioMac[i];
        lockQueue(); if (matches && nonce == expectedProbe) probeReady = true; unlockQueue();
      }
    } else if (result == terrasSmartPilotUart::Kind::INVALID) {
      lockQueue(); ++framingRejected;
      if (parser.error() == terrasSmartPilotUart::Error::OVERFLOW) ++uartOverflow;
      else ++uartUnknown;
      unlockQueue();
    }
  }
  parser.expire(millis());
#if defined(ESP8266)
  uartReading = false;
#endif
}
#if defined(ESP32)
void uartTask(void *) {
  for (;;) { readUart(); vTaskDelay(1); }
}
#endif
void publishPending() {
  if (!mqtt.connected()) return;
  lockQueue();
  if (head == tail) { unlockQueue(); return; }
  const auto f = queue[tail].frame;
  unlockQueue();
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
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(json)) {
    lockQueue(); ++invalid; tail = (tail + 1) % QUEUE_SIZE; unlockQueue(); return;
  }
  if (!mqtt.publish("terrasmart/data/v1", json, false)) return;
  lockQueue();
  Key &k = cache[nextKey]; k.used = true; memcpy(k.source, f.source, 6);
  k.boot = f.bootId; k.seq = f.sequence; nextKey = (nextKey + 1) % 16;
  tail = (tail + 1) % QUEUE_SIZE; ++published;
  unlockQueue();
  Serial.print("MQTT published: "); Serial.println(json);
}
} // namespace

void terrasSmartPilotOtaPauseTelemetry(bool paused) {
  lockQueue(); telemetryPaused = paused; unlockQueue();
  Serial.println(paused ? "Pilot OTA: telemetry paused" : "Pilot OTA: telemetry resumed");
}

void terrasSmartPilotOtaWriteUart(const String &line) {
  pilotUart.println(line);
#if defined(ESP32) || defined(PILOT_USB_UART_TEST)
  pilotUart.flush();
#endif
}
void terrasSmartPilotOtaServiceUart() {
#if defined(ESP8266)
  readUart();
#endif
}
void terrasSmartPilotOtaClearAck() { lockQueue(); pilotAckReady = false; unlockQueue(); }
bool terrasSmartPilotOtaTakeAck(uint8_t target, uint16_t sequence, uint8_t &status) {
  lockQueue();
  bool match = pilotAckReady && pilotAck.target == target && pilotAck.sequence == sequence && pilotAck.size == 1;
  if (match) { status = pilotAck.payload[0]; pilotAckReady = false; }
  unlockQueue(); return match;
}
bool terrasSmartPilotOtaVerifyRadio() {
#if defined(ESP32)
  const uint32_t nonce = esp_random();
#else
  const uint32_t nonce = ESP.random();
#endif
  lockQueue(); expectedProbe = nonce; probeReady = false; unlockQueue();
  char line[32]; snprintf(line, sizeof(line), "!TSPROBE:%08lX\n", static_cast<unsigned long>(nonce));
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    pilotUart.print(line);
#if defined(ESP32) || defined(PILOT_USB_UART_TEST)
    pilotUart.flush();
#endif
    const uint32_t start = millis();
    while (millis() - start < 2000) {
      terrasSmartPilotOtaServiceUart();
      lockQueue(); bool ready = probeReady; unlockQueue();
      if (ready) { Serial.println("Pilot radio identity verified"); return true; }
      client.loop(); delay(1);
    }
  }
  Serial.println("Pilot radio identity check failed"); return false;
}

void pilotMqttCallback(char *topic, uint8_t *payload, unsigned int length) {
  if (!terrasSmartPilotOta::acceptsTopic(topic)) return;
  if (terrasSmartOtaUpdateInProgress() || pendingOtaLength ||
      !length || length > sizeof(pendingOtaCommand)) {
    Serial.println("Pilot OTA command ignored: busy or invalid length");
    return;
  }
  memcpy(pendingOtaCommand, payload, length);
  pendingOtaLength = length;
}

#if defined(ESP8266)
void servicePilotUart() { if (uartServiceReady) readUart(); }
#endif

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
  // A full OTA line is 387 bytes. Listeners that also receive their own TX
  // echo must hold the line plus the ACK without dropping the ACK trailer.
  pilotUart.setRxBufferSize(1024);
  pilotUart.begin(TERRASMART_PILOT_UART_BAUD, SERIAL_8N1, 16, 17);
#else
  pilotUart.begin(TERRASMART_PILOT_UART_BAUD, SWSERIAL_8N1, D6, D7, false, 512);
#endif
#endif
  Serial.printf("Pilot UART baud: %lu\n", static_cast<unsigned long>(TERRASMART_PILOT_UART_BAUD));
#if defined(ESP32)
  uartServiceReady = xTaskCreate(uartTask, "pilot_uart", 4096, nullptr, 2, nullptr) == pdPASS;
#else
  uartServiceReady = schedule_recurrent_function_us([]() { readUart(); return true; }, 1000);
#endif
  if (!uartServiceReady) Serial.println("UART receiver service failed; MQTT reconnect disabled");
  mqtt.setBufferSize(512); mqtt.setSocketTimeout(1);
  mqtt.setCallback(pilotMqttCallback);
  buildApIdentity(); loadConfiguration(); applyConfiguration();
  networkInitialized = true;
  WiFi.mode(WIFI_AP_STA);
#if defined(ESP32)
  WiFi.setSleep(false);
#endif
  WiFi.softAP(fdrs_runtime_ap_ssid, fdrs_runtime_ap_password);
  if (runtimeConfigured && wifiSsid.length()) WiFi.begin(ssid, password);
  startWebServer();
  Serial.printf("Setup AP: %s IP: %s\n", fdrs_runtime_ap_ssid, WiFi.softAPIP().toString().c_str());
  Serial.print("Gateway ID: "); Serial.println(gatewayId);
}
void loop() {
#if defined(ESP8266)
  readUart();
#endif
#if defined(ESP32)
  handlePhysicalReset();
#endif
  maintainCaptivePortal(); server.handleClient(); maintainWifi(); maintainMdns();
  uint32_t now = millis();
  if (uartServiceReady && WiFi.status() == WL_CONNECTED && !mqtt.connected() && runtimeConfigured && mqttHost.length() &&
      static_cast<int32_t>(now - nextMqttAttempt) >= 0) {
    nextMqttAttempt = now + 5000;
    if (mqttUser.length()) mqtt.connect(clientId, mqttUser.c_str(), mqttPassword.c_str());
    else mqtt.connect(clientId);
  }
  if (!mqtt.connected()) otaSubscribed = false;
  if (mqtt.connected() && !otaSubscribed) {
    otaSubscribed = mqtt.subscribe(terrasSmartPilotOta::TOPIC);
    if (otaSubscribed) Serial.println("Pilot OTA topic subscribed");
  }
  mqtt.loop();
  if (pendingOtaLength) {
    const size_t length = pendingOtaLength;
    pendingOtaLength = 0;
    terrasSmartOtaHandleMqttCommand(reinterpret_cast<const uint8_t *>(pendingOtaCommand), length);
  }
  publishPending();
  if (now - lastReport >= 5000) {
    lastReport = now;
    lockQueue();
    const uint8_t queued = (head + QUEUE_SIZE - tail) % QUEUE_SIZE;
    const uint32_t localPublished = published, localBytes = uartBytes, localMarkers = uartMarkers;
    const uint32_t localInvalid = invalid, localRejected = framingRejected, localDuplicate = duplicate, localDropped = dropped;
    const uint32_t localOtaDiscarded = otaDiscarded;
    const uint32_t localBadTelemetry = uartBadTelemetry, localUnknown = uartUnknown, localOverflow = uartOverflow;
    unlockQueue();
    Serial.printf("WiFi=%d MQTT=%d queued=%u published=%lu uart_bytes=%lu uart_markers=%lu invalid=%lu framing_rejected=%lu duplicate=%lu dropped=%lu ota_discarded=%lu uart_bad_data=%lu uart_unknown=%lu uart_overflow=%lu\n",
        WiFi.status() == WL_CONNECTED, mqtt.connected(), queued,
        static_cast<unsigned long>(localPublished), static_cast<unsigned long>(localBytes),
        static_cast<unsigned long>(localMarkers), static_cast<unsigned long>(localInvalid),
        static_cast<unsigned long>(localRejected), static_cast<unsigned long>(localDuplicate), static_cast<unsigned long>(localDropped),
        static_cast<unsigned long>(localOtaDiscarded), static_cast<unsigned long>(localBadTelemetry),
        static_cast<unsigned long>(localUnknown), static_cast<unsigned long>(localOverflow));
  }
  delay(1);
}
