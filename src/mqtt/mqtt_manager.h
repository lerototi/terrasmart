#pragma once
#include <PubSubClient.h>

extern PubSubClient mqtt;

void setupMQTT();
void loopMQTT();
void publishStatus(const char* status);
void publishMessage(const char* topic, const char* payload);
bool mqttIsConnected();
void testMQTTConnection();