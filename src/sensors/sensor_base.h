#pragma once

#include <Arduino.h>

// =============================================================================
// SENSOR BASE INTERFACE
// =============================================================================

class SensorData
{
public:
    float temperature = 0.0f;
    float humidity = 0.0f;
    float distance = 0.0f;
    bool valid = false;

    SensorData() = default;

    SensorData(float temp, float hum, float dist, bool isValid = true)
        : temperature(temp), humidity(hum), distance(dist), valid(isValid) {}
};

class BaseSensor
{
public:
    virtual ~BaseSensor() = default;

    // Initialize the sensor
    virtual bool begin() = 0;

    // Read sensor data
    virtual SensorData read() = 0;

    // Get sensor name
    virtual const char *getName() const = 0;

    // Check if sensor is ready
    virtual bool isReady() const = 0;
};

// =============================================================================
// SENSOR FACTORY
// =============================================================================

enum SensorType
{
    SENSOR_NONE = 0,
    SENSOR_TEMPERATURE = 1,
    SENSOR_ULTRASONIC = 2,
    SENSOR_HUMIDITY = 3
};

class SensorFactory
{
public:
    static BaseSensor *createSensor(SensorType type);
    static void destroySensor(BaseSensor *sensor);
};