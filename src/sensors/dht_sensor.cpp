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

    // Throttling: DHT22 não deve ser lido mais que 1x a cada 2 segundos
    static unsigned long lastReadTime = 0;
    static SensorData lastValidData = SensorData(0, 0, 0, false);

    unsigned long now = millis();
    if (now - lastReadTime < 2000 && lastValidData.valid)
    {
        // Retornar última leitura válida (cache)
        return lastValidData;
    }

    yield(); // Dar tempo ao watchdog

    float temperature = dht->readTemperature();
    yield(); // Dar tempo ao watchdog

    float humidity = dht->readHumidity();
    yield(); // Dar tempo ao watchdog

    lastReadTime = now;

    // Verificar se as leituras são válidas
    if (isnan(temperature) || isnan(humidity))
    {
        Serial.println("[DHT22] ✗ Leitura inválida");
        return SensorData(0, 0, 0, false);
    }

    // Validar ranges realistas
    if (temperature < -40 || temperature > 80)
    {
        Serial.printf("[DHT22] ✗ Temp fora do range: %.1f°C\n", temperature);
        return SensorData(0, 0, 0, false);
    }

    if (humidity < 0 || humidity > 100)
    {
        Serial.printf("[DHT22] ✗ Umidade fora do range: %.1f%%\n", humidity);
        return SensorData(0, 0, 0, false);
    }

    lastValidData = SensorData(temperature, humidity, 0.0f, true);
    return lastValidData;
}

const char *DHT22Sensor::getName() const
{
    return "DHT22 Temperature & Humidity Sensor";
}

bool DHT22Sensor::isReady() const
{
    return initialized;
}