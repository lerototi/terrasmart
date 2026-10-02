#pragma once
#include "terrasmart_ota_config.h"
#if defined(ESP32_MQTT_GATEWAY_FIRMWARE)
#ifndef TERRASMART_OTA_MQTT
#define TERRASMART_OTA_MQTT
#endif
#endif
#if defined(ESP8266) && !defined(FDRS_OTA_PASSWORD)
#define FDRS_OTA_PASSWORD TERRASMART_OTA_PASSWORD
#endif

// Local credentials belong in the ignored secrets header.
// The fallback values keep a clean clone compilable until that file is created.
#if __has_include("terrasmart_mqtt_gateway_secrets.h")
#include "terrasmart_mqtt_gateway_secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASS ""
#define MQTT_ADDR "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_AUTH
#define MQTT_USER "mqtt-user"
#define MQTT_PASS ""
#define FDRS_AP_PASSWORD "configure-me"
#endif

// This device is the Wi-Fi/MQTT front-end. It must not enable USE_ESPNOW.
#define UNIT_MAC 0x00
#define ESPNOW_NEIGHBOR_1 0x00
#define ESPNOW_NEIGHBOR_2 0x00
#define LORA_NEIGHBOR_1 0x00
#define LORA_NEIGHBOR_2 0x00

#define USE_WIFI
#define FDRS_DEBUG
#define DBG_LEVEL 2

// On ESP32, these pins connect a dedicated UART to the ESP-NOW gateway and
// the USB serial monitor remains available on the default Serial port.
// On ESP8266, the environment uses the main Serial pins instead.
#define RXD2 16
#define TXD2 17

#define TOPIC_DATA "terrasmart/data"
#define TOPIC_STATUS "terrasmart/status"
#define TOPIC_COMMAND "terrasmart/command"

// UART JSON from the ESP-NOW gateway goes to MQTT.
#define SERIAL_ACT sendMQTT();

// MQTT commands go back to the ESP-NOW gateway through UART.
#define MQTT_ACT sendSerial();

#define ESPNOWG_ACT
#define ESPNOW1_ACT
#define ESPNOW2_ACT
#define LORAG_ACT
#define LORA1_ACT
#define LORA2_ACT
#define INTERNAL_ACT

#define DST_RULE USDST
#define STD_OFFSET (-3)
#define DST_OFFSET STD_OFFSET
#define FDRS_NO_DST
#define TIME_PRINTTIME 15

#if defined(FDRS_RUNTIME_CONFIG)
// The ESP32 MQTT gateway loads these values from Preferences at runtime.
extern const char *fdrs_runtime_ap_ssid;
extern const char *fdrs_runtime_ap_password;
#endif
