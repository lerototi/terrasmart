#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif

class ButtonHandler
{
public:
    ButtonHandler();
    void begin(int pin);
    bool checkAndClearLongPress();
    bool isPressed();

private:
    int buttonPin;
    unsigned long pressStartTime;
    bool longPressDetected;
    bool initialized;

    static const unsigned long LONG_PRESS_DURATION = 5000; // 5 segundos
};