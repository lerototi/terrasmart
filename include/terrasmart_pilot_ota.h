#pragma once

#include <stdint.h>
#include <string.h>

namespace terrasSmartPilotOta {
constexpr char TOPIC[] = "terrasmart/pilot/ota";
constexpr uint8_t LOCAL_TARGET = 0;
constexpr uint8_t RADIO_TARGET = 1;

inline bool acceptsTopic(const char *topic) {
  return topic && strcmp(topic, TOPIC) == 0;
}
inline bool acceptsTarget(uint16_t target) { return target == LOCAL_TARGET || target == RADIO_TARGET; }
inline bool acceptsGateway(const char *requested, const char *own) {
  return requested && own && own[0] && strcmp(requested, own) == 0;
}
} // namespace terrasSmartPilotOta
