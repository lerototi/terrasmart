#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits>

namespace terras_smart_data_v1 {
constexpr size_t HEADER_SIZE = 30;
constexpr size_t READING_SIZE = 7;
constexpr size_t FRAME_SIZE = HEADER_SIZE + READING_SIZE;
constexpr size_t MAX_FRAME_SIZE = 250;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
              "Data v1 requires IEEE 754 binary32 floats");

struct Reading {
  uint16_t id;
  uint8_t type;
  float data;
};
struct Frame {
  uint32_t networkId;
  uint8_t source[6];
  uint8_t destination[6];
  uint32_t sequence;
  uint32_t bootId;
  uint8_t hopLimit;
  Reading reading;
};

inline void write16(uint8_t *p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8);
}
inline uint16_t read16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0] | static_cast<uint16_t>(p[1]) << 8);
}
inline void write32(uint8_t *p, uint32_t v) {
  for (uint8_t i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
inline uint32_t read32(const uint8_t *p) {
  uint32_t v = 0;
  for (uint8_t i = 0; i < 4; ++i) v |= static_cast<uint32_t>(p[i]) << (8 * i);
  return v;
}
inline bool encode(const Frame &f, uint8_t *out, size_t capacity) {
  if (!out || capacity < FRAME_SIZE || f.hopLimit != 1) return false;
  out[0] = 'T'; out[1] = 'S'; out[2] = 1; out[3] = 1;
  write32(out + 4, f.networkId);
  memcpy(out + 8, f.source, 6); memcpy(out + 14, f.destination, 6);
  write32(out + 20, f.sequence); write32(out + 24, f.bootId);
  out[28] = f.hopLimit; out[29] = READING_SIZE;
  write16(out + 30, f.reading.id); out[32] = f.reading.type;
  uint32_t bits; memcpy(&bits, &f.reading.data, sizeof(bits));
  write32(out + 33, bits);
  return true;
}
inline bool decode(const uint8_t *wire, size_t length, uint32_t networkId,
                   const uint8_t *destination, Frame &f) {
  if (!wire || !destination || length != FRAME_SIZE || length > MAX_FRAME_SIZE ||
      wire[0] != 'T' || wire[1] != 'S' || wire[2] != 1 || wire[3] != 1 ||
      read32(wire + 4) != networkId || memcmp(wire + 14, destination, 6) != 0 ||
      wire[28] != 1 || wire[29] != READING_SIZE) return false;
  Frame decoded = {};
  decoded.networkId = networkId;
  memcpy(decoded.source, wire + 8, 6); memcpy(decoded.destination, wire + 14, 6);
  decoded.sequence = read32(wire + 20); decoded.bootId = read32(wire + 24);
  decoded.hopLimit = wire[28]; decoded.reading.id = read16(wire + 30);
  decoded.reading.type = wire[32];
  uint32_t bits = read32(wire + 33);
  memcpy(&decoded.reading.data, &bits, sizeof(bits));
  f = decoded;
  return true;
}
} // namespace terras_smart_data_v1
