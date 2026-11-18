#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef TERRASMART_PILOT_UART_BAUD
#define TERRASMART_PILOT_UART_BAUD 9600
#endif

// One bounded reader dispatches telemetry, OTA and handshake lines.
namespace terrasSmartPilotUart {
constexpr size_t MAX_LINE = 7 + (9 + 180) * 2 + 2;
constexpr uint32_t TIMEOUT_MS = 2000;
enum class Kind { NONE, TELEMETRY, OTA, PROBE, READY, INVALID };
enum class Error { NONE, OVERFLOW, UNKNOWN_PREFIX };

class Parser {
 public:
  Kind feed(char c, uint32_t now) {
    expire(now);
    if (c == '!') { active_ = true; size_ = 0; }
    if (!active_) return Kind::NONE;
    last_ = now;
    if (size_ >= MAX_LINE) { active_ = false; error_ = Error::OVERFLOW; return Kind::INVALID; }
    buffer_[size_++] = c;
    if (c != '\n') return Kind::NONE;
    buffer_[size_] = 0;
    active_ = false;
    if (!strncmp(buffer_, "!TS1:", 5)) return Kind::TELEMETRY;
    if (!strncmp(buffer_, "!TSOTA:", 7)) return Kind::OTA;
    if (!strncmp(buffer_, "!TSPROBE:", 9)) return Kind::PROBE;
    if (!strncmp(buffer_, "!TSREADY:", 9)) return Kind::READY;
    error_ = Error::UNKNOWN_PREFIX;
    return Kind::INVALID;
  }
  void expire(uint32_t now) { if (active_ && now - last_ >= TIMEOUT_MS) active_ = false; }
  const char *line() const { return buffer_; }
  size_t size() const { return size_; }
  Error error() const { return error_; }
 private:
  char buffer_[MAX_LINE + 1] = {};
  size_t size_ = 0;
  uint32_t last_ = 0;
  bool active_ = false;
  Error error_ = Error::NONE;
};
} // namespace terrasSmartPilotUart
