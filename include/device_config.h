#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#pragma once

// =============================================================================
// CONFIGURAÇÕES POR DISPOSITIVO - DESCOMENTE APENAS UMA OPÇÃO
// =============================================================================

// Dispositivo: Monitor de Temperatura Interna
// #define DEVICE_TYPE_TEMPERATURE_ROOM
// #define DEVICE_NAME "Sala_Estar_Temp"
// #define DEVICE_LOCATION "Sala de Estar"
// #define MQTT_TOPIC_PREFIX "home/sensors/temperature"

// Dispositivo: Monitor de Caixa d'Água
#define DEVICE_TYPE_WATER_TANK
#define DEVICE_NAME "Caixa_Agua_Principal"
#define DEVICE_LOCATION "Reservatorio"
#define MQTT_TOPIC_PREFIX "home/sensors/water_tank"

// Dispositivo: Monitor de Umidade
// #define DEVICE_TYPE_HUMIDITY
// #define DEVICE_NAME "Jardim_Umidade"
// #define DEVICE_LOCATION "Jardim"
// #define MQTT_TOPIC_PREFIX "home/sensors/humidity"

// =============================================================================
// CONFIGURAÇÃO AUTOMÁTICA DO SENSOR BASEADO NO TIPO DE DISPOSITIVO
// =============================================================================

#if defined(DEVICE_TYPE_TEMPERATURE_ROOM)
#define SENSOR_TYPE 1 // TemperatureSensor
#define SENSOR_NAME "Sensor de Temperatura"
#define SENSOR_PINS_CONFIGURED false

#elif defined(DEVICE_TYPE_WATER_TANK)
#define SENSOR_TYPE 2 // UltrasonicSensor
#define SENSOR_NAME "Sensor Ultrassônico A02YYUW"
#define ULTRASONIC_RX_PIN 12  // D6 (GPIO12)
#define ULTRASONIC_TX_PIN 13  // D7 (GPIO13)
#define TANK_HEIGHT_CM 220.0f // Altura do reservatório em cm (ajustado para 220cm)
#define SENSOR_OFFSET_CM 5.0f // Distância do sensor até o topo quando cheio
#define SENSOR_PINS_CONFIGURED true

#elif defined(DEVICE_TYPE_HUMIDITY)
#define SENSOR_TYPE 3 // HumiditySensor
#define SENSOR_NAME "Sensor de Umidade DHT22"
#define DHT_PIN 12 // D6
#define SENSOR_PINS_CONFIGURED true

#else
#error "Defina um tipo de dispositivo em device_config.h"
#endif

// =============================================================================
// CONFIGURAÇÕES GERAIS (comuns a todos os dispositivos)
// =============================================================================

// NOTA: Tópicos MQTT e DEVICE_ID são definidos em config.h
// Este arquivo mantém apenas configurações específicas do sensor

// Intervalos e timers (se não definidos em config.h)
#ifndef TELEMETRY_INTERVAL
#define TELEMETRY_INTERVAL 5000 // 5 segundos para testes
#endif

#ifndef WIFI_RECONNECT_INTERVAL
#define WIFI_RECONNECT_INTERVAL 4000
#endif

#ifndef MQTT_RECONNECT_INTERVAL
#define MQTT_RECONNECT_INTERVAL 4000
#endif

// Configurações de debug
#define DEBUG_SERIAL false
#define DEBUG_WIFI false
#define DEBUG_MQTT true
#define DEBUG_SENSOR false

#endif // DEVICE_CONFIG_H