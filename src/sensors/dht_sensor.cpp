#include "dht_sensor.h"
#include <Arduino.h>

// =============================================================================
// DHT22 SENSOR IMPLEMENTATION
// =============================================================================

DHT22Sensor::DHT22Sensor(uint8_t dhtPin) : pin(dhtPin), initialized(false)
{
    dht = new DHT(dhtPin, DHT22);
}

DHT22Sensor::~DHT22Sensor()
{
    if (dht)
    {
        delete dht;
    }
}

bool DHT22Sensor::begin()
{
    if (initialized)
    {
        return true;
    }

    Serial.printf("[DHT22] Inicializando sensor no pino GPIO%d (D%d)\n", pin,
                  pin == 12 ? 6 : pin); // D6 = GPIO12

    dht->begin();
    delay(2000); // DHT22 precisa de tempo para estabilizar

    // Testar leitura inicial
    float temp = dht->readTemperature();
    float hum = dht->readHumidity();

    if (isnan(temp) || isnan(hum))
    {
        Serial.println("[DHT22] ✗ Falha na inicialização - valores NaN");
        initialized = false;
        return false;
    }

    Serial.printf("[DHT22] ✓ Inicializado - Temp: %.1f°C, Hum: %.1f%%\n", temp, hum);
    initialized = true;
    return true;
}

SensorData DHT22Sensor::read()
{
    if (!initialized)
    {
        Serial.println("[DHT22] ✗ Sensor não inicializado");
        return SensorData(0, 0, 0, false);
    }

    float temperature = dht->readTemperature();
    float humidity = dht->readHumidity();

    // Verificar se as leituras são válidas
    if (isnan(temperature) || isnan(humidity))
    {
        Serial.println("[DHT22] ✗ Erro na leitura - valores NaN");
        return SensorData(0, 0, 0, false);
    }

    // Validar ranges realistas
    if (temperature < -40 || temperature > 80)
    {
        Serial.printf("[DHT22] ✗ Temperatura fora do range: %.1f°C\n", temperature);
        return SensorData(0, 0, 0, false);
    }

    if (humidity < 0 || humidity > 100)
    {
        Serial.printf("[DHT22] ✗ Umidade fora do range: %.1f%%\n", humidity);
        return SensorData(0, 0, 0, false);
    }

    return SensorData(temperature, humidity, 0.0f, true);
}

const char *DHT22Sensor::getName() const
{
    return "DHT22 Temperature & Humidity Sensor";
}

bool DHT22Sensor::isReady() const
{
    return initialized;
}