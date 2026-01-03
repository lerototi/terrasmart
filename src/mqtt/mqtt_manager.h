#pragma once
#include <PubSubClient.h>
#include "config/config_manager.h"

extern PubSubClient mqtt;

void setupMQTT(const DeviceConfig &config);
void loopMQTT();
void publishStatus(const char *status);
void publishMessage(const char *topic, const char *payload);
bool mqttIsConnected();
void testMQTTConnection();