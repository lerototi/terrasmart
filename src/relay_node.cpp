#include <Arduino.h>
#include "terrasmart_relay_config.h"
#include <fdrs_node.h>

namespace {
constexpr uint8_t RELAY_ON_LEVEL = RELAY_ACTIVE_LOW ? LOW : HIGH;
constexpr uint8_t RELAY_OFF_LEVEL = RELAY_ACTIVE_LOW ? HIGH : LOW;
constexpr uint32_t REGISTRATION_RETRY_INTERVAL_MS = 5000;
constexpr uint32_t REGISTRATION_REFRESH_INTERVAL_MS = 30000;

bool relay1State = false;
bool relay2State = false;
bool statusReportPending = false;
bool gatewayRegistered = false;
uint32_t nextRegistrationAttempt = 0;

void fdrsReceiveCallback(DataReading data);

void writeRelay(uint8_t pin, bool enabled) {
  digitalWrite(pin, enabled ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

void printRelayStates() {
  Serial.print("Estado atual - Rele 1: ");
  Serial.print(relay1State ? "LIGADO" : "DESLIGADO");
  Serial.print(" | Rele 2: ");
  Serial.println(relay2State ? "LIGADO" : "DESLIGADO");
}

void queueStatusReport() {
  // Replace an older snapshot when both relay commands arrive in one packet.
  // This keeps the MQTT state report atomic and avoids publishing an
  // intermediate state between the two commands.
  data_count = 0;
  loadFDRS(relay1State ? 1.0f : 0.0f, STATUS_T, RELAY_1_CONTROL_ID);
  loadFDRS(relay2State ? 1.0f : 0.0f, STATUS_T, RELAY_2_CONTROL_ID);
  statusReportPending = true;
}

void registerWithGateway() {
  const bool wasRegistered = gatewayRegistered;
  Serial.println("Registering relay controller with gateway...");
  gatewayRegistered = addFDRS(1000, fdrsReceiveCallback);

  if (gatewayRegistered) {
    nextRegistrationAttempt = millis() + REGISTRATION_REFRESH_INTERVAL_MS;
    Serial.println("Relay controller registered with gateway");
    if (!wasRegistered) {
      queueStatusReport();
    }
  } else {
    nextRegistrationAttempt = millis() + REGISTRATION_RETRY_INTERVAL_MS;
    Serial.println("Gateway unavailable; registration will be retried");
  }
}

void fdrsReceiveCallback(DataReading data) {
  Serial.print("DataReading recebido: id=");
  Serial.print(data.id);
  Serial.print(" type=");
  Serial.print(data.t);
  Serial.print(" data=");
  Serial.println(data.d);

  if (data.t == 0) { // SET command
    const bool enabled = data.d >= 0.5f;

    if (data.id == RELAY_1_CONTROL_ID) {
      relay1State = enabled;
      writeRelay(RELAY_1_PIN, relay1State);
      Serial.print("Comando recebido: Rele 1 -> ");
      Serial.println(relay1State ? "LIGADO" : "DESLIGADO");
      queueStatusReport();
    } else if (data.id == RELAY_2_CONTROL_ID) {
      relay2State = enabled;
      writeRelay(RELAY_2_PIN, relay2State);
      Serial.print("Comando recebido: Rele 2 -> ");
      Serial.println(relay2State ? "LIGADO" : "DESLIGADO");
      queueStatusReport();
    }
  } else if (data.t == 1) { // GET command
    if (data.id == RELAY_1_CONTROL_ID || data.id == RELAY_2_CONTROL_ID) {
      Serial.println("Solicitação de estado recebida");
      printRelayStates();
      queueStatusReport();
    }
  }
}
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("TerraSmart ESP8266 relay node starting");
  Serial.println("Relay 1: D1 (GPIO5), command ID 101");
  Serial.println("Relay 2: D2 (GPIO4), command ID 102");

  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  writeRelay(RELAY_1_PIN, false);
  writeRelay(RELAY_2_PIN, false);
  printRelayStates();

  beginFDRS();
  subscribeFDRS(RELAY_1_CONTROL_ID);
  subscribeFDRS(RELAY_2_CONTROL_ID);
  registerWithGateway();
}

void loop() {
  loopFDRS();

  if (static_cast<int32_t>(millis() - nextRegistrationAttempt) >= 0) {
    registerWithGateway();
  }

  if (statusReportPending) {
    statusReportPending = false;
    Serial.println("Enviando confirmacao de estado dos reles...");
    if (sendFDRS()) {
      Serial.println("Confirmacao de estado enviada ao gateway.");
    } else {
      Serial.println("Falha ao enviar confirmacao; renovando registro com gateway.");
      gatewayRegistered = false;
      nextRegistrationAttempt = millis() + REGISTRATION_RETRY_INTERVAL_MS;
    }
  }
}
