#include <Arduino.h>
#if defined(ESP32DEV_GATEWAY_FIRMWARE)
#include "terrasmart_esp32_gateway_config.h"
#else
#include "terrasmart_gateway_config.h"
#endif
#include <fdrs_gateway.h>

void setup() {
  beginFDRS();
}

void loop() {
  loopFDRS();
}
