#pragma once

#include "sensors/sensor_base.h"

// Tipo de trigger para telemetria
enum TelemetryTrigger
{
    TRIGGER_CHANGE,    // Mudança significativa detectada
    TRIGGER_HEARTBEAT  // Heartbeat forçado (timeout)
};

bool initSensor();
void sendTelemetry(TelemetryTrigger trigger = TRIGGER_CHANGE);
SensorData readSensor();
BaseSensor* getSensor();
