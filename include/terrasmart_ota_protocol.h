#pragma once

#include <Arduino.h>
#include <string.h>

namespace TerraSmartOta {

constexpr uint8_t MAGIC_0 = 0x54; // T
constexpr uint8_t MAGIC_1 = 0x53; // S
constexpr uint8_t VERSION = 1;
constexpr size_t MAX_CHUNK = 180;
constexpr uint32_t UART_TIMEOUT_MS = 5000;
constexpr uint8_t MAX_RETRIES = 5;

enum class Type : uint8_t {
  Begin = 1,
  Data = 2,
  End = 3,
  Ack = 4,
  Abort = 5,
};

// Wire format for ESP-NOW. The header and payload remain below the 250-byte
// ESP-NOW application limit used by the FDRS library.
struct __attribute__((packed)) Packet {
  uint8_t magic[2];
  uint8_t version;
  Type type;
  uint8_t target;
  uint16_t sequence;
  uint16_t size;
  uint8_t payload[MAX_CHUNK];
};

inline bool isPacket(const uint8_t *data, size_t length) {
  return data != nullptr && length >= 9 && data[0] == MAGIC_0 &&
         data[1] == MAGIC_1 && data[2] == VERSION &&
         data[3] >= static_cast<uint8_t>(Type::Begin) &&
         data[3] <= static_cast<uint8_t>(Type::Abort) &&
         (static_cast<size_t>(data[7]) |
          (static_cast<size_t>(data[8]) << 8)) <= MAX_CHUNK &&
         length == 9 + data[7] + (static_cast<size_t>(data[8]) << 8);
}

inline Packet makePacket(Type type, uint8_t target, uint16_t sequence,
                         const uint8_t *payload, uint16_t size) {
  Packet packet{};
  packet.magic[0] = MAGIC_0;
  packet.magic[1] = MAGIC_1;
  packet.version = VERSION;
  packet.type = type;
  packet.target = target;
  packet.sequence = sequence;
  packet.size = size;
  if (payload != nullptr && size <= MAX_CHUNK) {
    memcpy(packet.payload, payload, size);
  }
  return packet;
}

} // namespace TerraSmartOta
