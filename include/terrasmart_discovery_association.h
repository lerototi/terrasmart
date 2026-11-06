#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace terras_smart_discovery {
namespace association {
constexpr size_t WIRE_SIZE = 18;
constexpr uint8_t VERSION = 1;

struct Record {
  uint8_t mac[6];
  uint8_t channel;
  uint32_t networkId;
};

enum class DecodeResult { OK, INVALID, WRONG_NETWORK, WRONG_CHANNEL };

inline uint32_t crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffffUL;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320UL & (0UL - (crc & 1UL)));
  }
  return ~crc;
}

inline void write32(uint8_t *out, uint32_t value) {
  for (uint8_t i = 0; i < 4; ++i)
    out[i] = static_cast<uint8_t>(value >> (8 * i));
}

inline uint32_t read32(const uint8_t *in) {
  uint32_t value = 0;
  for (uint8_t i = 0; i < 4; ++i)
    value |= static_cast<uint32_t>(in[i]) << (8 * i);
  return value;
}

inline bool validMac(const uint8_t *mac) {
  if (!mac || (mac[0] & 1)) return false;
  uint8_t any = 0, all = 0xff;
  for (uint8_t i = 0; i < 6; ++i) {
    any |= mac[i];
    all &= mac[i];
  }
  return any != 0 && all != 0xff;
}

inline bool encode(const Record &record, uint8_t *out, size_t capacity) {
  if (!out || capacity < WIRE_SIZE || !validMac(record.mac) ||
      record.channel == 0 || record.channel > 14)
    return false;
  out[0] = 'T'; out[1] = 'A'; out[2] = VERSION; out[3] = record.channel;
  write32(out + 4, record.networkId);
  memcpy(out + 8, record.mac, sizeof(record.mac));
  write32(out + 14, crc32(out, 14));
  return true;
}

inline DecodeResult decode(const uint8_t *data, size_t length,
                           uint32_t expectedNetwork, uint8_t expectedChannel,
                           Record &record) {
  if (!data || length != WIRE_SIZE || data[0] != 'T' || data[1] != 'A' ||
      data[2] != VERSION || data[3] == 0 || data[3] > 14 ||
      read32(data + 14) != crc32(data, 14) || !validMac(data + 8))
    return DecodeResult::INVALID;
  const uint32_t networkId = read32(data + 4);
  if (networkId != expectedNetwork) return DecodeResult::WRONG_NETWORK;
  if (data[3] != expectedChannel) return DecodeResult::WRONG_CHANNEL;
  memcpy(record.mac, data + 8, sizeof(record.mac));
  record.channel = data[3];
  record.networkId = networkId;
  return DecodeResult::OK;
}
} // namespace association
} // namespace terras_smart_discovery
