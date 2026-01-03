#pragma once
#include <Arduino.h>

struct DeviceConfig
{
  String wifiSsid;
  String wifiPass;
  String mqttHost;
  uint16_t mqttPort;
  String mqttUser;
  String mqttPass;
};

bool loadConfig(DeviceConfig &config);
bool saveConfig(const DeviceConfig &config);
bool hasValidConfig();