#include "command_handler.h"
#include "mqtt/mqtt_manager.h"
#include "config.h"

void handleCommand(String msg) {
  Serial.print("[CMD] ");
  Serial.println(msg);

  if (msg == "ping") {
    publishMessage(MQTT_RESP_TOPIC, "{\"result\":\"pong\"}");
  }

  if (msg == "reboot") {
    publishMessage(MQTT_RESP_TOPIC, "{\"result\":\"rebooting\"}");
    delay(200);
    ESP.restart();
  }
}
