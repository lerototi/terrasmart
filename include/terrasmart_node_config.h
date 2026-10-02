#pragma once

#include <fdrs_globals.h>

// Each node should use its own reading ID. The ESP32 gateway is 0x01.
#define READING_ID 1
#define TERRASMART_OTA_DEVICE_ID 0x11
#define TERRASMART_OTA_NODE
#define GTWY_MAC 0x01
#define FDRS_ESPNOW_CHANNEL 1

#define USE_ESPNOW
#define FDRS_DEBUG
#define DBG_LEVEL 2

// A02YYUW UART interface on the Wemos D1 mini.
// D6 = GPIO12 (sensor TX -> node RX), D7 = GPIO13 (node TX -> sensor RX).
#define A02YYUW_RX_PIN D6
#define A02YYUW_TX_PIN D7
#define A02YYUW_BAUD 9600

#define DST_RULE USDST
#define STD_OFFSET (-3)
#define DST_OFFSET (STD_OFFSET + 1)
#define TIME_PRINTTIME 15

// Deep sleep on ESP8266 requires GPIO16 connected to RST.
// Leave disabled for the first bench test so the serial output stays available.
// #define DEEP_SLEEP
