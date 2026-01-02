#pragma once
#include <Arduino.h>

struct DeviceConfig {
  String wifiSsid;
  String wifiPass;
};

bool loadConfig(DeviceConfig &config);
bool saveConfig(const DeviceConfig &config);
bool hasValidConfig();