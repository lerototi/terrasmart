#include "a02yyuw_sensor.h"
#include "../../include/config.h"
#include <Arduino.h>

// =============================================================================
// A02YYUW SENSOR IMPLEMENTATION
// Sensor ultrasônico à prova d'água com comunicação UART
// Range: 30cm - 450cm
// Protocolo: 9600 baud, 8N1
// =============================================================================

A02YYUWSensor::A02YYUWSensor(uint8_t rx, uint8_t tx, float tankHeightCm, float offsetCm)
    : rxPin(rx), txPin(tx), initialized(false), firstReading(true)
{
    serial = new SoftwareSerial(rxPin, txPin);
    tankHeight = tankHeightCm;
    sensorOffset = offsetCm;
    lastSentDistance = 0.0f;
    lastSentLevel = 0.0f;
    lastReadDistance = 0.0f;
    identicalReadings = 0;
    lastSuccessfulRead = 0;
    reinitializeAttempts = 0;
}

A02YYUWSensor::~A02YYUWSensor()
{
    if (serial)
    {
        serial->end();
        delete serial;
    }
}

bool A02YYUWSensor::begin()
{
    if (initialized)
    {
        return true;
    }

    Serial.printf("[A02YYUW] Inicializando sensor nos pinos RX:GPIO%d (D%d), TX:GPIO%d (D%d)\n",
                  rxPin, rxPin == 12 ? 6 : (rxPin == 13 ? 7 : rxPin),
                  txPin, txPin == 12 ? 6 : (txPin == 13 ? 7 : txPin));

    Serial.printf("[A02YYUW] Configuração: Altura reservatório=%.1fcm, Offset sensor=%.1fcm\n",
                  tankHeight, sensorOffset);

    serial->begin(9600);
    delay(1000); // Dar tempo ao sensor para estabilizar

    // Testar leitura inicial
    float distance;
    int attempts = 0;
    while (attempts < 5)
    {
        if (readDistance(distance))
        {
            float level = calculateLevel(distance);
            Serial.printf("[A02YYUW] ✓ Inicializado - Distância: %.1fcm, Nível: %.1f%%\n",
                          distance, level);
            initialized = true;
            lastReadDistance = distance;
            return true;
        }
        attempts++;
        delay(500);
    }

    Serial.println("[A02YYUW] ✗ Falha na inicialização - sem resposta do sensor");
    initialized = false;
    return false;
}

bool A02YYUWSensor::readDistance(float &distance)
{
    // Limpar buffer serial
    while (serial->available())
    {
        serial->read();
    }

    delay(100); // Aguardar nova leitura do sensor

    // A02YYUW envia 4 bytes: [0xFF] [H_DATA] [L_DATA] [CHECKSUM]
    if (serial->available() >= 4)
    {
        buffer[0] = serial->read();

        // Verificar header (0xFF)
        if (buffer[0] == 0xFF)
        {
            buffer[1] = serial->read();
            buffer[2] = serial->read();
            buffer[3] = serial->read();

            // Verificar checksum: (0xFF + H_DATA + L_DATA) & 0xFF = CHECKSUM
            unsigned char sum = (buffer[0] + buffer[1] + buffer[2]) & 0xFF;

            if (sum == buffer[3])
            {
                // Calcular distância em mm
                int distMm = (buffer[1] << 8) | buffer[2];

                // Converter para cm
                distance = distMm / 10.0f;

                // Validar range (30cm - 450cm)
                if (distance >= 30.0f && distance <= 450.0f)
                {
                    return true;
                }
                else
                {
                    Serial.printf("[A02YYUW] ✗ Distância fora do range: %.1fcm\n", distance);
                    return false;
                }
            }
            else
            {
                Serial.println("[A02YYUW] ✗ Checksum inválido");
                return false;
            }
        }
    }

    return false;
}

float A02YYUWSensor::calculateLevel(float distance)
{
    // Nível (%) = 100 - ((distância - offset) / altura_tanque * 100)
    // Quanto menor a distância, maior o nível

    float waterHeight = (distance - sensorOffset);
    float level = 100.0f - ((waterHeight / tankHeight) * 100.0f);

    // Limitar entre 0% e 100%
    if (level < 0.0f)
        level = 0.0f;
    if (level > 100.0f)
        level = 100.0f;

    return level;
}

SensorData A02YYUWSensor::read()
{
    if (!initialized)
    {
        Serial.println("[A02YYUW] ✗ Sensor não inicializado");
        return SensorData(0, 0, 0, false);
    }

    // Throttling: ler a cada 2 segundos no mínimo
    static unsigned long lastReadTime = 0;
    static SensorData lastValidData = SensorData(0, 0, 0, false);

    unsigned long now = millis();

    if (now - lastReadTime < 2000 && lastValidData.valid)
    {
        return lastValidData;
    }

    yield();

    float distance;
    if (!readDistance(distance))
    {
        Serial.println("[A02YYUW] ✗ Leitura inválida");

        identicalReadings++;
        if (identicalReadings >= 5)
        {
            Serial.println("[A02YYUW] 🔄 Muitas leituras inválidas - Reinicializando...");
            reinitialize();
            identicalReadings = 0;
        }

        return SensorData(0, 0, 0, false);
    }

    lastReadTime = now;

    // === DETECÇÃO DE SENSOR TRAVADO ===
    if (abs(distance - lastReadDistance) < 0.5) // Menos de 0.5cm de diferença
    {
        identicalReadings++;

        if (identicalReadings >= 15) // 30 segundos travado
        {
            Serial.printf("[A02YYUW] ⚠ SENSOR TRAVADO! %d leituras idênticas: %.1fcm\n",
                          identicalReadings, distance);
            Serial.println("[A02YYUW] 🔄 Reinicializando sensor...");

            reinitialize();
            identicalReadings = 0;
            reinitializeAttempts++;

            if (reinitializeAttempts >= 3)
            {
                Serial.println("\n╔═══════════════════════════════════════════════╗");
                Serial.println("║  ⚠️  SENSOR CONTINUA TRAVADO APÓS 3 TENTATIVAS ║");
                Serial.println("║  🔄  REINICIANDO ESP8266 EM 5 SEGUNDOS...     ║");
                Serial.println("╚═══════════════════════════════════════════════╝\n");

                delay(5000);
                ESP.restart();
            }
        }
    }
    else
    {
        identicalReadings = 0;
        reinitializeAttempts = 0;
    }

    lastReadDistance = distance;
    lastSuccessfulRead = now;

    // Calcular nível percentual
    float level = calculateLevel(distance);

    // SensorData: temperature=0, humidity=0, distance=distance
    // Usaremos o campo temperature para armazenar o nível (%)
    lastValidData = SensorData(level, 0.0f, distance, true);
    return lastValidData;
}

const char *A02YYUWSensor::getName() const
{
    return "A02YYUW Ultrasonic Water Level Sensor";
}

bool A02YYUWSensor::isReady() const
{
    return initialized;
}

// =============================================================================
// DETECÇÃO DE MUDANÇA PARA TELEMETRIA POR EVENTO
// =============================================================================

bool A02YYUWSensor::hasSignificantChange(const SensorData &current)
{
    if (!current.valid)
    {
        return false;
    }

    if (firstReading)
    {
        return true;
    }

    // Verificar mudança significativa:
    // - Distância: >= 2cm (variação no nível da água)
    // - Nível: >= 1% (threshold de nível)
    float distDiff = abs(current.distance - lastSentDistance);
    float levelDiff = abs(current.temperature - lastSentLevel); // temperature = level%

    return (distDiff >= 2.0f || levelDiff >= 1.0f);
}

void A02YYUWSensor::updateLastSent(const SensorData &data)
{
    if (data.valid)
    {
        lastSentDistance = data.distance;
        lastSentLevel = data.temperature; // temperature = level%
        firstReading = false;
    }
}

void A02YYUWSensor::setTankHeight(float heightCm)
{
    tankHeight = heightCm;
    Serial.printf("[A02YYUW] Altura do reservatório atualizada: %.1fcm\n", tankHeight);
}

void A02YYUWSensor::setSensorOffset(float offsetCm)
{
    sensorOffset = offsetCm;
    Serial.printf("[A02YYUW] Offset do sensor atualizado: %.1fcm\n", sensorOffset);
}

// =============================================================================
// REINICIALIZAÇÃO DO SENSOR (RECUPERAÇÃO DE TRAVAMENTO)
// =============================================================================

void A02YYUWSensor::reinitialize()
{
    Serial.println("[A02YYUW] ⚙️ Reinicializando sensor...");

    if (serial)
    {
        serial->end();
        delete serial;
        serial = nullptr;
    }

    delay(500);

    serial = new SoftwareSerial(rxPin, txPin);
    serial->begin(9600);
    delay(1000);

    float distance;
    if (readDistance(distance))
    {
        float level = calculateLevel(distance);
        Serial.printf("[A02YYUW] ✓ Sensor reinicializado - Distância: %.1fcm, Nível: %.1f%%\n",
                      distance, level);
        initialized = true;
        identicalReadings = 0;
        lastReadDistance = distance;
    }
    else
    {
        Serial.println("[A02YYUW] ⚠ Reinicialização falhou");
        initialized = false;
    }
}
