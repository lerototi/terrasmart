#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace terrasSmart {

inline bool formatMqttGatewayId(const uint8_t mac[6], char *output,
                                size_t outputSize) {
  if (mac == nullptr || output == nullptr || outputSize < 16) {
    return false;
  }
  const int written = snprintf(output, outputSize,
                               "gw-%02X%02X%02X%02X%02X%02X", mac[0], mac[1],
                               mac[2], mac[3], mac[4], mac[5]);
  return written == 15 && static_cast<size_t>(written) < outputSize;
}

inline bool formatMqttClientId(const uint8_t mac[6], char *output,
                               size_t outputSize) {
  if (mac == nullptr || output == nullptr || outputSize < 16) {
    return false;
  }
  const int written = snprintf(output, outputSize,
                               "ts-%02X%02X%02X%02X%02X%02X", mac[0], mac[1],
                               mac[2], mac[3], mac[4], mac[5]);
  return written == 15 && static_cast<size_t>(written) < outputSize;
}

inline bool formatMqttStatusTopic(const uint8_t mac[6], char *output,
                                  size_t outputSize) {
  if (mac == nullptr || output == nullptr) {
    return false;
  }
  const int written = snprintf(
      output, outputSize,
      "terrasmart/gateways/%02X%02X%02X%02X%02X%02X/status", mac[0], mac[1],
      mac[2], mac[3], mac[4], mac[5]);
  return written > 0 && static_cast<size_t>(written) < outputSize;
}

inline bool formatMqttCommandTopic(const uint8_t mac[6], char *output,
                                   size_t outputSize) {
  if (mac == nullptr || output == nullptr) {
    return false;
  }
  const int written = snprintf(
      output, outputSize,
      "terrasmart/gateways/%02X%02X%02X%02X%02X%02X/command", mac[0], mac[1],
      mac[2], mac[3], mac[4], mac[5]);
  return written > 0 && static_cast<size_t>(written) < outputSize;
}

inline bool formatMqttOfflinePayload(const char *gatewayId, char *output,
                                     size_t outputSize) {
  if (gatewayId == nullptr || output == nullptr) {
    return false;
  }
  const int written = snprintf(
      output, outputSize, "{\"event\":\"offline\",\"gateway_id\":\"%s\"}",
      gatewayId);
  return written > 0 && static_cast<size_t>(written) < outputSize;
}

} // namespace terrasSmart
