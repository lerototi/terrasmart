#include <Arduino.h>
#include <string.h>
#include "terrasmart_discovery_packet.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <user_interface.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#endif

#ifndef RSSI_PROBE_CHANNEL
#define RSSI_PROBE_CHANNEL 1
#endif

#ifndef DISCOVERY_GATEWAY_MAC_0
#define DISCOVERY_GATEWAY_MAC_0 0xAA
#endif
#ifndef DISCOVERY_GATEWAY_MAC_1
#define DISCOVERY_GATEWAY_MAC_1 0xBB
#endif
#ifndef DISCOVERY_GATEWAY_MAC_2
#define DISCOVERY_GATEWAY_MAC_2 0xCC
#endif
#ifndef DISCOVERY_GATEWAY_MAC_3
#define DISCOVERY_GATEWAY_MAC_3 0xDD
#endif
#ifndef DISCOVERY_GATEWAY_MAC_4
#define DISCOVERY_GATEWAY_MAC_4 0xEE
#endif
#ifndef DISCOVERY_GATEWAY_MAC_5
#define DISCOVERY_GATEWAY_MAC_5 0x01
#endif

#ifndef DISCOVERY_CANDIDATE_1_MAC_0
#define DISCOVERY_CANDIDATE_1_MAC_0 0xAA
#define DISCOVERY_CANDIDATE_1_MAC_1 0xBB
#define DISCOVERY_CANDIDATE_1_MAC_2 0xCC
#define DISCOVERY_CANDIDATE_1_MAC_3 0xDD
#define DISCOVERY_CANDIDATE_1_MAC_4 0xEE
#define DISCOVERY_CANDIDATE_1_MAC_5 0x01
#endif
#ifndef DISCOVERY_CANDIDATE_2_MAC_0
#define DISCOVERY_CANDIDATE_2_MAC_0 0xAA
#define DISCOVERY_CANDIDATE_2_MAC_1 0xBB
#define DISCOVERY_CANDIDATE_2_MAC_2 0xCC
#define DISCOVERY_CANDIDATE_2_MAC_3 0xDD
#define DISCOVERY_CANDIDATE_2_MAC_4 0xEE
#define DISCOVERY_CANDIDATE_2_MAC_5 0x02
#endif

#if defined(ESPNOW_DISCOVERY_GATEWAY_PROBE) == \
    defined(ESPNOW_DISCOVERY_NODE_PROBE)
#error "Select exactly one ESP-NOW discovery probe role"
#endif

namespace {
using namespace terras_smart_discovery;
constexpr uint8_t PACKET_BEACON = BEACON;
constexpr uint8_t PACKET_PROBE = PROBE;
constexpr uint8_t PACKET_REPLY = REPLY;
constexpr uint32_t NETWORK_ID = TERRASMART_DISCOVERY_NETWORK_ID;
constexpr uint32_t BEACON_INTERVAL_MS = 1000;
constexpr uint32_t PROBE_INTERVAL_MS = 2000;
constexpr uint32_t PROBE_TIMEOUT_MS = 1000;
constexpr uint32_t REPORT_INTERVAL_MS = 5000;
constexpr uint8_t BROADCAST_MAC[6] = {0xff, 0xff, 0xff,
                                      0xff, 0xff, 0xff};
constexpr uint8_t GATEWAY_ACTIVE_MAC[6] = {
    DISCOVERY_GATEWAY_MAC_0, DISCOVERY_GATEWAY_MAC_1,
    DISCOVERY_GATEWAY_MAC_2, DISCOVERY_GATEWAY_MAC_3,
    DISCOVERY_GATEWAY_MAC_4, DISCOVERY_GATEWAY_MAC_5};
constexpr uint8_t CANDIDATE_MACS[2][6] = {
    {DISCOVERY_CANDIDATE_1_MAC_0, DISCOVERY_CANDIDATE_1_MAC_1,
     DISCOVERY_CANDIDATE_1_MAC_2, DISCOVERY_CANDIDATE_1_MAC_3,
     DISCOVERY_CANDIDATE_1_MAC_4, DISCOVERY_CANDIDATE_1_MAC_5},
    {DISCOVERY_CANDIDATE_2_MAC_0, DISCOVERY_CANDIDATE_2_MAC_1,
     DISCOVERY_CANDIDATE_2_MAC_2, DISCOVERY_CANDIDATE_2_MAC_3,
     DISCOVERY_CANDIDATE_2_MAC_4, DISCOVERY_CANDIDATE_2_MAC_5}};
#if defined(ESP8266)
constexpr uint32_t SERIAL_BAUD = 74880;
#else
constexpr uint32_t SERIAL_BAUD = 115200;
#endif

using DiscoveryPacket = Packet;

struct PendingReply {
  uint8_t mac[6];
  uint32_t sequence;
};

uint8_t factoryStaMac[6] = {};
uint8_t activeStaMac[6] = {};
bool factoryStaMacValid = false;
bool activeStaMacValid = false;
bool radioReady = false;
uint32_t lastReportAt = 0;

#if defined(ESPNOW_DISCOVERY_GATEWAY_PROBE)
uint32_t nextBeaconAt = 0;
uint32_t beaconSequence = 0;
volatile uint32_t receiveCallbacks = 0;
volatile uint32_t validProbes = 0;
volatile uint32_t otherBeacons = 0;
volatile uint32_t invalidPackets = 0;
volatile uint32_t wrongNetworkPackets = 0;
volatile uint32_t peerAddFailures = 0;
volatile uint32_t sendQueueFailures = 0;
volatile uint32_t beaconSendSuccesses = 0;
volatile uint32_t beaconSendFailures = 0;
volatile uint32_t replyCallbacks = 0;
volatile uint32_t replySendFailures = 0;
uint8_t lastNodeMac[6] = {};
bool lastNodeMacValid = false;
PendingReply replyQueue[4] = {};
volatile uint8_t replyHead = 0;
volatile uint8_t replyTail = 0;
portMUX_TYPE replyQueueLock = portMUX_INITIALIZER_UNLOCKED;
#else
struct CandidateStats {
  uint32_t beacons;
  uint32_t probes;
  uint32_t replies;
  uint32_t timeouts;
  uint32_t lastLatencyMs;
};
CandidateStats candidates[2] = {};
uint32_t nextProbeAt = 0;
uint32_t probeSequence = 0;
volatile uint32_t receiveCallbacks = 0;
volatile uint32_t unmatchedReplies = 0;
volatile uint32_t invalidPackets = 0;
volatile uint32_t wrongNetworkPackets = 0;
volatile uint32_t sendCallbacksSuccess = 0;
volatile uint32_t sendCallbacksFailure = 0;
volatile uint32_t probeQueueFailures = 0;
volatile uint32_t probeTimeouts = 0;
volatile uint32_t lastReplySequence = 0;
volatile uint32_t lastReplyLatencyMs = 0;
volatile uint32_t pendingProbeSequence = 0;
volatile uint32_t pendingProbeAt = 0;
volatile uint8_t pendingCandidate = 0;
uint8_t nextCandidate = 0;
volatile bool probePending = false;
#endif

void compilerMemoryBarrier() { __asm__ __volatile__("" ::: "memory"); }

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

void printMacLine(const char *label, const uint8_t *mac, bool valid) {
  Serial.print(label);
  if (!valid) {
    Serial.println("unavailable");
    return;
  }
  printMac(mac);
  Serial.println();
}

bool readStaMac(uint8_t *mac) {
#if defined(ESP8266)
  return wifi_get_macaddr(STATION_IF, mac);
#elif defined(ESP32)
  return esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK;
#else
  (void)mac;
  return false;
#endif
}

void configureRadio() {
#if defined(ESP8266)
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  wifi_set_channel(RSSI_PROBE_CHANNEL);
#elif defined(ESP32)
  const esp_err_t powerSaveResult = esp_wifi_set_ps(WIFI_PS_NONE);
  if (powerSaveResult != ESP_OK) {
    Serial.print("Could not disable Wi-Fi power save, error: ");
    Serial.println(static_cast<int>(powerSaveResult));
  }
  const esp_err_t channelResult =
      esp_wifi_set_channel(RSSI_PROBE_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (channelResult != ESP_OK) {
    Serial.print("Could not set radio channel, error: ");
    Serial.println(static_cast<int>(channelResult));
  }
#endif
}

bool printActiveChannel() {
#if defined(ESP8266)
  const uint8_t channel = wifi_get_channel();
  Serial.print("Active radio channel: ");
  Serial.println(channel);
  return channel == RSSI_PROBE_CHANNEL;
#elif defined(ESP32)
  uint8_t channel = 0;
  wifi_second_chan_t secondaryChannel = WIFI_SECOND_CHAN_NONE;
  const esp_err_t result = esp_wifi_get_channel(&channel, &secondaryChannel);
  Serial.print("Active radio channel: ");
  if (result != ESP_OK) {
    Serial.print("unavailable, error: ");
    Serial.println(static_cast<int>(result));
    return false;
  }
  Serial.println(channel);
  return channel == RSSI_PROBE_CHANNEL;
#endif
}

DiscoveryPacket makePacket(uint8_t type, uint32_t sequence) {
  return {type, RSSI_PROBE_CHANNEL, NETWORK_ID, sequence};
}

#if defined(ESPNOW_DISCOVERY_GATEWAY_PROBE)
bool addBroadcastPeer() {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, sizeof(BROADCAST_MAC));
  peerInfo.channel = RSSI_PROBE_CHANNEL;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;
  return esp_now_add_peer(&peerInfo) == ESP_OK;
}

bool addNodePeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) {
    return true;
  }
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, sizeof(peerInfo.peer_addr));
  peerInfo.channel = RSSI_PROBE_CHANNEL;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;
  return esp_now_add_peer(&peerInfo) == ESP_OK;
}

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  if (mac != nullptr && memcmp(mac, BROADCAST_MAC, sizeof(BROADCAST_MAC)) == 0) {
    if (status == ESP_NOW_SEND_SUCCESS) {
      ++beaconSendSuccesses;
    } else {
      ++beaconSendFailures;
    }
    return;
  }
  if (status == ESP_NOW_SEND_SUCCESS) {
    ++replyCallbacks;
  } else {
    ++replySendFailures;
  }
}

void onDataReceived(const uint8_t *mac, const uint8_t *data, int length) {
  ++receiveCallbacks;
  if (mac == nullptr) {
    ++invalidPackets;
    return;
  }
  DiscoveryPacket probe;
  const DecodeResult result = length < 0 ? DecodeResult::INVALID :
      decode(data, static_cast<size_t>(length), RSSI_PROBE_CHANNEL, NETWORK_ID, probe);
  if (result == DecodeResult::WRONG_NETWORK) {
    ++wrongNetworkPackets;
    return;
  }
  if (result != DecodeResult::OK) {
    ++invalidPackets;
    return;
  }
  if (probe.type == PACKET_BEACON) {
    ++otherBeacons;
    return;
  }
  if (probe.type != PACKET_PROBE) {
    ++invalidPackets;
    return;
  }

  ++validProbes;
  memcpy(lastNodeMac, mac, sizeof(lastNodeMac));
  lastNodeMacValid = true;
  portENTER_CRITICAL(&replyQueueLock);
  const uint8_t head = replyHead;
  const uint8_t nextHead = static_cast<uint8_t>((head + 1) % 4);
  if (nextHead == replyTail) {
    ++sendQueueFailures;
    portEXIT_CRITICAL(&replyQueueLock);
    return;
  }
  memcpy(replyQueue[head].mac, mac, sizeof(replyQueue[head].mac));
  replyQueue[head].sequence = probe.sequence;
  replyHead = nextHead;
  portEXIT_CRITICAL(&replyQueueLock);
}

bool initializeEspNow() {
  if (esp_now_init() != ESP_OK) {
    return false;
  }
  if (esp_now_register_recv_cb(onDataReceived) != ESP_OK ||
      esp_now_register_send_cb(onDataSent) != ESP_OK) {
    return false;
  }
  return addBroadcastPeer();
}

void sendBeacon() {
  const DiscoveryPacket beacon = makePacket(PACKET_BEACON, ++beaconSequence);
  uint8_t wire[WIRE_SIZE];
  encode(beacon, wire);
  if (esp_now_send(BROADCAST_MAC, wire, sizeof(wire)) != ESP_OK) {
    ++sendQueueFailures;
  }
  nextBeaconAt = millis() + BEACON_INTERVAL_MS;
}

void processProbeReplies() {
  while (true) {
    portENTER_CRITICAL(&replyQueueLock);
    if (replyTail == replyHead) {
      portEXIT_CRITICAL(&replyQueueLock);
      break;
    }
    const uint8_t tail = replyTail;
    const PendingReply pending = replyQueue[tail];
    replyTail = static_cast<uint8_t>((tail + 1) % 4);
    portEXIT_CRITICAL(&replyQueueLock);
    if (!addNodePeer(pending.mac)) {
      ++peerAddFailures;
      continue;
    }
    DiscoveryPacket reply = makePacket(PACKET_REPLY, pending.sequence);
    uint8_t wire[WIRE_SIZE];
    encode(reply, wire);
    if (esp_now_send(pending.mac, wire, sizeof(wire)) != ESP_OK) {
      ++sendQueueFailures;
    }
  }
}

void printReport() {
  Serial.print("Beacons sent: ");
  Serial.print(beaconSequence);
  Serial.print(" | beacon callback success/fail: ");
  Serial.print(beaconSendSuccesses);
  Serial.print('/');
  Serial.print(beaconSendFailures);
  Serial.print(" | node callbacks: ");
  Serial.print(receiveCallbacks);
  Serial.print(" | valid probes: ");
  Serial.print(validProbes);
  Serial.print(" | last node MAC: ");
  if (lastNodeMacValid) {
    printMac(lastNodeMac);
  } else {
    Serial.print("none");
  }
  Serial.print(" | invalid packets: ");
  Serial.print(invalidPackets);
  Serial.print(" | wrong network: ");
  Serial.print(wrongNetworkPackets);
  Serial.print(" | other gateway beacons: ");
  Serial.print(otherBeacons);
  Serial.print(" | peer add failures: ");
  Serial.print(peerAddFailures);
  Serial.print(" | send queue failures: ");
  Serial.print(sendQueueFailures);
  Serial.print(" | reply send callbacks success/fail: ");
  Serial.print(replyCallbacks);
  Serial.print('/');
  Serial.println(replySendFailures);
}
#else
void onDataSent(uint8_t *mac, uint8_t status) {
  (void)mac;
  if (status == 0) {
    ++sendCallbacksSuccess;
  } else {
    ++sendCallbacksFailure;
  }
}

void onDataReceived(uint8_t *mac, uint8_t *data, uint8_t length) {
  ++receiveCallbacks;
  if (mac == nullptr) {
    ++invalidPackets;
    return;
  }
  DiscoveryPacket packet;
  const DecodeResult result = decode(data, length, RSSI_PROBE_CHANNEL, NETWORK_ID, packet);
  if (result == DecodeResult::WRONG_NETWORK) {
    ++wrongNetworkPackets;
    return;
  }
  if (result != DecodeResult::OK) {
    ++invalidPackets;
    return;
  }
  int8_t candidateIndex = -1;
  for (uint8_t i = 0; i < 2; ++i) {
    if (memcmp(mac, CANDIDATE_MACS[i], sizeof(CANDIDATE_MACS[i])) == 0) {
      candidateIndex = static_cast<int8_t>(i);
      break;
    }
  }
  if (candidateIndex < 0) {
    ++invalidPackets;
    return;
  }

  if (packet.type == PACKET_BEACON) {
    ++candidates[candidateIndex].beacons;
    return;
  }
  if (packet.type != PACKET_REPLY) {
    ++invalidPackets;
    return;
  }

  if (probePending && packet.sequence == pendingProbeSequence &&
      candidateIndex == pendingCandidate) {
    candidates[candidateIndex].lastLatencyMs = millis() - pendingProbeAt;
    ++candidates[candidateIndex].replies;
    lastReplySequence = packet.sequence;
    lastReplyLatencyMs = candidates[candidateIndex].lastLatencyMs;
    compilerMemoryBarrier();
    probePending = false;
  } else {
    ++unmatchedReplies;
  }
}

bool initializeEspNow() {
  if (esp_now_init() != 0) {
    return false;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  if (esp_now_register_recv_cb(onDataReceived) != 0 ||
      esp_now_register_send_cb(onDataSent) != 0) {
    return false;
  }
  for (uint8_t i = 0; i < 2; ++i) {
    if (esp_now_add_peer(const_cast<uint8_t *>(CANDIDATE_MACS[i]),
                         ESP_NOW_ROLE_COMBO, RSSI_PROBE_CHANNEL, nullptr,
                         0) != 0) {
      return false;
    }
  }
  return true;
}

void sendProbe() {
  const uint8_t candidate = nextCandidate;
  nextCandidate = static_cast<uint8_t>((nextCandidate + 1) % 2);
  const uint32_t sequence = ++probeSequence;
  DiscoveryPacket probe = makePacket(PACKET_PROBE, sequence);
  uint8_t wire[WIRE_SIZE];
  encode(probe, wire);
  pendingProbeSequence = sequence;
  pendingProbeAt = millis();
  pendingCandidate = candidate;
  compilerMemoryBarrier();
  probePending = true;
  const int result = esp_now_send(
      const_cast<uint8_t *>(CANDIDATE_MACS[candidate]),
      wire, sizeof(wire));
  if (result != 0) {
    ++probeQueueFailures;
    probePending = false;
  } else {
    ++candidates[candidate].probes;
  }
  nextProbeAt = millis() + PROBE_INTERVAL_MS;
}

void checkProbeTimeout() {
  if (!probePending || millis() - pendingProbeAt < PROBE_TIMEOUT_MS) {
    return;
  }
  compilerMemoryBarrier();
  if (probePending) {
    ++candidates[pendingCandidate].timeouts;
    probePending = false;
    ++probeTimeouts;
  }
}

void printReport() {
  for (uint8_t i = 0; i < 2; ++i) {
    Serial.print("Candidate ");
    Serial.print(i + 1);
    Serial.print(" MAC=");
    printMac(CANDIDATE_MACS[i]);
    Serial.print(" beacons=");
    Serial.print(candidates[i].beacons);
    Serial.print(" probes=");
    Serial.print(candidates[i].probes);
    Serial.print(" replies=");
    Serial.print(candidates[i].replies);
    Serial.print(" timeouts=");
    Serial.print(candidates[i].timeouts);
    Serial.print(" last_latency_ms=");
    Serial.println(candidates[i].lastLatencyMs);
  }
  Serial.print("Probe callbacks success/fail: ");
  Serial.print(sendCallbacksSuccess);
  Serial.print('/');
  Serial.print(sendCallbacksFailure);
  Serial.print(" | unmatched replies: ");
  Serial.print(unmatchedReplies);
  Serial.print(" | probe queue failures: ");
  Serial.print(probeQueueFailures);
  Serial.print(" | probe timeouts: ");
  Serial.print(probeTimeouts);
  Serial.print(" | invalid packets: ");
  Serial.print(invalidPackets);
  Serial.print(" | wrong network: ");
  Serial.print(wrongNetworkPackets);
  Serial.print(" | receive callbacks: ");
  Serial.println(receiveCallbacks);
}
#endif
} // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  Serial.println();
  Serial.println("TerraSmart ESP-NOW discovery link probe v2");
  Serial.print("Network ID: 0x"); Serial.println(NETWORK_ID, HEX);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  factoryStaMacValid = readStaMac(factoryStaMac);

#if defined(ESPNOW_DISCOVERY_GATEWAY_PROBE)
  if (!factoryStaMacValid ||
      esp_wifi_set_mac(WIFI_IF_STA, GATEWAY_ACTIVE_MAC) != ESP_OK) {
    Serial.println("Failed to set FDRS-compatible gateway STA MAC.");
    return;
  }
#endif

  configureRadio();
  activeStaMacValid = readStaMac(activeStaMac);
  printMacLine("Factory STA MAC: ", factoryStaMac, factoryStaMacValid);
  printMacLine("Active STA MAC: ", activeStaMac, activeStaMacValid);
  Serial.print("Expected gateway active MAC: ");
  printMac(GATEWAY_ACTIVE_MAC);
  Serial.println();
  Serial.print("Configured channel: ");
  Serial.println(RSSI_PROBE_CHANNEL);
  const bool channelMatches = printActiveChannel();

  if (!factoryStaMacValid || !activeStaMacValid || !channelMatches) {
    Serial.println("Radio identity/channel check failed; probe not started.");
    return;
  }

  if (memcmp(activeStaMac, GATEWAY_ACTIVE_MAC, sizeof(activeStaMac)) != 0) {
#if defined(ESPNOW_DISCOVERY_GATEWAY_PROBE)
    Serial.println("Active STA MAC does not match the FDRS gateway address.");
    return;
#else
    Serial.println("Node keeps its factory STA MAC; gateway uses FDRS address.");
#endif
  }

  radioReady = initializeEspNow();
  Serial.println(radioReady ? "ESP-NOW initialized." : "ESP-NOW init failed.");
  if (!radioReady) {
    return;
  }

#if defined(ESPNOW_DISCOVERY_GATEWAY_PROBE)
  nextBeaconAt = millis() + 250;
  Serial.println("Role: FDRS-address gateway, broadcast beacon + unicast reply.");
#else
  nextProbeAt = millis() + 500;
  Serial.print("Role: leaf node probing gateways ");
  printMac(CANDIDATE_MACS[0]);
  Serial.print(" and ");
  printMac(CANDIDATE_MACS[1]);
  Serial.println("; factory MAC remains active as node source.");
#endif
}

void loop() {
  if (!radioReady) {
    delay(1000);
    return;
  }

  const uint32_t now = millis();
#if defined(ESPNOW_DISCOVERY_GATEWAY_PROBE)
  processProbeReplies();
  if (static_cast<int32_t>(now - nextBeaconAt) >= 0) {
    sendBeacon();
  }
#else
  checkProbeTimeout();
  if (static_cast<int32_t>(now - nextProbeAt) >= 0) {
    sendProbe();
  }
#endif

  if (now - lastReportAt >= REPORT_INTERVAL_MS) {
    lastReportAt = now;
    printReport();
  }
  delay(1);
}
