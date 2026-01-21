#pragma once

#include "sensor_base.h"
#include <SoftwareSerial.h>

// =============================================================================
// A02YYUW ULTRASONIC DISTANCE SENSOR (UART)
// =============================================================================

class A02YYUWSensor : public BaseSensor
{
private:
    SoftwareSerial *serial;
    uint8_t rxPin;
    uint8_t txPin;
    bool initialized;
    
    // Configurações do reservatório
    float tankHeight;        // Altura total do reservatório (cm)
    float sensorOffset;      // Distância do sensor até o topo da água quando cheio (cm)
    
    // Para detecção de mudança (telemetria por evento)
    float lastSentDistance;
    float lastSentLevel;
    bool firstReading;
    
    // Para detecção de sensor travado
    float lastReadDistance;
    int identicalReadings;
    unsigned long lastSuccessfulRead;
    int reinitializeAttempts;
    
    // Buffer de leitura
    unsigned char buffer[4];
    
    // Ler distância do sensor via UART
    bool readDistance(float &distance);

public:
    A02YYUWSensor(uint8_t rx, uint8_t tx, float tankHeightCm = 200.0f, float offsetCm = 5.0f);
    ~A02YYUWSensor() override;

    bool begin() override;
    SensorData read() override;
    const char *getName() const override;
    bool isReady() const override;
    
    // Métodos para telemetria por evento
    bool hasSignificantChange(const SensorData &current);
    void updateLastSent(const SensorData &data);
    
    // Configuração do reservatório
    void setTankHeight(float heightCm);
    void setSensorOffset(float offsetCm);
    
    // Calcular nível em percentual
    float calculateLevel(float distance);
    
    // Método para reinicializar sensor travado
    void reinitialize();
};
