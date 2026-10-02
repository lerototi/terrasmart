#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <espnow.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#endif

#ifndef RSSI_PROBE_CHANNEL
#define RSSI_PROBE_CHANNEL 1
#endif

#ifndef RSSI_PROBE_GATEWAY_ID
#define RSSI_PROBE_GATEWAY_ID 1
#endif

namespace {
constexpr uint32_t PROBE_MAGIC = 0x54535250; // "TSRP"
constexpr uint8_t PROBE_VERSION = 1;
constexpr uint32_t BEACON_INTERVAL_MS = 1000;
constexpr uint32_t REPORT_INTERVAL_MS = 5000;
constexpr uint32_t MAX_INITIAL_BEACON_DELAY_MS = 1000;

struct __attribute__((packed)) ProbePacket {
  uint32_t magic;
  uint32_t sequence;
  uint8_t version;
  uint8_t gatewayId;
  uint8_t channel;
};

uint32_t lastReportAt = 0;
uint32_t receivedPackets = 0;
uint32_t lastReceivedSequence = 0;
uint8_t lastGatewayId = 0;
uint8_t lastSourceMac[6] = {};
bool radioReady = false;

void printMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; ++i) {
    if (i > 0) {
      Serial.print(':');
    }
    if (mac[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(mac[i], HEX);
  }
}

void configureRadio() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

#if defined(ESP8266)
  wifi_set_channel(RSSI_PROBE_CHANNEL);
#elif defined(ESP32)
  const esp_err_t channelResult =
      esp_wifi_set_channel(RSSI_PROBE_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (channelResult != ESP_OK) {
    Serial.print("Could not set radio channel, error: ");
    Serial.println(static_cast<int>(channelResult));
  }
#endif
}

#if defined(ESPNOW_RSSI_BEACON)
constexpr uint8_t BEACON_GATEWAY_ID = RSSI_PROBE_GATEWAY_ID;
const uint8_t BROADCAST_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint32_t nextBeaconAt = 0;
uint32_t beaconSequence = 0;
uint32_t sentPackets = 0;
uint32_t sendFailures = 0;

#if defined(ESP32)
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
#else
void onDataSent(uint8_t *mac, uint8_t status) {
#endif
  (void)mac;
  if (status == 0) {
    ++sentPackets;
  } else {
    ++sendFailures;
  }
}

bool initializeEspNow() {
#if defined(ESP8266)
  if (esp_now_init() != 0) {
    return false;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  return esp_now_register_send_cb(onDataSent) == 0 &&
         esp_now_add_peer(const_cast<uint8_t *>(BROADCAST_MAC),
                          ESP_NOW_ROLE_COMBO, RSSI_PROBE_CHANNEL, nullptr, 0) == 0;
#elif defined(ESP32)
  if (esp_now_init() != ESP_OK) {
    return false;
  }
  if (esp_now_register_send_cb(onDataSent) != ESP_OK) {
    return false;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, sizeof(BROADCAST_MAC));
  peerInfo.channel = RSSI_PROBE_CHANNEL;
  peerInfo.encrypt = false;
  return esp_now_add_peer(&peerInfo) == ESP_OK;
#endif
}

void sendBeacon() {
  ProbePacket packet = {
      PROBE_MAGIC,
      ++beaconSequence,
      PROBE_VERSION,
      BEACON_GATEWAY_ID,
      RSSI_PROBE_CHANNEL,
  };

#if defined(ESP8266)
  const int result = esp_now_send(const_cast<uint8_t *>(BROADCAST_MAC),
                                 reinterpret_cast<uint8_t *>(&packet),
                                 sizeof(packet));
  if (result != 0) {
    ++sendFailures;
  }
#elif defined(ESP32)
  const esp_err_t result = esp_now_send(BROADCAST_MAC,
                                        reinterpret_cast<const uint8_t *>(&packet),
                                        sizeof(packet));
  if (result != ESP_OK) {
    ++sendFailures;
  }
#endif
  nextBeaconAt = millis() + BEACON_INTERVAL_MS + random(0, 100);
}

#else
#if defined(ESP8266)
void onDataReceived(uint8_t *mac, uint8_t *data, uint8_t length);
#elif defined(ESP32)
void onDataReceived(const uint8_t *mac, const uint8_t *data, int length);
#endif

bool initializeEspNow() {
#if defined(ESP8266)
  if (esp_now_init() != 0) {
    return false;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  return esp_now_register_recv_cb(onDataReceived) == 0;
#elif defined(ESP32)
  if (esp_now_init() != ESP_OK) {
    return false;
  }
  return esp_now_register_recv_cb(onDataReceived) == ESP_OK;
#endif
}

#if defined(ESP8266)
void onDataReceived(uint8_t *mac, uint8_t *data, uint8_t length) {
#elif defined(ESP32)
void onDataReceived(const uint8_t *mac, const uint8_t *data, int length) {
#endif
  if (length != sizeof(ProbePacket)) {
    return;
  }

  ProbePacket packet;
  memcpy(&packet, data, sizeof(packet));
  if (packet.magic != PROBE_MAGIC || packet.version != PROBE_VERSION ||
      packet.channel != RSSI_PROBE_CHANNEL) {
    return;
  }

  memcpy(lastSourceMac, mac, sizeof(lastSourceMac));
  lastReceivedSequence = packet.sequence;
  lastGatewayId = packet.gatewayId;
  ++receivedPackets;
}
#endif
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("TerraSmart ESP-NOW RSSI/API probe");

  configureRadio();
  Serial.print("Configured channel: ");
  Serial.println(RSSI_PROBE_CHANNEL);

  radioReady = initializeEspNow();
  if (!radioReady) {
    Serial.println("ESP-NOW initialization failed; check channel and radio setup.");
    return;
  }

#if defined(ESPNOW_RSSI_BEACON)
  randomSeed(micros());
  nextBeaconAt = millis() + random(100, MAX_INITIAL_BEACON_DELAY_MS);
  Serial.print("Role: beacon, gateway ID: ");
  Serial.println(BEACON_GATEWAY_ID);
  Serial.print("Radio MAC: ");
  Serial.println(WiFi.macAddress());
#else
  randomSeed(micros());
  Serial.println("Role: receiver");
  Serial.print("Radio MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("RSSI: unavailable in this core's ESP-NOW receive callback signature");
#endif
}

void loop() {
  if (!radioReady) {
    delay(1000);
    return;
  }

  const uint32_t now = millis();

#if defined(ESPNOW_RSSI_BEACON)
  if (static_cast<int32_t>(now - nextBeaconAt) >= 0) {
    sendBeacon();
  }

  if (now - lastReportAt >= REPORT_INTERVAL_MS) {
    lastReportAt = now;
    Serial.print("Beacon sequence: ");
    Serial.print(beaconSequence);
    Serial.print(" | send callback successes: ");
    Serial.print(sentPackets);
    Serial.print(" | send failures: ");
    Serial.println(sendFailures);
  }
#else
  if (now - lastReportAt >= REPORT_INTERVAL_MS) {
    lastReportAt = now;

    Serial.print("Packets received: ");
    Serial.print(receivedPackets);
    Serial.print(" | last gateway ID: ");
    Serial.print(lastGatewayId);
    Serial.print(" | last sequence: ");
    Serial.print(lastReceivedSequence);
    Serial.print(" | source MAC: ");
    if (receivedPackets > 0) {
      printMac(lastSourceMac);
    } else {
      Serial.print("none");
    }
    Serial.println(" | RSSI: unavailable");
  }
#endif

  delay(1);
}
