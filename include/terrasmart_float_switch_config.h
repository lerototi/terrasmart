#pragma once

#include <fdrs_globals.h>

// The gateway uses address 0x01. Keep this value aligned with UNIT_MAC.
#define GTWY_MAC 0x01
#define READING_ID 2
#define FDRS_ESPNOW_CHANNEL 1
#define TERRASMART_OTA_DEVICE_ID 0x12
#define TERRASMART_OTA_NODE

#define USE_ESPNOW
#define FDRS_DEBUG
#define DBG_LEVEL 2

// Float switches are wired between the GPIO and GND.
// D6 = GPIO12: reservoir minimum level switch.
// D5 = GPIO14: reservoir maximum level switch.
#define BOIA_MIN_PIN D6
#define BOIA_MAX_PIN D5

// Separate IDs allow the gateway to distinguish both switches. The existing
// ultrasonic node uses ID 1.
#define BOIA_MIN_READING_ID 2
#define BOIA_MAX_READING_ID 3

#define DST_RULE USDST
#define STD_OFFSET (-3)
#define DST_OFFSET (STD_OFFSET + 1)
#define TIME_PRINTTIME 15

// Deep sleep on ESP8266 requires GPIO16 connected to RST.
// #define DEEP_SLEEP
