#include <Arduino.h>
#include "terrasmart_float_switch_config.h"
#include "terrasmart_ota.h"
#include <fdrs_node.h>

namespace {
constexpr uint32_t SEND_INTERVAL_SECONDS = 10;

bool isSwitchActive(uint8_t pin) {
  // INPUT_PULLUP keeps the input HIGH while the switch is open and LOW when
  // the float switch closes the circuit to GND.
  return digitalRead(pin) == LOW;
}
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("TerraSmart float switch node starting");
  Serial.println("Minimum switch: D6 (GPIO12)");
  Serial.println("Maximum switch: D5 (GPIO14)");

  pinMode(BOIA_MIN_PIN, INPUT_PULLUP);
  pinMode(BOIA_MAX_PIN, INPUT_PULLUP);

  beginFDRS();
  Serial.println("Pinging ESP-NOW gateway...");
  pingFDRS(2000);
  Serial.println("Entering float switch loop");
}

void loop() {
  static uint32_t nextSample = 0;
  loopFDRS();
  terrasSmartOtaServiceEspNow();
  if (static_cast<int32_t>(millis() - nextSample) < 0) {
    delay(1);
    return;
  }
  nextSample = millis() + SEND_INTERVAL_SECONDS * 1000UL;
  const bool minimumActive = isSwitchActive(BOIA_MIN_PIN);
  const bool maximumActive = isSwitchActive(BOIA_MAX_PIN);

  // FDRS transports float values, so 1.0 means the switch is closed and
  // 0.0 means it is open.
  loadFDRS(minimumActive ? 1.0f : 0.0f, LEVEL_T, BOIA_MIN_READING_ID);
  loadFDRS(maximumActive ? 1.0f : 0.0f, LEVEL_T, BOIA_MAX_READING_ID);

  Serial.print("Boia minima: ");
  Serial.println(minimumActive ? "acionada" : "aberta");
  Serial.print("Boia maxima: ");
  Serial.println(maximumActive ? "acionada" : "aberta");

  if (sendFDRS()) {
    DBG("Float switch packet sent.");
  } else {
    DBG("Float switch packet failed.");
  }

}
