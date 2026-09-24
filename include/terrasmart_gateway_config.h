#pragma once

// This gateway uses address 0x01. Nodes must use the same value as GTWY_MAC.
#define UNIT_MAC 0x01
#define FDRS_ESPNOW_CHANNEL 1
#define ESPNOW_NEIGHBOR_1 0x00
#define ESPNOW_NEIGHBOR_2 0x00
#define LORA_NEIGHBOR_1 0x00
#define LORA_NEIGHBOR_2 0x00

#define USE_ESPNOW
#define FDRS_DEBUG

// This firmware is an ESP-NOW gateway and therefore does not connect to Wi-Fi.
// For a Wi-Fi/MQTT gateway, use a separate device and the MQTT configuration
// shown in README.md. Do not enable USE_WIFI together with USE_ESPNOW here.
// #define WIFI_SSID "your-wifi-name"
// #define WIFI_PASS "your-wifi-password"

#define DST_RULE USDST
#define STD_OFFSET (-3)
#define DST_OFFSET (STD_OFFSET + 1)
#define TIME_PRINTTIME 15

// Print readings received from ESP-NOW to USB serial as FDRS JSON.
#define ESPNOWG_ACT sendSerial();
#define ESPNOW1_ACT sendSerial();
#define ESPNOW2_ACT sendSerial();
#define SERIAL_ACT
#define MQTT_ACT
#define INTERNAL_ACT
#define LORAG_ACT
#define LORA1_ACT
#define LORA2_ACT
