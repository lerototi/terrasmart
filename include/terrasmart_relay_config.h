#pragma once

#include <fdrs_globals.h>

// Use a unique reading ID for this controller node.
#define READING_ID 4
#define TERRASMART_OTA_DEVICE_ID 0x14
#define TERRASMART_OTA_NODE
#define GTWY_MAC 0x01
#define FDRS_ESPNOW_CHANNEL 1

#define USE_ESPNOW
#define FDRS_DEBUG
#define DBG_LEVEL 2

// Two relay inputs on the ESP8266 NodeMCU.
#define RELAY_1_PIN D1 // GPIO5
#define RELAY_2_PIN D2 // GPIO4

// Most relay modules for NodeMCU are active-low.
// Change to 0 if the module turns on with a HIGH signal.
#define RELAY_ACTIVE_LOW 1

// FDRS command IDs used to control and report each relay.
#define RELAY_1_CONTROL_ID 101
#define RELAY_2_CONTROL_ID 102

#define DST_RULE USDST
#define STD_OFFSET (-3)
#define DST_OFFSET (STD_OFFSET + 1)
#define TIME_PRINTTIME 15
