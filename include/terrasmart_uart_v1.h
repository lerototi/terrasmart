#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Dedicated pilot UART only. The printable, newline-delimited frame never
// consumes binary FDRS/OTA traffic and must not share an unmodified parser.
namespace terras_smart_uart_v1 {
constexpr size_t PAYLOAD_SIZE = 43; // radio gateway MAC + 37-byte data envelope
constexpr size_t LINE_SIZE = 5 + PAYLOAD_SIZE * 2 + 8 + 1; // !TS1: + hex + CRC + LF
constexpr uint32_t TIMEOUT_MS = 1000;
constexpr char PREFIX[] = "!TS1:";

inline uint32_t crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffffUL;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320UL & (0UL - (crc & 1UL)));
  }
  return ~crc;
}
inline int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
inline bool encode(const uint8_t *payload, char *out, size_t capacity) {
  if (!payload || !out || capacity < LINE_SIZE + 1) return false;
  static const char hex[] = "0123456789ABCDEF";
  memcpy(out, PREFIX, 5);
  for (size_t i = 0; i < PAYLOAD_SIZE; ++i) {
    out[5 + 2*i] = hex[payload[i] >> 4];
    out[6 + 2*i] = hex[payload[i] & 15];
  }
  uint32_t crc = crc32(payload, PAYLOAD_SIZE);
  for (size_t i = 0; i < 8; ++i) out[5 + 2*PAYLOAD_SIZE + i] = hex[(crc >> (28 - 4*i)) & 15];
  out[LINE_SIZE - 1] = '\n'; out[LINE_SIZE] = 0;
  return true;
}
inline bool decode(const char *line, size_t length, uint8_t *out) {
  if (!line || !out || length != LINE_SIZE || line[length-1] != '\n' || memcmp(line, PREFIX, 5)) return false;
  uint8_t payload[PAYLOAD_SIZE];
  for (size_t i = 0; i < PAYLOAD_SIZE; ++i) {
    int a = nibble(line[5 + 2*i]), b = nibble(line[6 + 2*i]);
    if (a < 0 || b < 0) return false;
    payload[i] = static_cast<uint8_t>((a << 4) | b);
  }
  uint32_t expected = 0;
  for (size_t i = 0; i < 8; ++i) {
    int n = nibble(line[5 + 2*PAYLOAD_SIZE + i]);
    if (n < 0) return false;
    expected = (expected << 4) | static_cast<uint32_t>(n);
  }
  if (crc32(payload, sizeof(payload)) != expected) return false;
  memcpy(out, payload, sizeof(payload));
  return true;
}

class Parser {
 public:
  enum class Result { NONE, FRAME, REJECTED };
  Result feedResult(char c, uint32_t now, uint8_t *payload) {
    expire(now);
    // '!' cannot occur in hex payload, so a new marker always resynchronizes.
    if (c == '!') { size_ = 0; active_ = true; }
    if (!active_) return Result::NONE;
    lastByte_ = now;
    if (size_ >= LINE_SIZE) { reset(); return Result::REJECTED; }
    buffer_[size_++] = c;
    if (c != '\n') return Result::NONE;
    bool valid = decode(buffer_, size_, payload);
    reset();
    return valid ? Result::FRAME : Result::REJECTED;
  }
  bool feed(char c, uint32_t now, uint8_t *payload) {
    return feedResult(c, now, payload) == Result::FRAME;
  }
  void expire(uint32_t now) { if (active_ && now - lastByte_ >= TIMEOUT_MS) reset(); }
 private:
  void reset() { active_ = false; size_ = 0; }
  char buffer_[LINE_SIZE] = {};
  size_t size_ = 0;
  uint32_t lastByte_ = 0;
  bool active_ = false;
};
} // namespace terras_smart_uart_v1
