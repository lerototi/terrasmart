#pragma once

#include <stddef.h>
#include <stdint.h>

// Diagnostic discovery v2. This is separate from the production data envelope.
#ifndef TERRASMART_DISCOVERY_NETWORK_ID
#define TERRASMART_DISCOVERY_NETWORK_ID 0x54530101UL
#endif

namespace terras_smart_discovery {
constexpr size_t WIRE_SIZE = 13;
constexpr uint8_t VERSION = 2;
constexpr uint8_t BEACON = 1;
constexpr uint8_t PROBE = 2;
constexpr uint8_t REPLY = 3;

struct Packet {
  uint8_t type;
  uint8_t channel;
  uint32_t networkId;
  uint32_t sequence;
};

enum class DecodeResult { OK, INVALID, WRONG_NETWORK };

inline void write32(uint8_t *out, uint32_t value) {
  for (uint8_t i = 0; i < 4; ++i) out[i] = static_cast<uint8_t>(value >> (8 * i));
}

inline uint32_t read32(const uint8_t *in) {
  uint32_t value = 0;
  for (uint8_t i = 0; i < 4; ++i) value |= static_cast<uint32_t>(in[i]) << (8 * i);
  return value;
}

inline void encode(const Packet &packet, uint8_t *out) {
  out[0] = 'T'; out[1] = 'S'; out[2] = VERSION;
  out[3] = packet.type; out[4] = packet.channel;
  write32(out + 5, packet.networkId);
  write32(out + 9, packet.sequence);
}

inline DecodeResult decode(const uint8_t *data, size_t length, uint8_t channel,
                           uint32_t networkId, Packet &packet) {
  if (!data || length != WIRE_SIZE || data[0] != 'T' || data[1] != 'S' ||
      data[2] != VERSION || data[3] < BEACON || data[3] > REPLY ||
      data[4] != channel) return DecodeResult::INVALID;
  if (read32(data + 5) != networkId) return DecodeResult::WRONG_NETWORK;
  packet = {data[3], data[4], networkId, read32(data + 9)};
  return DecodeResult::OK;
}
} // namespace terras_smart_discovery
