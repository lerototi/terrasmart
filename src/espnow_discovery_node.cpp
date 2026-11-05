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

namespace {
constexpr uint8_t CHANNEL = 1;
constexpr uint16_t MAGIC = 0x5354;
constexpr uint8_t VERSION = 1;
constexpr uint8_t BEACON = 1;
constexpr uint8_t PROBE = 2;
constexpr uint8_t REPLY = 3;
constexpr uint8_t MAX_CANDIDATES = 4;
constexpr uint8_t QUEUE_SIZE = 16;
constexpr uint32_t EXPIRY_MS = 6000;
constexpr uint32_t PROBE_INTERVAL_MS = 1000;
constexpr uint32_t TIMEOUT_MS = 800;
constexpr uint32_t REPORT_MS = 5000;
constexpr uint8_t FAIL_LIMIT = 3;
constexpr uint8_t CONFIRMATIONS = 3;

struct __attribute__((packed)) Packet {
  uint16_t magic;
  uint8_t version;
  uint8_t type;
  uint8_t channel;
  uint32_t sequence;
};
static_assert(sizeof(Packet) == 9, "Unexpected diagnostic packet size");

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
volatile uint32_t drops = 0, invalid = 0;
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

void printMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; ++i) {
    if (i) Serial.print(':');
    if (mac[i] < 16) Serial.print('0');
    Serial.print(mac[i], HEX);
  }
}

void receive(const uint8_t *mac, const uint8_t *data, int length) {
  Packet packet;
  if (!mac || !data || length != sizeof(Packet)) {
    lockQueue(); ++invalid; unlockQueue(); return;
  }
  memcpy(&packet, data, sizeof(packet));
  if (packet.magic != MAGIC || packet.version != VERSION ||
      packet.channel != CHANNEL || (packet.type != BEACON && packet.type != REPLY) ||
      (mac[0] & 1)) {
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
  return esp_now_send(mac, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet)) == ESP_OK;
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
  return esp_now_send(const_cast<uint8_t *>(mac),
                      const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(&packet)),
                      sizeof(packet)) == 0;
}
#endif

void setSelected(int8_t index) {
  if (selected == index) return;
  selected = index;
  Serial.print("Selection: ");
  if (index < 0) Serial.println("none");
  else { printMac(candidates[index].mac); Serial.println(" confirmed"); }
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
    if (selected == pending && c.failures >= FAIL_LIMIT) setSelected(-1);
    pending = -1;
  }
  for (uint8_t i = 0; i < MAX_CANDIDATES; ++i) {
    Candidate &c = candidates[i];
    if (!c.used || now - c.lastBeacon < EXPIRY_MS) continue;
    if (selected == i) setSelected(-1);
    if (pending == i) pending = -1;
    Serial.print("Candidate expired: "); printMac(c.mac); Serial.println();
    removePeer(c.mac);
    c = {};
  }
  if (selected < 0) {
    // Choose deterministically among confirmed, currently advertised candidates.
    int8_t best = -1;
    for (uint8_t i = 0; i < MAX_CANDIDATES; ++i)
      if (candidates[i].used && candidates[i].successes >= CONFIRMATIONS &&
          (best < 0 || memcmp(candidates[i].mac, candidates[best].mac, 6) < 0))
        best = i;
    if (best >= 0) setSelected(best);
  }
  if (pending >= 0 || static_cast<int32_t>(now - nextProbe) < 0) return;
  for (uint8_t n = 0; n < MAX_CANDIDATES; ++n) {
    uint8_t i = nextCandidate;
    nextCandidate = (nextCandidate + 1) % MAX_CANDIDATES;
    if (!candidates[i].used) continue;
    Packet p = {MAGIC, VERSION, PROBE, CHANNEL, ++sequence};
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
  lockQueue(); uint32_t localDrops = drops, localInvalid = invalid; unlockQueue();
  Serial.print(" | invalid="); Serial.print(localInvalid);
  Serial.print(" drops="); Serial.print(localDrops);
  Serial.print(" unmatched="); Serial.print(unmatched);
  Serial.print(" table_full="); Serial.print(tableFull);
  Serial.print(" peer/send errors="); Serial.println(peerErrors);
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
  Serial.println("\nTerraSmart experimental dynamic discovery v1");
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
