#include <ESP8266WiFi.h>
#include "config.h"
#include "wifi_manager.h"

void setupWiFi() {
  Serial.println("[WIFI] Conectando...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint8_t retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++retry > 30) ESP.restart();
  }

  Serial.println("\n[WIFI] Conectado!");
  Serial.print("[WIFI] IP: ");
  Serial.println(WiFi.localIP());
}
