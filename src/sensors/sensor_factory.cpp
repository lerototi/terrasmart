#include "sensor_base.h"
#include "dht_sensor.h"
#include "../../include/device_config.h"

// =============================================================================
// SENSOR FACTORY IMPLEMENTATION
// =============================================================================

BaseSensor *SensorFactory::createSensor(SensorType type)
{
    switch (type)
    {
    case SENSOR_HUMIDITY:
        return new DHT22Sensor(DHT_PIN);

    case SENSOR_ULTRASONIC:
        // TODO: Implement ultrasonic sensor
        Serial.println("[FACTORY] ✗ Sensor ultrassônico não implementado ainda");
        return nullptr;

    case SENSOR_TEMPERATURE:
        // TODO: Implement temperature-only sensor
        Serial.println("[FACTORY] ✗ Sensor de temperatura não implementado ainda");
        return nullptr;

    default:
        Serial.printf("[FACTORY] ✗ Tipo de sensor desconhecido: %d\n", type);
        return nullptr;
    }
}

void SensorFactory::destroySensor(BaseSensor *sensor)
{
    if (sensor)
    {
        delete sensor;
    }
}
