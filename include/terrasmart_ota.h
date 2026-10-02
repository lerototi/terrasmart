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
  if (!line.startsWith("!TSOTA:") || ((line.length() - 7) & 1U)) {
    return false;
  }
  uint8_t bytes[9 + MAX_CHUNK];
  const size_t length = (line.length() - 7) / 2;
  if (length > sizeof(bytes)) return false;
  for (size_t i = 0; i < length; ++i) {
    const int high = fromHex(line[7 + i * 2]);
    const int low = fromHex(line[8 + i * 2]);
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

inline void sendEspNow(const uint8_t *address, const Packet &packet) {
  uint8_t bytes[9 + MAX_CHUNK];
  const size_t length = serialize(packet, bytes, sizeof(bytes));
  if (!length) return;
#if defined(ESP8266)
  uint8_t *mutableAddress = const_cast<uint8_t *>(address);
  if (!esp_now_is_peer_exist(mutableAddress)) {
    if (esp_now_add_peer(mutableAddress, ESP_NOW_ROLE_COMBO,
                         FDRS_ESPNOW_CHANNEL, nullptr, 0) != 0) return;
  }
  esp_now_send(mutableAddress, bytes, length);
#elif defined(ESP32)
  if (!esp_now_is_peer_exist(address)) {
    esp_now_peer_info_t peer{};
    peer.ifidx = WIFI_IF_STA;
    peer.channel = FDRS_ESPNOW_CHANNEL;
    memcpy(peer.peer_addr, address, 6);
    if (esp_now_add_peer(&peer) != ESP_OK) return;
  }
  esp_now_send(address, bytes, length);
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
    return;
  }
  if (packet.target != TERRASMART_OTA_GATEWAY_ID) return;
#elif defined(TERRASMART_OTA_NODE)
  if (packet.target != TERRASMART_OTA_DEVICE_ID) return;
#endif

  const uint8_t status = processUpdatePacket(packet);
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
  if (packet.target == TERRASMART_OTA_GATEWAY_ID) {
    const bool isEnd = packet.type == Type::End;
    const uint8_t status = processUpdatePacket(packet);
    Serial1.println(toUartLine(makePacket(Type::Ack, packet.target,
                                          packet.sequence, &status, 1)));
    if (isEnd && status == STATUS_OK) {
      delay(100);
      ESP.restart();
    }
    return true;
  }

  // Device IDs are configured in firmware, while ESP-NOW MACs are assigned by
  // the radio. Broadcast the numbered OTA blocks; only the matching node ID
  // accepts them and its ACK returns unicast to this gateway.
  const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  sendEspNow(broadcast, packet);
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
#else
  Serial.println(line);
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
      if (consumeUartAck(line) && uartAck.target == target &&
          uartAck.sequence == sequence && uartAck.size == 1) {
        return uartAck.payload[0] == STATUS_OK;
      }
    }
    client.loop();
    delay(1);
  }
  return false;
}

inline bool sendAndWait(const Packet &packet) {
  for (uint8_t attempt = 0; attempt < MAX_RETRIES; ++attempt) {
    uartAckReady = false;
    writeUartLine(toUartLine(packet));
    if (waitForAck(packet.target, packet.sequence)) return true;
  }
  return false;
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
  return url.startsWith("https://") && expectedSize > 0 &&
         url.length() <= 240;
}

inline bool performUpdate(uint8_t target, const String &url,
                          uint32_t expectedSize, uint32_t expectedCrc) {
  WiFiClientSecure secureClient;
#if defined(TERRASMART_OTA_ROOT_CA)
  secureClient.setCACert(TERRASMART_OTA_ROOT_CA);
#else
  Serial.println("OTA refused: configure TERRASMART_OTA_ROOT_CA");
  return false;
#endif
  HTTPClient http;
  if (!http.begin(secureClient, url)) return false;
  const int response = http.GET();
  if (response != HTTP_CODE_OK || expectedSize > 700000UL ||
      http.getSize() != static_cast<int>(expectedSize)) {
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
    }
    if (received != wanted) {
      success = false;
      break;
    }
    computedCrc = crc32Update(computedCrc, chunk, received);
    packet = makePacket(Type::Data, target, sequence++, chunk, received);
    success = sendAndWait(packet);
    remaining -= received;
  }
  if (!success || remaining || (computedCrc ^ 0xFFFFFFFFUL) != expectedCrc) {
    sendAndWait(makePacket(Type::Abort, target, sequence, nullptr, 0));
    http.end();
    return false;
  }
  packet = makePacket(Type::End, target, sequence, reinterpret_cast<const uint8_t *>(&expectedCrc), sizeof(expectedCrc));
  const bool completed = sendAndWait(packet);
  http.end();
  return completed;
}

inline bool performGatewayUpdate(const String &url, uint32_t size,
                                 uint32_t expectedCrc);

inline bool handleMqttCommand(JsonDocument &doc) {
  if (updateInProgress) return false;
  uint8_t target;
  String url;
  uint32_t size;
  uint32_t crc;
  if (!parseUpdateCommand(doc, target, url, size, crc)) return false;
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
  if (target == TERRASMART_OTA_MQTT_DEVICE_ID) {
    return performGatewayUpdate(url, size, crc);
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
  WiFiClientSecure secureClient;
#if defined(TERRASMART_OTA_ROOT_CA)
  secureClient.setCACert(TERRASMART_OTA_ROOT_CA);
#else
  Serial.println("OTA refused: configure TERRASMART_OTA_ROOT_CA");
  return false;
#endif
  HTTPClient http;
  if (!http.begin(secureClient, url)) return false;
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
