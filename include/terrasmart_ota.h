#pragma once

#include <Arduino.h>
#include "terrasmart_ota_protocol.h"
#include "terrasmart_ota_config.h"

#if defined(TERRASMART_OTA_NODE) || defined(TERRASMART_OTA_ESPNOW_GATEWAY) || defined(TERRASMART_OTA_MQTT)
#if defined(ESP8266)
#include <Updater.h>
#define TERRASMART_OTA_ABORT() Update.end(true)
#else
#include <Update.h>
#define TERRASMART_OTA_ABORT() Update.abort()
#endif
#endif

#if defined(TERRASMART_OTA_MQTT)
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
extern PubSubClient client;
void terrasSmartOtaServiceStatusLed();
#endif

#if defined(TERRASMART_OTA_NODE) || defined(TERRASMART_OTA_ESPNOW_GATEWAY)
#if defined(ESP8266)
#include <espnow.h>
#else
#include <esp_now.h>
#endif
#endif

namespace TerraSmartOta {

constexpr uint8_t STATUS_OK = 0;
constexpr uint8_t STATUS_BAD_PACKET = 1;
constexpr uint8_t STATUS_UPDATE_ERROR = 2;
constexpr uint8_t STATUS_BAD_CRC = 3;

inline uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return crc;
}

inline size_t serialize(const Packet &packet, uint8_t *output, size_t capacity) {
  const size_t length = 9 + packet.size;
  if (output == nullptr || packet.size > MAX_CHUNK || capacity < length) {
    return 0;
  }
  memcpy(output, &packet, length);
  return length;
}

inline bool parse(const uint8_t *input, size_t length, Packet &packet) {
  if (!isPacket(input, length)) {
    return false;
  }
  memset(&packet, 0, sizeof(packet));
  memcpy(&packet, input, length);
  return true;
}

inline String toUartLine(const Packet &packet) {
  uint8_t bytes[9 + MAX_CHUNK];
  const size_t length = serialize(packet, bytes, sizeof(bytes));
  String line = "!TSOTA:";
  static const char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < length; ++i) {
    line += hex[bytes[i] >> 4];
    line += hex[bytes[i] & 0x0F];
  }
  return line;
}

inline int fromHex(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

inline bool fromUartLine(const String &line, Packet &packet) {
  // Serial.println() terminates UART frames with CRLF. readStringUntil('\n')
  // leaves the carriage return in the String, which otherwise makes the hex
  // payload appear to have an invalid odd length and silently drops the frame.
  String normalizedLine = line;
  normalizedLine.trim();
  if (!normalizedLine.startsWith("!TSOTA:") ||
      ((normalizedLine.length() - 7) & 1U)) {
    return false;
  }
  uint8_t bytes[9 + MAX_CHUNK];
  const size_t length = (normalizedLine.length() - 7) / 2;
  if (length > sizeof(bytes)) return false;
  for (size_t i = 0; i < length; ++i) {
    const int high = fromHex(normalizedLine[7 + i * 2]);
    const int low = fromHex(normalizedLine[8 + i * 2]);
    if (high < 0 || low < 0) return false;
    bytes[i] = static_cast<uint8_t>((high << 4) | low);
  }
  return parse(bytes, length, packet);
}

#if defined(TERRASMART_OTA_NODE) || defined(TERRASMART_OTA_ESPNOW_GATEWAY)
struct PendingEspNowPacket {
  volatile bool ready = false;
  uint8_t bytes[9 + MAX_CHUNK] = {};
  uint8_t source[6] = {};
  uint16_t length = 0;
};

static PendingEspNowPacket pendingEspNow;
static bool updaterStarted = false;
static uint32_t expectedSize = 0;
static uint32_t receivedSize = 0;
static uint32_t runningCrc = 0xFFFFFFFFUL;
static uint16_t nextSequence = 0;
static bool restartAfterAck = false;
#if defined(TERRASMART_OTA_ESPNOW_GATEWAY)
static bool forwardedUpdateInProgress = false;
static uint8_t forwardedUpdateTarget = 0;
static uint16_t forwardedTerminalSequence = 0;
static bool forwardedTerminalSequenceValid = false;
static uint32_t forwardedUpdateLastActivity = 0;
static uint16_t forwardedBeginSequence = 0;
static uint8_t forwardedNodeMac[6] = {};
static bool forwardedNodeMacValid = false;
constexpr uint32_t FORWARDED_UPDATE_TIMEOUT_MS = 60000;
#endif

inline bool captureEspNow(const uint8_t *data, size_t length,
                          const uint8_t *source) {
  if (!isPacket(data, length)) return false;
  if (pendingEspNow.ready || length > sizeof(pendingEspNow.bytes)) return true;
  memcpy(pendingEspNow.bytes, data, length);
  memcpy(pendingEspNow.source, source, sizeof(pendingEspNow.source));
  pendingEspNow.length = static_cast<uint16_t>(length);
  pendingEspNow.ready = true;
  return true;
}

inline bool sendEspNow(const uint8_t *address, const Packet &packet) {
  uint8_t bytes[9 + MAX_CHUNK];
  const size_t length = serialize(packet, bytes, sizeof(bytes));
  if (!length) return false;
#if defined(ESP8266)
  uint8_t *mutableAddress = const_cast<uint8_t *>(address);
  if (!esp_now_is_peer_exist(mutableAddress)) {
    if (esp_now_add_peer(mutableAddress, ESP_NOW_ROLE_COMBO,
                         FDRS_ESPNOW_CHANNEL, nullptr, 0) != 0) return false;
  }
  return esp_now_send(mutableAddress, bytes, length) == 0;
#elif defined(ESP32)
  if (!esp_now_is_peer_exist(address)) {
    esp_now_peer_info_t peer{};
    peer.ifidx = WIFI_IF_STA;
    peer.channel = FDRS_ESPNOW_CHANNEL;
    memcpy(peer.peer_addr, address, 6);
    if (esp_now_add_peer(&peer) != ESP_OK) return false;
  }
  return esp_now_send(address, bytes, length) == ESP_OK;
#else
  return false;
#endif
}

inline void sendAck(const uint8_t *address, uint8_t target,
                    uint16_t sequence, uint8_t status) {
  sendEspNow(address, makePacket(Type::Ack, target, sequence, &status, 1));
}

inline bool handleUartLine(const String &line);

inline void otaWriteUartLine(const String &line) {
#if defined(ESP32)
  Serial1.println(line);
#else
  Serial.println(line);
#endif
}

inline uint8_t processUpdatePacket(const Packet &packet) {
  if (packet.type == Type::Abort) {
    if (updaterStarted) TERRASMART_OTA_ABORT();
    updaterStarted = false;
    return STATUS_OK;
  }
  if (packet.type == Type::Begin) {
    if (packet.size != sizeof(uint32_t)) return STATUS_BAD_PACKET;
    if (updaterStarted) {
      TERRASMART_OTA_ABORT();
      updaterStarted = false;
    }
    memcpy(&expectedSize, packet.payload, sizeof(expectedSize));
    if (expectedSize == 0 || !Update.begin(expectedSize)) {
#if defined(ESP32)
      Serial.printf("OTA update begin failed: size=%lu error=%u (%s)\n",
                    static_cast<unsigned long>(expectedSize), Update.getError(),
                    Update.errorString());
#else
      Serial.printf("OTA update begin failed: size=%lu\n",
                    static_cast<unsigned long>(expectedSize));
#endif
      updaterStarted = false;
      return STATUS_UPDATE_ERROR;
    }
    updaterStarted = true;
    receivedSize = 0;
    runningCrc = 0xFFFFFFFFUL;
    nextSequence = packet.sequence + 1;
    return STATUS_OK;
  }
  if (!updaterStarted) {
    return STATUS_BAD_PACKET;
  }
  // ACK loss must not write a retransmitted OTA block twice.
  if (packet.sequence + 1 == nextSequence && packet.type == Type::Data) {
    return STATUS_OK;
  }
  if (packet.sequence != nextSequence) return STATUS_BAD_PACKET;
  if (packet.type == Type::Data) {
    if (packet.size == 0 || receivedSize + packet.size > expectedSize ||
        Update.write(const_cast<uint8_t *>(packet.payload), packet.size) != packet.size) {
      TERRASMART_OTA_ABORT();
      updaterStarted = false;
      return STATUS_UPDATE_ERROR;
    }
    runningCrc = crc32Update(runningCrc, packet.payload, packet.size);
    receivedSize += packet.size;
    ++nextSequence;
    return STATUS_OK;
  }
  if (packet.type == Type::End) {
    if (packet.size != sizeof(uint32_t) || receivedSize != expectedSize) {
      TERRASMART_OTA_ABORT();
      updaterStarted = false;
      return STATUS_BAD_PACKET;
    }
    uint32_t expectedCrc;
    memcpy(&expectedCrc, packet.payload, sizeof(expectedCrc));
    if ((runningCrc ^ 0xFFFFFFFFUL) != expectedCrc) {
      TERRASMART_OTA_ABORT();
      updaterStarted = false;
      return STATUS_BAD_CRC;
    }
    if (!Update.end()) {
      updaterStarted = false;
      return STATUS_UPDATE_ERROR;
    }
    updaterStarted = false;
    restartAfterAck = true;
    return STATUS_OK;
  }
  return STATUS_BAD_PACKET;
}

inline void serviceEspNow() {
  if (!pendingEspNow.ready) return;
  Packet packet;
  uint8_t bytes[9 + MAX_CHUNK];
  uint8_t source[6];
  const uint16_t length = pendingEspNow.length;
  memcpy(bytes, pendingEspNow.bytes, length);
  memcpy(source, pendingEspNow.source, sizeof(source));
  pendingEspNow.ready = false;
  if (!parse(bytes, length, packet)) return;

#if defined(TERRASMART_OTA_ESPNOW_GATEWAY)
  if (packet.type == Type::Ack) {
    Serial1.println(toUartLine(packet));
    if (forwardedUpdateInProgress && packet.target == forwardedUpdateTarget) {
      forwardedUpdateLastActivity = millis();
      if (packet.sequence == forwardedBeginSequence || packet.sequence <= 12 ||
          (forwardedTerminalSequenceValid &&
           packet.sequence == forwardedTerminalSequence)) {
        Serial.printf("OTA ESP-NOW ACK: target=%u sequence=%u status=%u from=%02X:%02X:%02X:%02X:%02X:%02X\n",
                      packet.target, packet.sequence,
                      packet.size == 1 ? packet.payload[0] : 0xFF, source[0],
                      source[1], source[2], source[3], source[4], source[5]);
      }
      if (packet.sequence == forwardedBeginSequence && packet.size == 1 &&
          packet.payload[0] == STATUS_OK) {
        memcpy(forwardedNodeMac, source, sizeof(forwardedNodeMac));
        forwardedNodeMacValid = true;
        Serial.printf("OTA target %u discovered at %02X:%02X:%02X:%02X:%02X:%02X; switching to unicast\n",
                      forwardedUpdateTarget, source[0], source[1], source[2],
                      source[3], source[4], source[5]);
      }
    }
    if (forwardedUpdateInProgress && forwardedTerminalSequenceValid &&
        packet.target == forwardedUpdateTarget &&
        packet.sequence == forwardedTerminalSequence) {
      forwardedUpdateInProgress = false;
      forwardedTerminalSequenceValid = false;
      forwardedNodeMacValid = false;
    }
    return;
  }
  if (packet.target != TERRASMART_OTA_GATEWAY_ID) return;
#elif defined(TERRASMART_OTA_NODE)
  if (packet.target != TERRASMART_OTA_DEVICE_ID) return;
#endif

  const uint8_t status = processUpdatePacket(packet);
  if (packet.type != Type::Data || status != STATUS_OK ||
      packet.sequence <= 12 || packet.sequence % 128 == 0) {
    Serial.printf("OTA ESP-NOW RX: target=%u type=%u sequence=%u size=%u status=%u received=%lu/%lu\n",
                  packet.target, static_cast<uint8_t>(packet.type), packet.sequence,
                  packet.size, status, static_cast<unsigned long>(receivedSize),
                  static_cast<unsigned long>(expectedSize));
  }
  sendAck(source, packet.target, packet.sequence, status);
  if (restartAfterAck && packet.type == Type::End && status == STATUS_OK) {
    delay(100);
    ESP.restart();
  }
}
#endif

#if defined(TERRASMART_OTA_ESPNOW_GATEWAY)
inline bool handleUartLine(const String &line) {
  Packet packet;
  if (!fromUartLine(line, packet)) return false;
  if (packet.type != Type::Data || packet.sequence % 256 == 0 ||
      packet.sequence <= 12) {
    Serial.printf("OTA UART packet: target=%u type=%u sequence=%u size=%u\n",
                  packet.target, static_cast<uint8_t>(packet.type), packet.sequence,
                  packet.size);
  }
  if (packet.target == TERRASMART_OTA_GATEWAY_ID) {
    const bool isEnd = packet.type == Type::End;
    const uint8_t status = processUpdatePacket(packet);
    if (status != STATUS_OK) {
      Serial.printf("OTA packet rejected: type=%u sequence=%u status=%u\n",
                    static_cast<uint8_t>(packet.type), packet.sequence, status);
    }
    Serial.printf("OTA UART ACK sent: target=%u sequence=%u status=%u\n",
                  packet.target, packet.sequence, status);
    Serial1.println(toUartLine(makePacket(Type::Ack, packet.target,
                                          packet.sequence, &status, 1)));
    Serial1.flush();
    if (isEnd && status == STATUS_OK) {
      Serial.println("OTA_SUCCESS: ESP-NOW gateway firmware updated; restarting");
      Serial.flush();
      delay(100);
      ESP.restart();
    }
    return true;
  }

  // Discover the selected logical target with a broadcast Begin, then use the
  // MAC address from its ACK for unicast data packets. This avoids losing OTA
  // blocks to best-effort broadcast delivery.
  if (packet.type == Type::Begin) {
    if (!forwardedUpdateInProgress || forwardedUpdateTarget != packet.target ||
        forwardedBeginSequence != packet.sequence) {
      forwardedUpdateInProgress = true;
      forwardedUpdateTarget = packet.target;
      forwardedBeginSequence = packet.sequence;
      forwardedTerminalSequenceValid = false;
      forwardedNodeMacValid = false;
    }
    forwardedUpdateLastActivity = millis();
  } else if (forwardedUpdateInProgress &&
             packet.target == forwardedUpdateTarget &&
             (packet.type == Type::End || packet.type == Type::Abort)) {
    forwardedTerminalSequence = packet.sequence;
    forwardedTerminalSequenceValid = true;
    forwardedUpdateLastActivity = millis();
  } else if (forwardedUpdateInProgress &&
             packet.target == forwardedUpdateTarget) {
    forwardedUpdateLastActivity = millis();
  }
  bool queued;
  if (forwardedUpdateInProgress && packet.target == forwardedUpdateTarget &&
      packet.type != Type::Begin && forwardedNodeMacValid) {
    queued = sendEspNow(forwardedNodeMac, packet);
  } else {
    const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    queued = sendEspNow(broadcast, packet);
  }
  if (packet.type != Type::Data || packet.sequence <= 12 ||
      packet.sequence % 128 == 0 || !queued) {
    Serial.printf("OTA ESP-NOW TX: target=%u type=%u sequence=%u mode=%s queued=%s\n",
                  packet.target, static_cast<uint8_t>(packet.type), packet.sequence,
                  packet.type != Type::Begin && forwardedNodeMacValid ? "unicast" : "broadcast",
                  queued ? "yes" : "no");
  }
  return true;
}
#endif

#if defined(TERRASMART_OTA_MQTT)
static volatile bool uartAckReady = false;
static Packet uartAck{};
static bool updateInProgress = false;

inline bool consumeUartAck(const String &line) {
  Packet packet;
  if (!fromUartLine(line, packet) || packet.type != Type::Ack) return false;
  uartAck = packet;
  uartAckReady = true;
  return true;
}

inline void writeUartLine(const String &line) {
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  Serial1.println(line);
  Serial1.flush();
#else
  Serial.println(line);
  Serial.flush();
#endif
}

inline bool waitForAck(uint8_t target, uint16_t sequence) {
  const uint32_t started = millis();
  while (millis() - started < UART_TIMEOUT_MS) {
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
    Stream &uart = Serial1;
#else
    Stream &uart = Serial;
#endif
    if (uart.available()) {
      String line = uart.readStringUntil('\n');
      if (line.length()) {
        Serial.printf("OTA UART RX: length=%u prefix=%.48s\n",
                      static_cast<unsigned>(line.length()), line.c_str());
      }
      if (consumeUartAck(line)) {
        if (uartAck.target == target && uartAck.sequence == sequence &&
            uartAck.size == 1) {
          if (uartAck.payload[0] != STATUS_OK) {
            Serial.printf("OTA UART NACK: target=%u sequence=%u status=%u\n",
                          target, sequence, uartAck.payload[0]);
            return false;
          }
          return true;
        }
        Serial.printf("OTA UART ACK mismatch: expected target=%u sequence=%u; received target=%u sequence=%u size=%u\n",
                      target, sequence, uartAck.target, uartAck.sequence,
                      uartAck.size);
      }
    }
    client.loop();
    delay(1);
    terrasSmartOtaServiceStatusLed();
  }
  return false;
}

inline bool sendAndWait(const Packet &packet) {
  for (uint8_t attempt = 0; attempt < MAX_RETRIES; ++attempt) {
    uartAckReady = false;
    writeUartLine(toUartLine(packet));
    if (waitForAck(packet.target, packet.sequence)) return true;
  }
  Serial.printf("OTA UART ACK failed: target=%u type=%u sequence=%u after %u attempts\n",
                packet.target, static_cast<uint8_t>(packet.type), packet.sequence,
                MAX_RETRIES);
  return false;
}

inline bool isLocalHttpUrl(const String &url) {
  if (!url.startsWith("http://")) return false;
  const int authorityStart = 7;
  int authorityEnd = url.indexOf('/', authorityStart);
  if (authorityEnd < 0) authorityEnd = url.length();
  String authority = url.substring(authorityStart, authorityEnd);
  if (authority.length() == 0 || authority.indexOf('@') >= 0) return false;

  const int portSeparator = authority.indexOf(':');
  String host = portSeparator >= 0 ? authority.substring(0, portSeparator) : authority;
  host.toLowerCase();
  if (host.endsWith(".local")) return true;

  uint8_t octets[4] = {};
  uint8_t octetIndex = 0;
  int segmentStart = 0;
  for (int i = 0; i <= host.length(); ++i) {
    if (i != host.length() && host[i] != '.') continue;
    if (octetIndex >= 4 || i == segmentStart || i - segmentStart > 3) return false;
    uint16_t value = 0;
    for (int digit = segmentStart; digit < i; ++digit) {
      if (host[digit] < '0' || host[digit] > '9') return false;
      value = value * 10 + static_cast<uint8_t>(host[digit] - '0');
    }
    if (value > 255) return false;
    octets[octetIndex++] = static_cast<uint8_t>(value);
    segmentStart = i + 1;
  }
  if (octetIndex != 4) return false;
  const bool private10 = octets[0] == 10;
  const bool private172 = octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31;
  const bool private192 = octets[0] == 192 && octets[1] == 168;
  return private10 || private172 || private192;
}

inline bool beginFirmwareDownload(HTTPClient &http, const String &url,
                                  WiFiClient &plainClient,
                                  WiFiClientSecure &secureClient) {
  if (url.startsWith("https://")) {
#if defined(TERRASMART_OTA_ROOT_CA)
    secureClient.setCACert(TERRASMART_OTA_ROOT_CA);
    return http.begin(secureClient, url);
#else
    Serial.println("OTA refused: HTTPS requires TERRASMART_OTA_ROOT_CA");
    return false;
#endif
  }
  if (!isLocalHttpUrl(url)) {
    Serial.println("OTA refused: HTTP is allowed only for private IPs or .local hosts");
    return false;
  }
  return http.begin(plainClient, url);
}

inline bool parseUpdateCommand(JsonDocument &doc, uint8_t &target,
                               String &url, uint32_t &expectedSize,
                               uint32_t &expectedCrc) {
  if (!doc["target"].is<uint16_t>() || !doc["url"].is<const char *>() ||
      !doc["size"].is<uint32_t>() || !doc["crc32"].is<uint32_t>()) {
    return false;
  }
  const uint16_t requestedTarget = doc["target"].as<uint16_t>();
  if (requestedTarget > 255) return false;
  target = static_cast<uint8_t>(requestedTarget);
  url = doc["url"].as<String>();
  expectedSize = doc["size"].as<uint32_t>();
  expectedCrc = doc["crc32"].as<uint32_t>();
  return (url.startsWith("https://") || isLocalHttpUrl(url)) && expectedSize > 0 &&
         url.length() <= 240;
}

inline bool performUpdate(uint8_t target, const String &url,
                          uint32_t expectedSize, uint32_t expectedCrc) {
  const uint32_t maxForwardSize =
      target == TERRASMART_OTA_GATEWAY_ID
          ? TERRASMART_OTA_GATEWAY_FORWARD_LIMIT
          : TERRASMART_OTA_FORWARD_LIMIT;
  if (expectedSize == 0 || expectedSize > maxForwardSize) {
    Serial.printf("OTA target %u rejected: image size %lu exceeds limit %lu\n",
                  target, static_cast<unsigned long>(expectedSize),
                  static_cast<unsigned long>(maxForwardSize));
    return false;
  }

  Serial.printf("OTA relay started: target=%u size=%lu\n", target,
                static_cast<unsigned long>(expectedSize));
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  HTTPClient http;
  if (!beginFirmwareDownload(http, url, plainClient, secureClient)) {
    Serial.println("OTA download setup failed");
    return false;
  }
  const int response = http.GET();
  if (response != HTTP_CODE_OK ||
      http.getSize() != static_cast<int>(expectedSize)) {
    Serial.printf("OTA download failed: HTTP=%d expected_size=%lu received_size=%d\n",
                  response, static_cast<unsigned long>(expectedSize), http.getSize());
    http.end();
    return false;
  }

  uint32_t sequence = 0;
  Packet packet = makePacket(Type::Begin, target, sequence++,
                             reinterpret_cast<const uint8_t *>(&expectedSize),
                             sizeof(expectedSize));
  if (!sendAndWait(packet)) {
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t chunk[MAX_CHUNK];
  uint32_t remaining = expectedSize;
  uint32_t computedCrc = 0xFFFFFFFFUL;
  bool success = true;
  while (remaining > 0 && success) {
    const size_t wanted = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
    size_t received = 0;
    const uint32_t started = millis();
    while (received < wanted && millis() - started < 10000) {
      if (stream->available()) {
        received += stream->readBytes(chunk + received, wanted - received);
      } else {
        delay(1);
      }
      client.loop();
      terrasSmartOtaServiceStatusLed();
    }
    if (received != wanted) {
      Serial.printf("OTA download timed out at byte %lu of %lu\n",
                    static_cast<unsigned long>(expectedSize - remaining + received),
                    static_cast<unsigned long>(expectedSize));
      success = false;
      break;
    }
    computedCrc = crc32Update(computedCrc, chunk, received);
    packet = makePacket(Type::Data, target, sequence++, chunk, received);
    success = sendAndWait(packet);
    remaining -= received;
  }
  if (!success || remaining || (computedCrc ^ 0xFFFFFFFFUL) != expectedCrc) {
    if (success && remaining == 0) {
      Serial.printf("OTA CRC mismatch: expected=%lu calculated=%lu\n",
                    static_cast<unsigned long>(expectedCrc),
                    static_cast<unsigned long>(computedCrc ^ 0xFFFFFFFFUL));
    }
    sendAndWait(makePacket(Type::Abort, target, sequence, nullptr, 0));
    http.end();
    return false;
  }
  packet = makePacket(Type::End, target, sequence, reinterpret_cast<const uint8_t *>(&expectedCrc), sizeof(expectedCrc));
  const bool completed = sendAndWait(packet);
  http.end();
  if (!completed) Serial.printf("OTA target %u did not confirm completion\n", target);
  return completed;
}

inline bool performGatewayUpdate(const String &url, uint32_t size,
                                 uint32_t expectedCrc);

inline bool handleMqttCommand(JsonDocument &doc) {
  if (updateInProgress) {
    Serial.println("OTA command ignored: update already in progress");
    return false;
  }
  uint8_t target;
  String url;
  uint32_t size;
  uint32_t crc;
  if (!parseUpdateCommand(doc, target, url, size, crc)) {
    Serial.println("OTA command rejected: invalid fields, URL, size, or CRC");
    return false;
  }
  Serial.printf("OTA MQTT command accepted: target=%u size=%lu url=%s\n", target,
                static_cast<unsigned long>(size), url.c_str());
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  if (target == TERRASMART_OTA_MQTT_DEVICE_ID) {
    const bool result = performGatewayUpdate(url, size, crc);
    if (!result) Serial.println("OTA target 0: failed");
    return result;
  }
#endif
  updateInProgress = true;
  const bool result = performUpdate(target, url, size, crc);
  updateInProgress = false;
  Serial.printf("OTA target %u: %s\n", target, result ? "complete" : "failed");
  return result;
}

inline bool performGatewayUpdate(const String &url, uint32_t size,
                                 uint32_t expectedCrc) {
  if (size == 0 || size > 1200000UL) return false;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  HTTPClient http;
  if (!beginFirmwareDownload(http, url, plainClient, secureClient)) return false;
  const int response = http.GET();
  if (response != HTTP_CODE_OK || http.getSize() != static_cast<int>(size) ||
      !Update.begin(size)) {
    http.end();
    return false;
  }
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[512];
  uint32_t remaining = size;
  uint32_t crcState = 0xFFFFFFFFUL;
  bool success = true;
  while (remaining && success) {
    const size_t wanted = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    size_t received = 0;
    const uint32_t started = millis();
    while (received < wanted && millis() - started < 10000) {
      if (stream->available()) received += stream->readBytes(buffer + received, wanted - received);
      else delay(1);
      client.loop();
      terrasSmartOtaServiceStatusLed();
    }
    if (received != wanted || Update.write(buffer, received) != received) {
      success = false;
      break;
    }
    crcState = crc32Update(crcState, buffer, received);
    remaining -= received;
    client.loop();
  }
  http.end();
  if (!success || remaining || (crcState ^ 0xFFFFFFFFUL) != expectedCrc || !Update.end()) {
    Update.abort();
    return false;
  }
  Serial.println("OTA target 0: complete; restarting");
  delay(250);
  ESP.restart();
  return true;
}

inline void loopMqtt() {}
#endif

} // namespace TerraSmartOta

inline bool terrasSmartOtaCaptureEspNow(const uint8_t *data, size_t length,
                                         const uint8_t *source) {
#if defined(TERRASMART_OTA_NODE) || defined(TERRASMART_OTA_ESPNOW_GATEWAY)
  return TerraSmartOta::captureEspNow(data, length, source);
#else
  (void)data;
  (void)length;
  (void)source;
  return false;
#endif
}

inline void terrasSmartOtaServiceEspNow() {
#if defined(TERRASMART_OTA_NODE) || defined(TERRASMART_OTA_ESPNOW_GATEWAY)
  TerraSmartOta::serviceEspNow();
#endif
}

inline bool terrasSmartOtaUpdateInProgress() {
#if defined(TERRASMART_OTA_NODE) || defined(TERRASMART_OTA_ESPNOW_GATEWAY)
#if defined(TERRASMART_OTA_ESPNOW_GATEWAY)
  if (TerraSmartOta::forwardedUpdateInProgress &&
      millis() - TerraSmartOta::forwardedUpdateLastActivity >=
          TerraSmartOta::FORWARDED_UPDATE_TIMEOUT_MS) {
    TerraSmartOta::forwardedUpdateInProgress = false;
    TerraSmartOta::forwardedTerminalSequenceValid = false;
    TerraSmartOta::forwardedNodeMacValid = false;
    Serial.println("OTA forwarded session timed out; resuming FDRS UART");
  }
  return TerraSmartOta::updaterStarted ||
         TerraSmartOta::forwardedUpdateInProgress;
#else
  return TerraSmartOta::updaterStarted;
#endif
#elif defined(TERRASMART_OTA_MQTT)
  return TerraSmartOta::updateInProgress;
#else
  return false;
#endif
}

inline void terrasSmartOtaServiceUart() {
#if defined(TERRASMART_OTA_ESPNOW_GATEWAY)
  static String line;
  while (Serial1.available()) {
    const char value = static_cast<char>(Serial1.read());
    if (value == '\n') {
      line.trim();
      if (line.startsWith("!TSOTA:")) TerraSmartOta::handleUartLine(line);
      line = "";
    } else if (line.length() < 600) {
      line += value;
    } else {
      line = "";
    }
  }
#elif defined(TERRASMART_OTA_MQTT)
  static String line;
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  Stream &uart = Serial1;
#else
  Stream &uart = Serial;
#endif
  while (uart.available()) {
    const char value = static_cast<char>(uart.read());
    if (value == '\n') {
      line.trim();
      if (line.startsWith("!TSOTA:")) {
        TerraSmartOta::Packet packet;
        if (TerraSmartOta::fromUartLine(line, packet) &&
            packet.type == TerraSmartOta::Type::Ack) {
          TerraSmartOta::uartAck = packet;
          TerraSmartOta::uartAckReady = true;
        }
      }
      line = "";
    } else if (line.length() < 600) {
      line += value;
    } else {
      line = "";
    }
  }
#endif
}

inline bool terrasSmartOtaHandleUartLine(const char *line) {
#if defined(TERRASMART_OTA_ESPNOW_GATEWAY)
  return TerraSmartOta::handleUartLine(String(line));
#elif defined(TERRASMART_OTA_MQTT)
  TerraSmartOta::Packet packet;
  if (!TerraSmartOta::fromUartLine(String(line), packet) ||
      packet.type != TerraSmartOta::Type::Ack) return false;
  TerraSmartOta::uartAck = packet;
  TerraSmartOta::uartAckReady = true;
  return true;
#else
  (void)line;
  return false;
#endif
}

inline bool terrasSmartOtaHandleMqttCommand(const uint8_t *payload,
                                           size_t length) {
#if defined(TERRASMART_OTA_MQTT)
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return false;
  return TerraSmartOta::handleMqttCommand(doc);
#else
  (void)payload;
  (void)length;
  return false;
#endif
}

inline void terrasSmartOtaLoop() {
#if defined(TERRASMART_OTA_MQTT)
  TerraSmartOta::loopMqtt();
#endif
}
