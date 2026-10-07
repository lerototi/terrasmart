#include <Arduino.h>
#if defined(ESP8266)
#include <SoftwareSerial.h>
#endif
#include "terrasmart_node_config.h"
#include "terrasmart_ota.h"
#include <fdrs_node.h>

namespace {
constexpr uint32_t SEND_INTERVAL_SECONDS = 10;
constexpr uint8_t A02YYUW_HEADER = 0xFF;
constexpr uint32_t SENSOR_READ_TIMEOUT_MS = 100;
constexpr uint8_t DEBUG_FRAME_BYTES = 16;

#if defined(ESP8266)
SoftwareSerial sensorSerial(A02YYUW_RX_PIN, A02YYUW_TX_PIN);
#elif defined(ESP32)
HardwareSerial sensorSerial(1);
#else
HardwareSerial &sensorSerial = Serial1;
#endif

uint8_t debugBytes[DEBUG_FRAME_BYTES];
uint8_t debugByteCount = 0;

void rememberSensorByte(uint8_t value) {
  if (debugByteCount < DEBUG_FRAME_BYTES) {
    debugBytes[debugByteCount++] = value;
  }
}

void printSensorBytes() {
  Serial.print("A02YYUW bytes (hex):");
  if (debugByteCount == 0) {
    Serial.println(" none");
    return;
  }

  for (uint8_t i = 0; i < debugByteCount; ++i) {
    Serial.print(' ');
    if (debugBytes[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(debugBytes[i], HEX);
  }
  Serial.println();
}

bool readSensorByte(uint8_t &value, uint32_t timeoutMs) {
  const uint32_t started = millis();
  while (millis() - started < timeoutMs) {
    if (sensorSerial.available()) {
      value = static_cast<uint8_t>(sensorSerial.read());
      rememberSensorByte(value);
      return true;
    }
    yield();
  }
  return false;
}

bool readDistanceMillimeters(uint16_t &distanceMm) {
  const uint32_t started = millis();
  uint8_t header;
  debugByteCount = 0;

  while (millis() - started < SENSOR_READ_TIMEOUT_MS) {
    if (!readSensorByte(header, 20)) {
      return false;
    }
    if (header != A02YYUW_HEADER) {
      continue;
    }

    uint8_t highByte;
    uint8_t lowByte;
    uint8_t checksum;
    if (!readSensorByte(highByte, 20) ||
        !readSensorByte(lowByte, 20) ||
        !readSensorByte(checksum, 20)) {
      return false;
    }

    const uint8_t expectedChecksum =
        static_cast<uint8_t>(A02YYUW_HEADER + highByte + lowByte);
    if (checksum != expectedChecksum) {
      Serial.print("A02YYUW checksum error, expected 0x");
      Serial.print(expectedChecksum, HEX);
      Serial.print(" received 0x");
      Serial.println(checksum, HEX);
      continue;
    }

    distanceMm = (static_cast<uint16_t>(highByte) << 8) | lowByte;
    return true;
  }

  return false;
}
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("TerraSmart A02YYUW node starting");
  Serial.print("Sensor RX pin: ");
  Serial.println(A02YYUW_RX_PIN);
  Serial.print("Sensor TX pin: ");
  Serial.println(A02YYUW_TX_PIN);
  Serial.print("Sensor baud: ");
  Serial.println(A02YYUW_BAUD);
#if defined(ESP8266)
  sensorSerial.begin(A02YYUW_BAUD);
  sensorSerial.listen();
#else
  sensorSerial.begin(A02YYUW_BAUD, SERIAL_8N1, A02YYUW_RX_PIN, A02YYUW_TX_PIN);
#endif
  beginFDRS();
  Serial.println("A02YYUW serial initialized");
  Serial.println("Pinging ESP-NOW gateway...");
  pingFDRS(2000);
  Serial.println("Entering sensor loop");
}

void loop() {
  static uint32_t nextSensorRead = 0;
  loopFDRS();
  terrasSmartOtaServiceEspNow();
  if (static_cast<int32_t>(millis() - nextSensorRead) >= 0) {
    nextSensorRead = millis() + SEND_INTERVAL_SECONDS * 1000UL;
    uint16_t distanceMm;
    if (readDistanceMillimeters(distanceMm)) {
      printSensorBytes();
      DBG("A02YYUW distance: " + String(distanceMm) + " mm");
      loadFDRS(static_cast<float>(distanceMm), LEVEL_T);

      if (sendFDRS()) {
        DBG("FDRS packet sent.");
      } else {
        DBG("FDRS packet failed.");
      }
    } else {
      printSensorBytes();
      Serial.println("A02YYUW: no valid frame received.");
    }
  }
  delay(1);
}
