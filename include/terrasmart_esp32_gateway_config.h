#pragma once

// ESP-NOW gateway: radio on this ESP32, dedicated UART toward the MQTT gateway.
#define UNIT_MAC 0x01
#define FDRS_ESPNOW_CHANNEL 1
#define ESPNOW_NEIGHBOR_1 0x00
#define ESPNOW_NEIGHBOR_2 0x00
#define LORA_NEIGHBOR_1 0x00
#define LORA_NEIGHBOR_2 0x00

#define USE_ESPNOW
#define FDRS_DEBUG
#define DBG_LEVEL 2

// Dedicated UART pins for the link to either MQTT gateway implementation.
#define RXD2 16
#define TXD2 17

// Forward ESP-NOW readings to UART and route UART commands back to nodes.
#define ESPNOWG_ACT sendSerial();
#define ESPNOW1_ACT sendSerial();
#define ESPNOW2_ACT sendSerial();
#define SERIAL_ACT sendESPNowPeers();
#define MQTT_ACT
#define INTERNAL_ACT
#define LORAG_ACT
#define LORA1_ACT
#define LORA2_ACT

#define DST_RULE USDST
#define STD_OFFSET (-3)
#define DST_OFFSET (STD_OFFSET + 1)
#define TIME_PRINTTIME 15
