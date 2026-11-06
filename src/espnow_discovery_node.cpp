#include <Arduino.h>
#include <string.h>
#include "terrasmart_discovery_packet.h"
#include "terrasmart_discovery_association.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <user_interface.h>
#include <EEPROM.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#endif

namespace {
constexpr uint8_t CHANNEL = 1;
using namespace terras_smart_discovery;
namespace association = terras_smart_discovery::association;
constexpr uint32_t NETWORK_ID = TERRASMART_DISCOVERY_NETWORK_ID;
constexpr uint8_t MAX_CANDIDATES = 4;
constexpr uint8_t QUEUE_SIZE = 16;
constexpr uint32_t EXPIRY_MS = 6000;
constexpr uint32_t PROBE_INTERVAL_MS = 1000;
constexpr uint32_t TIMEOUT_MS = 800;
constexpr uint32_t REPORT_MS = 5000;
constexpr uint8_t FAIL_LIMIT = 3;
constexpr uint8_t CONFIRMATIONS = 3;

struct Event { uint8_t mac[6]; Packet packet; };
struct Candidate {
  bool used;
  uint8_t mac[6];
  uint32_t lastBeacon;
  uint32_t beacons;
  uint32_t probes;
  uint32_t replies;
  uint32_t timeouts;
  uint32_t latency;
  uint8_t successes;
  uint8_t failures;
};

Event queue[QUEUE_SIZE];
volatile uint8_t head = 0, tail = 0;
volatile uint32_t drops = 0, invalid = 0, wrongNetwork = 0;
#if defined(ESP32)
portMUX_TYPE queueLock = portMUX_INITIALIZER_UNLOCKED;
void lockQueue() { portENTER_CRITICAL(&queueLock); }
void unlockQueue() { portEXIT_CRITICAL(&queueLock); }
#else
// ESP8266 callbacks and loop run cooperatively; these sections do not yield.
void lockQueue() {}
void unlockQueue() {}
#endif
Candidate candidates[MAX_CANDIDATES] = {};
int8_t selected = -1, pending = -1;
uint8_t nextCandidate = 0;
uint32_t sequence = 0, pendingSequence = 0, pendingAt = 0;
uint32_t nextProbe = 0, lastReport = 0, unmatched = 0, peerErrors = 0;
uint32_t tableFull = 0;
bool ready = false;
bool preferredValid = false;
uint8_t preferredMac[6] = {};
#if defined(ESP32)
Preferences associationPreferences;
#endif

bool loadAssociation() {
  uint8_t bytes[association::WIRE_SIZE] = {};
#if defined(ESP8266)
  EEPROM.begin(64);
  for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = EEPROM.read(i);
#elif defined(ESP32)
  if (!associationPreferences.begin("ts-discovery", true)) return false;
  const size_t count = associationPreferences.getBytes("assoc", bytes, sizeof(bytes));
  associationPreferences.end();
  if (count != sizeof(bytes)) return false;
#endif
  association::Record record = {};
  if (association::decode(bytes, sizeof(bytes), NETWORK_ID, CHANNEL, record) !=
      association::DecodeResult::OK)
    return false;
  memcpy(preferredMac, record.mac, sizeof(preferredMac));
  preferredValid = true;
  return true;
}

bool saveAssociation(const uint8_t *mac) {
  if (!association::validMac(mac)) return false;
  if (preferredValid && memcmp(preferredMac, mac, sizeof(preferredMac)) == 0)
    return true;
  association::Record record = {};
  memcpy(record.mac, mac, sizeof(record.mac));
  record.channel = CHANNEL;
  record.networkId = NETWORK_ID;
  uint8_t bytes[association::WIRE_SIZE] = {};
  if (!association::encode(record, bytes, sizeof(bytes))) return false;
#if defined(ESP8266)
  for (size_t i = 0; i < sizeof(bytes); ++i) EEPROM.write(i, bytes[i]);
  if (!EEPROM.commit()) return false;
#elif defined(ESP32)
  if (!associationPreferences.begin("ts-discovery", false)) return false;
  const size_t written = associationPreferences.putBytes("assoc", bytes, sizeof(bytes));
  associationPreferences.end();
  if (written != sizeof(bytes)) return false;
#endif
  memcpy(preferredMac, mac, sizeof(preferredMac));
  preferredValid = true;
  return true;
}

void printMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; ++i) {
    if (i) Serial.print(':');
    if (mac[i] < 16) Serial.print('0');
    Serial.print(mac[i], HEX);
  }
}

void receive(const uint8_t *mac, const uint8_t *data, int length) {
  Packet packet;
  if (!mac || (mac[0] & 1) || length < 0) {
    lockQueue(); ++invalid; unlockQueue(); return;
  }
  const DecodeResult result = decode(data, static_cast<size_t>(length), CHANNEL, NETWORK_ID, packet);
  if (result == DecodeResult::WRONG_NETWORK) {
    lockQueue(); ++wrongNetwork; unlockQueue(); return;
  }
  if (result != DecodeResult::OK || (packet.type != BEACON && packet.type != REPLY)) {
    lockQueue(); ++invalid; unlockQueue(); return;
  }
  lockQueue();
  const uint8_t next = (head + 1) % QUEUE_SIZE;
  if (next == tail) ++drops;
  else {
    memcpy(queue[head].mac, mac, 6);
    queue[head].packet = packet;
    head = next;
  }
  unlockQueue();
}

#if defined(ESP32)
void onReceive(const uint8_t *mac, const uint8_t *data, int length) {
  receive(mac, data, length);
}
bool addPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  return esp_now_add_peer(&peer) == ESP_OK;
}
void removePeer(const uint8_t *mac) { esp_now_del_peer(mac); }
bool sendPacket(const uint8_t *mac, const Packet &packet) {
  uint8_t wire[WIRE_SIZE]; encode(packet, wire);
  return esp_now_send(mac, wire, sizeof(wire)) == ESP_OK;
}
#else
void onReceive(uint8_t *mac, uint8_t *data, uint8_t length) {
  receive(mac, data, length);
}
bool addPeer(const uint8_t *mac) {
  return esp_now_add_peer(const_cast<uint8_t *>(mac), ESP_NOW_ROLE_COMBO,
                          CHANNEL, nullptr, 0) == 0;
}
void removePeer(const uint8_t *mac) { esp_now_del_peer(const_cast<uint8_t *>(mac)); }
bool sendPacket(const uint8_t *mac, const Packet &packet) {
  uint8_t wire[WIRE_SIZE]; encode(packet, wire);
  return esp_now_send(const_cast<uint8_t *>(mac), wire, sizeof(wire)) == 0;
}
#endif

void setSelected(int8_t index) {
  if (selected == index) return;
  selected = index;
  Serial.print("Selection: ");
  if (index < 0) Serial.println("none");
  else {
    printMac(candidates[index].mac);
    Serial.println(" confirmed");
    if (!saveAssociation(candidates[index].mac))
      Serial.println("Association persistence failed");
    else
      Serial.println("Confirmed association saved");
  }
}

int8_t findCandidate(const uint8_t *mac) {
  for (uint8_t i = 0; i < MAX_CANDIDATES; ++i)
    if (candidates[i].used && memcmp(candidates[i].mac, mac, 6) == 0) return i;
  return -1;
}

void processEvents() {
  while (true) {
    Event event;
    lockQueue();
    if (tail == head) { unlockQueue(); break; }
    event = queue[tail];
    tail = (tail + 1) % QUEUE_SIZE;
    unlockQueue();
    int8_t index = findCandidate(event.mac);
    if (event.packet.type == BEACON) {
      if (index < 0) {
        bool hasFreeSlot = false;
        for (uint8_t i = 0; i < MAX_CANDIDATES; ++i) {
          if (!candidates[i].used) {
            hasFreeSlot = true;
            if (!addPeer(event.mac)) { ++peerErrors; break; }
            index = i;
            candidates[i] = {};
            candidates[i].used = true;
            memcpy(candidates[i].mac, event.mac, 6);
            if (preferredValid &&
                memcmp(preferredMac, event.mac, sizeof(preferredMac)) == 0)
              nextCandidate = i;
            Serial.print("Candidate discovered: "); printMac(event.mac); Serial.println();
            break;
          }
        }
        if (!hasFreeSlot) ++tableFull;
      }
      if (index >= 0) {
        candidates[index].lastBeacon = millis();
        ++candidates[index].beacons;
      }
    } else if (index >= 0 && index == pending &&
               event.packet.sequence == pendingSequence &&
               millis() - pendingAt < TIMEOUT_MS) {
      Candidate &c = candidates[index];
      c.latency = millis() - pendingAt;
      ++c.replies;
      c.failures = 0;
      if (c.successes < CONFIRMATIONS) ++c.successes;
      pending = -1;
    } else ++unmatched;
  }
}

void serviceCandidates(uint32_t now) {
  if (pending >= 0 && now - pendingAt >= TIMEOUT_MS) {
    Candidate &c = candidates[pending];
    ++c.timeouts;
    c.successes = 0;
    if (c.failures < FAIL_LIMIT) ++c.failures;
    if (c.failures >= FAIL_LIMIT) {
      if (selected == pending) setSelected(-1);
      if (preferredValid &&
          memcmp(preferredMac, c.mac, sizeof(preferredMac)) == 0) {
        preferredValid = false;
        Serial.println("Persisted gateway preference invalidated after probe failures");
      }
    }
    pending = -1;
  }
  for (uint8_t i = 0; i < MAX_CANDIDATES; ++i) {
    Candidate &c = candidates[i];
    if (!c.used || now - c.lastBeacon < EXPIRY_MS) continue;
    if (selected == i) setSelected(-1);
    if (preferredValid &&
        memcmp(preferredMac, c.mac, sizeof(preferredMac)) == 0) {
      preferredValid = false;
      Serial.println("Persisted gateway preference invalidated after beacon expiry");
    }
    if (pending == i) pending = -1;
    Serial.print("Candidate expired: "); printMac(c.mac); Serial.println();
    removePeer(c.mac);
    c = {};
  }
  if (selected < 0) {
    // Prefer the last confirmed gateway; fall back deterministically after loss.
    int8_t best = -1;
    for (uint8_t i = 0; i < MAX_CANDIDATES; ++i) {
      if (!candidates[i].used || candidates[i].successes < CONFIRMATIONS) continue;
      if (preferredValid &&
          memcmp(preferredMac, candidates[i].mac, sizeof(preferredMac)) == 0) {
        best = i;
        break;
      }
      if (best < 0 || memcmp(candidates[i].mac, candidates[best].mac, 6) < 0)
        best = i;
    }
    if (best >= 0) setSelected(best);
  }
  if (pending >= 0 || static_cast<int32_t>(now - nextProbe) < 0) return;
  if (preferredValid) {
    const int8_t preferred = findCandidate(preferredMac);
    if (preferred >= 0 && candidates[preferred].successes < CONFIRMATIONS) {
      Packet p = {PROBE, CHANNEL, NETWORK_ID, ++sequence};
      ++candidates[preferred].probes;
      pending = preferred;
      pendingSequence = p.sequence;
      pendingAt = now;
      if (!sendPacket(candidates[preferred].mac, p)) {
        ++peerErrors;
        pendingAt = now - TIMEOUT_MS;
      }
      nextProbe = now + (candidates[preferred].failures ? 250 : PROBE_INTERVAL_MS);
      return;
    }
  }
  for (uint8_t n = 0; n < MAX_CANDIDATES; ++n) {
    uint8_t i = nextCandidate;
    nextCandidate = (nextCandidate + 1) % MAX_CANDIDATES;
    if (!candidates[i].used) continue;
    Packet p = {PROBE, CHANNEL, NETWORK_ID, ++sequence};
    ++candidates[i].probes;
    pending = i;
    pendingSequence = p.sequence;
    pendingAt = now;
    if (!sendPacket(candidates[i].mac, p)) {
      // Queue errors count as failed attempts, not as successful confirmations.
      ++peerErrors;
      pendingAt = now - TIMEOUT_MS;
    }
    nextProbe = now + PROBE_INTERVAL_MS;
    return;
  }
}

void report() {
  Serial.print("Selected: ");
  if (selected < 0) Serial.print("none"); else printMac(candidates[selected].mac);
  lockQueue(); uint32_t localDrops = drops, localInvalid = invalid, localWrongNetwork = wrongNetwork; unlockQueue();
  Serial.print(" | invalid="); Serial.print(localInvalid);
  Serial.print(" wrong_network="); Serial.print(localWrongNetwork);
  Serial.print(" drops="); Serial.print(localDrops);
  Serial.print(" unmatched="); Serial.print(unmatched);
  Serial.print(" table_full="); Serial.print(tableFull);
  Serial.print(" peer/send errors="); Serial.println(peerErrors);
  Serial.print("Persisted gateway preference: ");
  if (preferredValid) { printMac(preferredMac); Serial.println(); }
  else Serial.println("none");
  for (const Candidate &c : candidates) {
    if (!c.used) continue;
    Serial.print("  MAC="); printMac(c.mac);
    Serial.print(" beacons="); Serial.print(c.beacons);
    Serial.print(" probes="); Serial.print(c.probes);
    Serial.print(" replies="); Serial.print(c.replies);
    Serial.print(" timeouts="); Serial.print(c.timeouts);
    Serial.print(" confirmations="); Serial.print(c.successes);
    Serial.print(" failures="); Serial.print(c.failures);
    Serial.print(" latency_ms="); Serial.println(c.latency);
  }
}
} // namespace

void setup() {
#if defined(ESP8266)
  Serial.begin(74880);
#else
  Serial.begin(115200);
#endif
  delay(100);
  Serial.println("\nTerraSmart experimental dynamic discovery v2");
  if (loadAssociation()) {
    Serial.print("Loaded persisted gateway preference: ");
    printMac(preferredMac);
    Serial.println();
  } else {
    Serial.println("No valid persisted gateway preference; discovering candidates");
  }
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
#if defined(ESP8266)
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  if (!wifi_set_channel(CHANNEL) || wifi_get_channel() != CHANNEL || esp_now_init() != 0 ||
      esp_now_set_self_role(ESP_NOW_ROLE_COMBO) != 0 || esp_now_register_recv_cb(onReceive) != 0) {
    Serial.println("Radio/ESP-NOW initialization failed"); return;
  }
#else
  if (esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK ||
      esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK || esp_now_init() != ESP_OK ||
      esp_now_register_recv_cb(onReceive) != ESP_OK) {
    Serial.println("Radio/ESP-NOW initialization failed"); return;
  }
#endif
  Serial.print("Node STA MAC: "); Serial.println(WiFi.macAddress());
  Serial.print("Network ID: 0x"); Serial.println(NETWORK_ID, HEX);
  Serial.println("Channel 1; discovery ready (diagnostic protocol, no authentication)");
  ready = true;
}

void loop() {
  if (!ready) { delay(1000); return; }
  processEvents();
  uint32_t now = millis();
  serviceCandidates(now);
  if (now - lastReport >= REPORT_MS) { lastReport = now; report(); }
  delay(1);
}
