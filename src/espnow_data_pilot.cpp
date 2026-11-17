#include <Arduino.h>
#include <string.h>
#include "terrasmart_data_v1.h"
#include "terrasmart_discovery_packet.h"
#if defined(TERRASMART_PILOT_UART_BRIDGE)
#include "terrasmart_uart_v1.h"
#endif
#include <fdrs_datatypes.h>

#if defined(ESPNOW_DATA_PILOT_NODE) && defined(ESP8266)
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <user_interface.h>
#elif (defined(ESPNOW_DATA_PILOT_GATEWAY) || defined(ESPNOW_DATA_PILOT_NODE)) && defined(ESP32)
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#else
#error "Select the ESP8266 node or ESP32 gateway pilot"
#endif

#ifndef PILOT_GATEWAY_MAC_0
#error "The pilot node requires a gateway factory STA MAC"
#endif

namespace {
using namespace terras_smart_data_v1;
constexpr uint8_t CHANNEL = 1;
constexpr uint32_t NETWORK_ID = TERRASMART_DISCOVERY_NETWORK_ID;
constexpr uint8_t gatewayMac[6] = {PILOT_GATEWAY_MAC_0, PILOT_GATEWAY_MAC_1,
    PILOT_GATEWAY_MAC_2, PILOT_GATEWAY_MAC_3, PILOT_GATEWAY_MAC_4, PILOT_GATEWAY_MAC_5};
uint8_t ownMac[6] = {};
bool ready = false;
uint32_t lastReport = 0;

void printMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; ++i) {
    if (i) Serial.print(':');
    if (mac[i] < 16) Serial.print('0');
    Serial.print(mac[i], HEX);
  }
}

#if defined(ESPNOW_DATA_PILOT_NODE)
uint32_t bootId = 0, sequence = 0, nextSend = 0;
volatile uint32_t sendOk = 0, sendFail = 0;
uint32_t queueErrors = 0;
#if defined(ESP8266)
void onSent(uint8_t *, uint8_t status) {
  if (status == 0) ++sendOk; else ++sendFail;
}
#else
void onSent(const uint8_t *, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) ++sendOk; else ++sendFail;
}
#endif
void sendReading() {
  // Synthetic FDRS reading keeps sensor wiring independent from the radio test.
  DataReading reading = {};
  reading.id = 42; reading.t = LEVEL_T; reading.d = 23.5f;
  Frame frame = {};
  frame.networkId = NETWORK_ID;
  memcpy(frame.source, ownMac, 6); memcpy(frame.destination, gatewayMac, 6);
  frame.sequence = ++sequence; frame.bootId = bootId; frame.hopLimit = 1;
  frame.reading = {reading.id, reading.t, reading.d};
  uint8_t wire[FRAME_SIZE];
  if (!encode(frame, wire, sizeof(wire)) ||
      esp_now_send(const_cast<uint8_t *>(gatewayMac), wire, sizeof(wire)) != 0)
    ++queueErrors;
  nextSend = millis() + 2000;
}
#else
constexpr uint8_t QUEUE_SIZE = 8;
struct Event { uint8_t source[6]; uint8_t wire[FRAME_SIZE]; };
Event queue[QUEUE_SIZE];
uint8_t head = 0, tail = 0;
uint32_t drops = 0, invalidLength = 0, accepted = 0, invalid = 0;
#if defined(TERRASMART_PILOT_UART_BRIDGE)
HardwareSerial pilotUart(1);
uint32_t uartForwarded = 0, uartDrops = 0;
#endif
portMUX_TYPE queueLock = portMUX_INITIALIZER_UNLOCKED;
void onReceived(const uint8_t *mac, const uint8_t *data, int length) {
  portENTER_CRITICAL(&queueLock);
  if (!mac || !data || length != static_cast<int>(FRAME_SIZE)) {
    ++invalidLength;
  } else {
    uint8_t next = (head + 1) % QUEUE_SIZE;
    if (next == tail) ++drops;
    else {
      memcpy(queue[head].source, mac, 6);
      memcpy(queue[head].wire, data, FRAME_SIZE);
      head = next;
    }
  }
  portEXIT_CRITICAL(&queueLock);
}
void processEvents() {
  while (true) {
    Event event;
    portENTER_CRITICAL(&queueLock);
    if (tail == head) { portEXIT_CRITICAL(&queueLock); break; }
    event = queue[tail]; tail = (tail + 1) % QUEUE_SIZE;
    portEXIT_CRITICAL(&queueLock);
    Frame frame = {};
    if (!decode(event.wire, sizeof(event.wire), NETWORK_ID, ownMac, frame) ||
        memcmp(event.source, frame.source, 6) != 0) { ++invalid; continue; }
    // Explicit conversion keeps FDRS's in-memory field order off the wire.
    DataReading reading = {};
    reading.id = frame.reading.id; reading.t = frame.reading.type; reading.d = frame.reading.data;
    ++accepted;
#if defined(TERRASMART_PILOT_UART_BRIDGE)
    uint8_t payload[terras_smart_uart_v1::PAYLOAD_SIZE];
    memcpy(payload, ownMac, 6);
    memcpy(payload + 6, event.wire, FRAME_SIZE);
    char line[terras_smart_uart_v1::LINE_SIZE + 1];
    if (!terras_smart_uart_v1::encode(payload, line, sizeof(line)) ||
        pilotUart.availableForWrite() < static_cast<int>(terras_smart_uart_v1::LINE_SIZE) ||
        pilotUart.write(reinterpret_cast<const uint8_t *>(line), terras_smart_uart_v1::LINE_SIZE) != terras_smart_uart_v1::LINE_SIZE)
      ++uartDrops;
    else ++uartForwarded;
#if defined(PILOT_USB_UART_TEST)
    Serial.print("PILOT_FRAME:"); Serial.print(line);
#endif
#endif
    Serial.print("DATA node="); printMac(frame.source);
    Serial.print(" gateway="); printMac(frame.destination);
    Serial.print(" boot="); Serial.print(frame.bootId);
    Serial.print(" seq="); Serial.print(frame.sequence);
    Serial.print(" id="); Serial.print(reading.id);
    Serial.print(" type="); Serial.print(reading.t);
    Serial.print(" data="); Serial.println(reading.d, 2);
  }
}
#endif
} // namespace

void setup() {
#if defined(ESPNOW_DATA_PILOT_NODE) && defined(ESP8266)
  Serial.begin(74880);
#else
  Serial.begin(115200);
#endif
  delay(100);
  Serial.println("\nTerraSmart experimental data v1 pilot");
  WiFi.mode(WIFI_STA); WiFi.disconnect();
#if defined(ESPNOW_DATA_PILOT_NODE) && defined(ESP8266)
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  if (!wifi_get_macaddr(STATION_IF, ownMac) || !wifi_set_channel(CHANNEL) ||
      wifi_get_channel() != CHANNEL || esp_now_init() != 0 ||
      esp_now_set_self_role(ESP_NOW_ROLE_COMBO) != 0 ||
      esp_now_register_send_cb(onSent) != 0 ||
      esp_now_add_peer(const_cast<uint8_t *>(gatewayMac), ESP_NOW_ROLE_COMBO,
                       CHANNEL, nullptr, 0) != 0) {
    Serial.println("Radio initialization failed"); return;
  }
  bootId = ESP.random();
  Serial.print("Boot ID: "); Serial.println(bootId);
#elif defined(ESPNOW_DATA_PILOT_NODE)
  if (esp_wifi_get_mac(WIFI_IF_STA, ownMac) != ESP_OK ||
      esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK ||
      esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
      esp_now_init() != ESP_OK || esp_now_register_send_cb(onSent) != ESP_OK) {
    Serial.println("Radio initialization failed"); return;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, gatewayMac, 6); peer.channel = CHANNEL; peer.ifidx = WIFI_IF_STA;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("Peer initialization failed"); return; }
  bootId = esp_random();
  Serial.print("Boot ID: "); Serial.println(bootId);
#else
  #if defined(TERRASMART_PILOT_UART_BRIDGE)
  pilotUart.begin(9600, SERIAL_8N1, 16, 17);
  #endif
  esp_err_t result = esp_wifi_get_mac(WIFI_IF_STA, ownMac);
  if (result != ESP_OK) {
    Serial.printf("STA MAC read failed: %d\n", static_cast<int>(result)); return;
  }
  Serial.print("Actual STA MAC: "); printMac(ownMac); Serial.println();
  Serial.print("Configured gateway MAC: "); printMac(gatewayMac); Serial.println();
  if (memcmp(ownMac, gatewayMac, 6) != 0) {
    Serial.println("Gateway MAC mismatch; check pilot configuration"); return;
  }
  result = esp_wifi_set_ps(WIFI_PS_NONE);
  if (result != ESP_OK) {
    Serial.printf("Disable power save failed: %d\n", static_cast<int>(result)); return;
  }
  result = esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (result != ESP_OK) {
    Serial.printf("Set channel failed: %d\n", static_cast<int>(result)); return;
  }
  result = esp_now_init();
  if (result != ESP_OK) {
    Serial.printf("ESP-NOW init failed: %d\n", static_cast<int>(result)); return;
  }
  result = esp_now_register_recv_cb(onReceived);
  if (result != ESP_OK) {
    Serial.printf("ESP-NOW receive callback registration failed: %d\n", static_cast<int>(result)); return;
  }
#endif
  Serial.print("STA MAC: "); printMac(ownMac); Serial.println();
  Serial.print("Gateway factory STA MAC: "); printMac(gatewayMac); Serial.println();
  Serial.print("Network ID: 0x"); Serial.println(NETWORK_ID, HEX);
  Serial.println("Pilot ready: synthetic data, no authentication, no MQTT");
  ready = true;
}

void loop() {
  if (!ready) { delay(1000); return; }
  const uint32_t now = millis();
#if defined(ESPNOW_DATA_PILOT_NODE)
  if (static_cast<int32_t>(now - nextSend) >= 0) sendReading();
  if (now - lastReport >= 5000) {
    lastReport = now;
    Serial.print("Sent seq="); Serial.print(sequence);
    Serial.print(" link_ok="); Serial.print(sendOk);
    Serial.print(" link_fail="); Serial.print(sendFail);
    Serial.print(" queue_errors="); Serial.println(queueErrors);
  }
#else
  processEvents();
  if (now - lastReport >= 5000) {
    lastReport = now;
    portENTER_CRITICAL(&queueLock);
    const uint32_t localDrops = drops, localInvalidLength = invalidLength;
    portEXIT_CRITICAL(&queueLock);
    Serial.print("Accepted="); Serial.print(accepted);
    Serial.print(" rejected="); Serial.print(invalid + localInvalidLength);
    Serial.print(" queue_drops="); Serial.print(localDrops);
#if defined(TERRASMART_PILOT_UART_BRIDGE)
    Serial.print(" uart_forwarded="); Serial.print(uartForwarded);
    Serial.print(" uart_drops="); Serial.print(uartDrops);
#endif
    Serial.println();
  }
#endif
  delay(1);
}
