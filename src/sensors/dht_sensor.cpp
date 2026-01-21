#include "dht_sensor.h"
#include "../../include/config.h"
#include <Arduino.h>

// =============================================================================
// DHT22 SENSOR IMPLEMENTATION
// =============================================================================

DHT22Sensor::DHT22Sensor(uint8_t dhtPin) : pin(dhtPin), initialized(false), firstReading(true)
{
    dht = new DHT(dhtPin, DHT22);
    lastSentTemperature = 0.0f;
    lastSentHumidity = 0.0f;
    lastReadTemperature = 0.0f;
    lastReadHumidity = 0.0f;
    identicalReadings = 0;
    lastSuccessfulRead = 0;
    reinitializeAttempts = 0;
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
    
    // IMPORTANTE: Apenas retornar cache se passou MENOS de 2 segundos
    // Se passou 2s ou mais, SEMPRE fazer nova leitura
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
        Serial.println("[DHT22] ✗ Leitura inválida (NaN)");
        
        // Se muitas leituras NaN, reinicializar
        identicalReadings++;
        if (identicalReadings >= 5)
        {
            Serial.println("[DHT22] 🔄 Muitas leituras NaN - Reinicializando...");
            reinitialize();
            identicalReadings = 0;
        }
        
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

    // === DETECÇÃO DE SENSOR TRAVADO ===
    // Verificar se os valores são EXATAMENTE iguais às últimas leituras
    if (abs(temperature - lastReadTemperature) < 0.01 && 
        abs(humidity - lastReadHumidity) < 0.01)
    {
        identicalReadings++;
        
        // Se 15 leituras idênticas (30 segundos), sensor pode estar travado
        if (identicalReadings >= 15)
        {
            Serial.printf("[DHT22] ⚠ SENSOR TRAVADO! %d leituras idênticas: %.1f°C / %.1f%%\n", 
                          identicalReadings, temperature, humidity);
            Serial.println("[DHT22] 🔄 Reinicializando sensor...");
            
            reinitialize();
            identicalReadings = 0;
            reinitializeAttempts++;
            
            // Se já tentou reinicializar 3 vezes e continua travado, resetar ESP
            if (reinitializeAttempts >= 3)
            {
                Serial.println("\n╔═══════════════════════════════════════════════╗");
                Serial.println("║  ⚠️  SENSOR CONTINUA TRAVADO APÓS 3 TENTATIVAS ║");
                Serial.println("║  🔄  REINICIANDO ESP8266 EM 5 SEGUNDOS...     ║");
                Serial.println("╚═══════════════════════════════════════════════╝\n");
                
                delay(5000); // Dar tempo para ver a mensagem
                ESP.restart(); // Reiniciar ESP8266 completamente
            }
        }
    }
    else
    {
        // Valores mudaram, resetar contadores
        identicalReadings = 0;
        reinitializeAttempts = 0; // Reset também o contador de reinicializações
    }
    
    // Atualizar últimas leituras
    lastReadTemperature = temperature;
    lastReadHumidity = humidity;
    lastSuccessfulRead = now;

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

// =============================================================================
// DETECÇÃO DE MUDANÇA PARA TELEMETRIA POR EVENTO
// =============================================================================

bool DHT22Sensor::hasSignificantChange(const SensorData &current)
{
    if (!current.valid)
    {
        return false;
    }
    
    // Primeira leitura sempre envia
    if (firstReading)
    {
        return true;
    }
    
    // Verificar se houve mudança significativa
    float tempDiff = abs(current.temperature - lastSentTemperature);
    float humDiff = abs(current.humidity - lastSentHumidity);
    
    return (tempDiff >= TEMP_CHANGE_THRESHOLD || humDiff >= HUMIDITY_CHANGE_THRESHOLD);
}

void DHT22Sensor::updateLastSent(const SensorData &data)
{
    if (data.valid)
    {
        lastSentTemperature = data.temperature;
        lastSentHumidity = data.humidity;
        firstReading = false;
    }
}

// =============================================================================
// REINICIALIZAÇÃO DO SENSOR (RECUPERAÇÃO DE TRAVAMENTO)
// =============================================================================

void DHT22Sensor::reinitialize()
{
    Serial.println("[DHT22] ⚙️ Reinicializando sensor...");
    
    // Destruir instância atual
    if (dht)
    {
        delete dht;
        dht = nullptr;
    }
    
    delay(500); // Dar tempo ao sensor para resetar
    
    // Recriar instância
    dht = new DHT(pin, DHT22);
    dht->begin();
    delay(2000); // DHT22 precisa de tempo para estabilizar
    
    // Testar leitura
    float temp = dht->readTemperature();
    float hum = dht->readHumidity();
    
    if (isnan(temp) || isnan(hum))
    {
        Serial.println("[DHT22] ⚠ Reinicialização falhou");
        initialized = false;
    }
    else
    {
        Serial.printf("[DHT22] ✓ Sensor reinicializado - Temp: %.1f°C, Hum: %.1f%%\n", temp, hum);
        initialized = true;
        identicalReadings = 0;
        lastReadTemperature = temp;
        lastReadHumidity = hum;
    }
}