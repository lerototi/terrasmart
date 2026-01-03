#include "button_handler.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

ButtonHandler::ButtonHandler()
    : buttonPin(-1), pressStartTime(0), longPressDetected(false), initialized(false)
{
}

void ButtonHandler::begin(int pin)
{
    buttonPin = pin;
    pinMode(buttonPin, HIGH);
    initialized = true;

#ifdef ARDUINO
    Serial.printf("[BUTTON] Handler inicializado no pino %d\n", pin);
#endif
}

bool ButtonHandler::isPressed()
{
    if (!initialized)
        return false;
#ifdef ARDUINO
    return digitalRead(buttonPin) == HIGH; // LOW = pressionado
#else
    return false; // Para testes nativos
#endif
}

bool ButtonHandler::checkAndClearLongPress()
{
    if (!initialized)
        return false;

#ifdef ARDUINO
    bool pressed = isPressed();

    // Debug: log estado do botão a cada 2 segundos
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 2000)
    {
        lastDebug = millis();
    }

    if (pressed)
    {
        if (pressStartTime == 0)
        {
            // Início do pressionamento
            pressStartTime = millis();
            longPressDetected = false;
            Serial.printf("[BUTTON] Início do pressionamento detectado (pino %d)\n", buttonPin);
        }
        else
        {
            // Verificar se já passou 5 segundos
            unsigned long pressDuration = millis() - pressStartTime;
            if (!longPressDetected && pressDuration >= LONG_PRESS_DURATION)
            {
                longPressDetected = true;
                Serial.println("\n╔════════════════════════════════════╗");
                Serial.println("║   BOTÃO PRESSIONADO POR 5s        ║");
                Serial.println("║   RESET DE CONFIGURAÇÃO           ║");
                Serial.println("╚════════════════════════════════════╝");
                return true;
            }
            else if (!longPressDetected)
            {
                // Debug: mostrar progresso
                if (pressDuration % 1000 == 0)
                { // A cada segundo
                    Serial.printf("[BUTTON] Pressionado por %lu segundos...\n", pressDuration / 1000);
                }
            }
        }
    }
    else
    {

        if (pressStartTime > 0)
        {
            Serial.printf("[BUTTON] Botão solto após %lu ms\n", millis() - pressStartTime);
        }
        pressStartTime = 0;
        longPressDetected = false;
    }
#endif

    return false;
}