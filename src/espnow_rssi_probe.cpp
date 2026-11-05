#include <Arduino.h>
#include <string.h>

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

#ifndef RSSI_PROBE_DEST_MAC_0
#define RSSI_PROBE_DEST_MAC_0 0x3C
#define RSSI_PROBE_DEST_MAC_1 0xE9
#define RSSI_PROBE_DEST_MAC_2 0x0E
#define RSSI_PROBE_DEST_MAC_3 0xD2
#define RSSI_PROBE_DEST_MAC_4 0xBB
#define RSSI_PROBE_DEST_MAC_5 0xCE
#endif

namespace {
constexpr uint16_t PROBE_MAGIC = 0x5354; // "TS" on wire, little-endian.
constexpr uint8_t PROBE_VERSION = 1;
constexpr uint8_t PROBE_TYPE_BEACON = 0x01;
constexpr uint32_t BEACON_INTERVAL_MS = 1000;
constexpr uint32_t REPORT_INTERVAL_MS = 5000;
constexpr uint32_t ACTIVE_GATEWAY_TIMEOUT_MS = 3000;
constexpr uint32_t MAX_INITIAL_BEACON_DELAY_MS = 1000;
constexpr size_t MAX_TRACKED_GATEWAYS = 8;
constexpr uint8_t RX_QUEUE_SIZE = 16;
constexpr uint8_t BROADCAST_MAC[6] = {0xff, 0xff, 0xff,
                                      0xff, 0xff, 0xff};
constexpr uint8_t RECEIVER_MAC[6] = {
    RSSI_PROBE_DEST_MAC_0, RSSI_PROBE_DEST_MAC_1, RSSI_PROBE_DEST_MAC_2,
    RSSI_PROBE_DEST_MAC_3, RSSI_PROBE_DEST_MAC_4, RSSI_PROBE_DEST_MAC_5};
#if defined(ESP8266)
constexpr uint32_t PROBE_SERIAL_BAUD = 74880;
#else
constexpr uint32_t PROBE_SERIAL_BAUD = 115200;
#endif

// Diagnostic beacon: fixed-size, versioned and independent from the production
// FDRS envelope. The sender MAC is the stable gateway identity.
struct __attribute__((packed)) ProbePacket {
  uint16_t magic;
  uint8_t version;
  uint8_t type;
  uint8_t channel;
  uint32_t sequence;
};

struct GatewayObservation {
  uint8_t mac[6];
  uint32_t packets;
  uint32_t lastSequence;
  uint32_t lastSeenAt;
  uint32_t forwardPackets;
  uint32_t missingSequences;
  uint32_t duplicates;
  uint32_t outOfOrder;
  bool sequenceInitialized;
  bool active;
};

struct PendingBeacon {
  uint8_t mac[6];
  uint32_t sequence;
};

static_assert(sizeof(ProbePacket) == 9,
              "ProbePacket wire format must remain exactly 9 bytes");

uint8_t deviceMac[6] = {};
bool deviceMacValid = false;
bool radioReady = false;
uint32_t lastReportAt = 0;

#if defined(ESPNOW_RSSI_BEACON) || defined(ESPNOW_RSSI_UNICAST_TEST)
uint32_t nextBeaconAt = 0;
uint32_t beaconSequence = 0;
uint32_t sentPackets = 0;
uint32_t sendFailures = 0;
#else
GatewayObservation gateways[MAX_TRACKED_GATEWAYS] = {};
PendingBeacon receiveQueue[RX_QUEUE_SIZE] = {};
volatile uint8_t receiveHead = 0;
volatile uint8_t receiveTail = 0;
volatile uint32_t receiveCallbacks = 0;
volatile uint32_t invalidPackets = 0;
volatile uint32_t queueDrops = 0;
uint32_t untrackedPackets = 0;
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

bool readFactoryStaMac(uint8_t *mac) {
  if (mac == nullptr) {
    return false;
  }

#if defined(ESP8266)
  // Read the station MAC from the SDK before any FDRS code can replace it.
  return wifi_get_macaddr(STATION_IF, mac);
#elif defined(ESP32)
  return esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK;
#else
  return false;
#endif
}

void configureRadio() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

#if defined(ESP8266)
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
#elif defined(ESP32)
  const esp_err_t powerSaveResult = esp_wifi_set_ps(WIFI_PS_NONE);
  if (powerSaveResult != ESP_OK) {
    Serial.print("Could not disable Wi-Fi power save, error: ");
    Serial.println(static_cast<int>(powerSaveResult));
  }
#endif

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

void printRadioChannel(const char *stage) {
  Serial.print("Radio channel ");
  Serial.print(stage);
  Serial.print(": ");
#if defined(ESP8266)
  Serial.println(wifi_get_channel());
#elif defined(ESP32)
  uint8_t primaryChannel = 0;
  wifi_second_chan_t secondaryChannel = WIFI_SECOND_CHAN_NONE;
  const esp_err_t result =
      esp_wifi_get_channel(&primaryChannel, &secondaryChannel);
  if (result == ESP_OK) {
    Serial.println(primaryChannel);
  } else {
    Serial.print("unavailable, error: ");
    Serial.println(static_cast<int>(result));
  }
#endif
}

#if defined(ESPNOW_RSSI_BEACON) || defined(ESPNOW_RSSI_UNICAST_TEST)
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
#if defined(ESPNOW_RSSI_UNICAST_TEST)
  return esp_now_register_send_cb(onDataSent) == 0 &&
         esp_now_add_peer(const_cast<uint8_t *>(RECEIVER_MAC),
                          ESP_NOW_ROLE_COMBO, RSSI_PROBE_CHANNEL, nullptr, 0) ==
             0;
#else
  return esp_now_register_send_cb(onDataSent) == 0 &&
         esp_now_add_peer(const_cast<uint8_t *>(BROADCAST_MAC),
                          ESP_NOW_ROLE_COMBO, RSSI_PROBE_CHANNEL, nullptr, 0) ==
             0;
#endif
#elif defined(ESP32)
  if (esp_now_init() != ESP_OK) {
    return false;
  }
  if (esp_now_register_send_cb(onDataSent) != ESP_OK) {
    return false;
  }

  esp_now_peer_info_t peerInfo = {};
#if defined(ESPNOW_RSSI_UNICAST_TEST)
  memcpy(peerInfo.peer_addr, RECEIVER_MAC, sizeof(RECEIVER_MAC));
#else
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, sizeof(BROADCAST_MAC));
#endif
  peerInfo.channel = RSSI_PROBE_CHANNEL;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;
  return esp_now_add_peer(&peerInfo) == ESP_OK;
#endif
}

void sendBeacon() {
  ProbePacket packet = {PROBE_MAGIC, PROBE_VERSION, PROBE_TYPE_BEACON,
                        RSSI_PROBE_CHANNEL, ++beaconSequence};

#if defined(ESPNOW_RSSI_UNICAST_TEST)
  const uint8_t *destination = RECEIVER_MAC;
#else
  const uint8_t *destination = BROADCAST_MAC;
#endif

#if defined(ESP8266)
  const int result = esp_now_send(
      const_cast<uint8_t *>(destination), reinterpret_cast<uint8_t *>(&packet),
      sizeof(packet));
  if (result != 0) {
    ++sendFailures;
  }
#elif defined(ESP32)
  const esp_err_t result = esp_now_send(
      destination, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
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
  Serial.println("Stage: esp_now_init begin");
  if (esp_now_init() != 0) {
    Serial.println("Stage: esp_now_init failed");
    return false;
  }
  Serial.println("Stage: esp_now_init done");
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  Serial.println("Stage: register receive callback begin");
  const bool registered = esp_now_register_recv_cb(onDataReceived) == 0;
  Serial.println(registered ? "Stage: register receive callback done"
                            : "Stage: register receive callback failed");
  return registered;
#elif defined(ESP32)
  if (esp_now_init() != ESP_OK) {
    return false;
  }
  return esp_now_register_recv_cb(onDataReceived) == ESP_OK;
#endif
}

bool sameMac(const uint8_t *left, const uint8_t *right) {
  return memcmp(left, right, 6) == 0;
}

GatewayObservation *findOrAllocateGateway(const uint8_t *mac) {
  GatewayObservation *available = nullptr;
  for (size_t i = 0; i < MAX_TRACKED_GATEWAYS; ++i) {
    if (gateways[i].active && sameMac(gateways[i].mac, mac)) {
      return &gateways[i];
    }
    if (!gateways[i].active && available == nullptr) {
      available = &gateways[i];
    }
  }

  if (available == nullptr) {
    return nullptr;
  }
  memcpy(available->mac, mac, sizeof(available->mac));
  available->packets = 0;
  available->lastSequence = 0;
  available->lastSeenAt = 0;
  available->forwardPackets = 0;
  available->missingSequences = 0;
  available->duplicates = 0;
  available->outOfOrder = 0;
  available->sequenceInitialized = false;
  available->active = true;
  return available;
}

#if defined(ESP8266)
void onDataReceived(uint8_t *mac, uint8_t *data, uint8_t length) {
#elif defined(ESP32)
void onDataReceived(const uint8_t *mac, const uint8_t *data, int length) {
#endif
  ++receiveCallbacks;
  if (mac == nullptr || data == nullptr ||
      length != static_cast<int>(sizeof(ProbePacket))) {
    ++invalidPackets;
    return;
  }

  ProbePacket packet;
  memcpy(&packet, data, sizeof(packet));
  if (packet.magic != PROBE_MAGIC || packet.version != PROBE_VERSION ||
      packet.type != PROBE_TYPE_BEACON ||
      packet.channel != RSSI_PROBE_CHANNEL) {
    ++invalidPackets;
    return;
  }

  const uint8_t head = receiveHead;
  const uint8_t nextHead = static_cast<uint8_t>((head + 1) % RX_QUEUE_SIZE);
  if (nextHead == receiveTail) {
    ++queueDrops;
    return;
  }
  memcpy(receiveQueue[head].mac, mac, sizeof(receiveQueue[head].mac));
  receiveQueue[head].sequence = packet.sequence;
  compilerMemoryBarrier();
  receiveHead = nextHead;
}

void processReceivedBeacons() {
  while (true) {
    const uint8_t tail = receiveTail;
    if (tail == receiveHead) {
      break;
    }
    compilerMemoryBarrier();
    const PendingBeacon beacon = receiveQueue[tail];
    receiveTail = static_cast<uint8_t>((tail + 1) % RX_QUEUE_SIZE);

    GatewayObservation *gateway = findOrAllocateGateway(beacon.mac);
    if (gateway == nullptr) {
      ++untrackedPackets;
      continue;
    }
    ++gateway->packets;
    gateway->lastSeenAt = millis();
    if (!gateway->sequenceInitialized) {
      gateway->lastSequence = beacon.sequence;
      gateway->forwardPackets = 1;
      gateway->sequenceInitialized = true;
      continue;
    }

    // Signed modular comparison handles uint32 sequence wrap as long as a
    // gateway does not go silent for half the sequence space.
    const int32_t sequenceDelta =
        static_cast<int32_t>(beacon.sequence - gateway->lastSequence);
    if (sequenceDelta > 0) {
      gateway->missingSequences += static_cast<uint32_t>(sequenceDelta - 1);
      ++gateway->forwardPackets;
      gateway->lastSequence = beacon.sequence;
    } else if (sequenceDelta == 0) {
      ++gateway->duplicates;
    } else {
      ++gateway->outOfOrder;
    }
  }
}
#endif

void printIdentity() {
  if (!deviceMacValid) {
    Serial.println("Factory STA MAC: unavailable");
    return;
  }

  Serial.print("Factory STA MAC: ");
  printMac(deviceMac);
  Serial.println();
}
} // namespace

void setup() {
  Serial.begin(PROBE_SERIAL_BAUD);
  delay(100);
  Serial.println();
  Serial.println("TerraSmart ESP-NOW identity/compatibility probe v1");

  Serial.println("Stage: configure radio begin");
  configureRadio();
  Serial.println("Stage: configure radio done");
  printRadioChannel("after radio setup");
  Serial.println("Stage: read factory STA MAC begin");
  deviceMacValid = readFactoryStaMac(deviceMac);
  Serial.println(deviceMacValid ? "Stage: read factory STA MAC done"
                                : "Stage: read factory STA MAC failed");
  Serial.print("Configured channel: ");
  Serial.println(RSSI_PROBE_CHANNEL);
  printIdentity();
  if (!deviceMacValid) {
    Serial.println("Could not read factory STA MAC; probe will not start.");
    return;
  }

  Serial.println("Stage: initialize ESP-NOW begin");
  radioReady = initializeEspNow();
  Serial.println(radioReady ? "Stage: initialize ESP-NOW done"
                            : "Stage: initialize ESP-NOW failed");
  if (!radioReady) {
    Serial.println("ESP-NOW initialization failed; check channel and radio setup.");
    return;
  }
  printRadioChannel("after ESP-NOW init");

  randomSeed(micros());
#if defined(ESPNOW_RSSI_BEACON) || defined(ESPNOW_RSSI_UNICAST_TEST)
  nextBeaconAt = millis() + random(100, MAX_INITIAL_BEACON_DELAY_MS);
#if defined(ESPNOW_RSSI_UNICAST_TEST)
  Serial.print("Role: unicast diagnostic beacon to receiver MAC ");
  printMac(RECEIVER_MAC);
  Serial.println();
#else
  Serial.println("Role: versioned broadcast beacon");
#endif
#else
  Serial.print("Role: receiver, tracking up to ");
  Serial.print(MAX_TRACKED_GATEWAYS);
  Serial.println(" distinct gateway MACs");
  Serial.println("RSSI: unavailable in the configured ESP-NOW receive callback");
  Serial.println("Sequence gaps are estimates; loss at sender and late frames can affect them.");
#endif
}

void loop() {
  if (!radioReady) {
    delay(1000);
    return;
  }

  const uint32_t now = millis();
#if defined(ESPNOW_RSSI_BEACON) || defined(ESPNOW_RSSI_UNICAST_TEST)
  if (static_cast<int32_t>(now - nextBeaconAt) >= 0) {
    sendBeacon();
  }

  if (now - lastReportAt >= REPORT_INTERVAL_MS) {
    lastReportAt = now;
    Serial.print("Beacon sequence: ");
    Serial.print(beaconSequence);
#if defined(ESPNOW_RSSI_UNICAST_TEST)
    Serial.print(" | unicast callback successes (peer ACK): ");
#else
    Serial.print(" | send callback successes: ");
#endif
    Serial.print(sentPackets);
    Serial.print(" | send failures: ");
    Serial.println(sendFailures);
  }
#else
  processReceivedBeacons();
  if (now - lastReportAt >= REPORT_INTERVAL_MS) {
    lastReportAt = now;
    uint8_t activeGateways = 0;
    uint8_t observedGateways = 0;
    Serial.println("Observed gateways:");
    for (size_t i = 0; i < MAX_TRACKED_GATEWAYS; ++i) {
      if (!gateways[i].active) {
        continue;
      }
      ++observedGateways;
      const uint32_t age = now - gateways[i].lastSeenAt;
      if (age <= ACTIVE_GATEWAY_TIMEOUT_MS) {
        ++activeGateways;
      }
      Serial.print("  MAC=");
      printMac(gateways[i].mac);
      Serial.print(" packets=");
      Serial.print(gateways[i].packets);
      Serial.print(" sequence=");
      Serial.print(gateways[i].lastSequence);
      Serial.print(" last_seen_ms_ago=");
      Serial.print(age);
      Serial.print(" missing_est=");
      Serial.print(gateways[i].missingSequences);
      Serial.print(" loss_est=");
      const uint32_t sequenceTotal =
          gateways[i].forwardPackets + gateways[i].missingSequences;
      const uint64_t lossTenths =
          sequenceTotal == 0
              ? 0
              : (static_cast<uint64_t>(gateways[i].missingSequences) * 1000) /
                    sequenceTotal;
      Serial.print(static_cast<uint32_t>(lossTenths / 10));
      Serial.print('.');
      Serial.print(static_cast<uint32_t>(lossTenths % 10));
      Serial.print("% duplicates=");
      Serial.print(gateways[i].duplicates);
      Serial.print(" out_of_order=");
      Serial.print(gateways[i].outOfOrder);
      Serial.println(" RSSI=unavailable");
    }
    Serial.print("Recently active gateways: ");
    Serial.print(activeGateways);
    Serial.print(" | observed gateways: ");
    Serial.print(observedGateways);
    Serial.print(" | invalid packets: ");
    Serial.print(invalidPackets);
    Serial.print(" | receive callbacks: ");
    Serial.print(receiveCallbacks);
    Serial.print(" | receive queue drops: ");
    Serial.print(queueDrops);
    Serial.print(" | untracked packets: ");
    Serial.println(untrackedPackets);
  }
#endif

  delay(1);
}
