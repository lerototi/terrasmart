#include "sensor_base.h"
#include "dht_sensor.h"
#include "a02yyuw_sensor.h"
#include "../../include/device_config.h"

// =============================================================================
// SENSOR FACTORY IMPLEMENTATION
// =============================================================================

BaseSensor *SensorFactory::createSensor(SensorType type)
{
    switch (type)
    {
    case SENSOR_HUMIDITY:
#ifdef DHT_PIN
        return new DHT22Sensor(DHT_PIN);
#else
        Serial.println("[FACTORY] ✗ DHT_PIN não definido para sensor de umidade");
        return nullptr;
#endif

    case SENSOR_ULTRASONIC:
#if defined(ULTRASONIC_RX_PIN) && defined(ULTRASONIC_TX_PIN)
        // A02YYUW: RX=D6 (GPIO12), TX=D7 (GPIO13)
        Serial.printf("[FACTORY] Criando sensor A02YYUW (RX:%d, TX:%d)\n", 
                      ULTRASONIC_RX_PIN, ULTRASONIC_TX_PIN);
        return new A02YYUWSensor(ULTRASONIC_RX_PIN, ULTRASONIC_TX_PIN, 
                                  TANK_HEIGHT_CM, SENSOR_OFFSET_CM);
#else
        Serial.println("[FACTORY] ✗ Pinos do sensor ultrassônico não definidos");
        return nullptr;
#endif

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
