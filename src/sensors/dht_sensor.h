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
    
    // Para detecção de mudança
    float lastSentTemperature;
    float lastSentHumidity;
    bool firstReading;
    
    // Para detecção de sensor travado
    float lastReadTemperature;
    float lastReadHumidity;
    int identicalReadings;
    unsigned long lastSuccessfulRead;
    int reinitializeAttempts; // Contador de tentativas de reinicialização

public:
    DHT22Sensor(uint8_t dhtPin);
    ~DHT22Sensor() override;

    bool begin() override;
    SensorData read() override;
    const char *getName() const override;
    bool isReady() const override;
    
    // Métodos para telemetria por evento
    bool hasSignificantChange(const SensorData &current);
    void updateLastSent(const SensorData &data);
    
    // Método para reinicializar sensor travado
    void reinitialize();
};