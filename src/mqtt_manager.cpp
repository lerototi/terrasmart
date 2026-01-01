#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "command_handler.h"

WiFiClient espClient;
PubSubClient mqtt(espClient);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (uint8_t i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  handleCommand(msg);
}

void setupMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
}

void publishStatus(const char* status) {
  mqtt.publish(MQTT_STATUS_TOPIC, status, true);
}

void publishMessage(const char* topic, const char* payload) {
  mqtt.publish(topic, payload, true);
}

void loopMQTT() {
  if (!mqtt.connected()) {
    while (!mqtt.connected()) {
      Serial.print("[MQTT] Conectando... ");

      if (mqtt.connect(
            DEVICE_ID,
            MQTT_USER,
            MQTT_PASS,
            MQTT_STATUS_TOPIC,
            1,
            true,
            "offline"
          )) {

        Serial.println("OK");
        publishStatus("online");
        mqtt.subscribe(MQTT_CMD_TOPIC);

      } else {
        Serial.print("falhou rc=");
        Serial.println(mqtt.state());
        delay(2000);
      }
    }
  }
  mqtt.loop();
}
