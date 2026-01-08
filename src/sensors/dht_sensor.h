#pragma once

#include "sensor_base.h"
#include <DHT.h>

// =============================================================================
// DHT22 TEMPERATURE AND HUMIDITY SENSOR
// =============================================================================

class DHT22Sensor : public BaseSensor
{
private:
    DHT *dht;
    uint8_t pin;
    bool initialized;

public:
    DHT22Sensor(uint8_t dhtPin);
    ~DHT22Sensor() override;

    bool begin() override;
    SensorData read() override;
    const char *getName() const override;
    bool isReady() const override;
};