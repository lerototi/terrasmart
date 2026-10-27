#include <Arduino.h>
#if defined(ESP32DEV_GATEWAY_FIRMWARE)
#include "terrasmart_esp32_gateway_config.h"
#else
#include "terrasmart_gateway_config.h"
#endif
#include <fdrs_globals.h>
#include "terrasmart_ota.h"
#include <fdrs_gateway.h>

#if !defined(LED_BUILTIN)
#define LED_BUILTIN 2
#endif

namespace {
constexpr uint8_t STATUS_LED = LED_BUILTIN;
constexpr unsigned long STATUS_LED_BLINK_INTERVAL_MS = 80;
unsigned long lastStatusLedToggle = 0;
bool statusLedOn = false;
bool otaLedActive = false;

void setStatusLed(bool on) {
  // This ESP32 DevKit's built-in LED is active-high.
  digitalWrite(STATUS_LED, on ? HIGH : LOW);
}

void updateStatusLed() {
  const unsigned long now = millis();
  const bool updating = terrasSmartOtaUpdateInProgress();
  if (!updating) {
    otaLedActive = false;
    statusLedOn = false;
    setStatusLed(false);
    return;
  }
  if (!otaLedActive) {
    otaLedActive = true;
    lastStatusLedToggle = now;
    statusLedOn = false;
    setStatusLed(false);
  }
  if (now - lastStatusLedToggle >= STATUS_LED_BLINK_INTERVAL_MS) {
    lastStatusLedToggle = now;
    statusLedOn = !statusLedOn;
    setStatusLed(statusLedOn);
  }
}
}

void setup() {
  pinMode(STATUS_LED, OUTPUT);
  setStatusLed(false);
#if defined(ESP32DEV_GATEWAY_FIRMWARE)
  // OTA UART frames are hex encoded and can reach 387 bytes including CRLF.
  // Enlarge the RX ring before beginFDRS() initializes Serial1.
  Serial1.setRxBufferSize(1024);
#endif
  beginFDRS();
}

void loop() {
  // loopFDRS() also reads Serial1 for regular JSON traffic. During a local
  // firmware write or a forwarded node update, the OTA service must be the
  // sole reader of that UART. ESP-NOW servicing stays active to relay node ACKs.
  if (!terrasSmartOtaUpdateInProgress()) {
    loopFDRS();
  }
  terrasSmartOtaServiceEspNow();
  terrasSmartOtaServiceUart();
  updateStatusLed();
}
