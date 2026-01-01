#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "telemetry.h"

unsigned long lastTelemetry = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ESP iniciado ===");

  setupWiFi();
  setupMQTT();
}

void loop() {
  loopMQTT();

  if (millis() - lastTelemetry > TELEMETRY_INTERVAL) {
    lastTelemetry = millis();
    sendTelemetry();
  }
}
