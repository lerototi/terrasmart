#pragma once

// OTA command subscription. Restrict publish access to this topic in the MQTT
// broker ACL; commands are accepted only while the gateway is connected.
#define TERRASMART_OTA_TOPIC "terrasmart/ota"
#define TERRASMART_OTA_RELEASE_GATEWAY_ID ""
#define TERRASMART_GATEWAY_COMMAND_TOPIC "terrasmart/gateways/+/command"

// Maximum forwarded image sizes. The ESP-NOW gateway uses a dedicated OTA
// partition slot; other forwarded targets retain the conservative limit.
#define TERRASMART_OTA_FORWARD_LIMIT 700000UL
#define TERRASMART_OTA_GATEWAY_FORWARD_LIMIT 0x1E0000UL

// Stable OTA target identifiers are independent of the FDRS reading IDs.
// Keep these unique across every device behind the same gateway.
#ifndef TERRASMART_OTA_GATEWAY_ID
#define TERRASMART_OTA_GATEWAY_ID 0x01
#endif

#ifndef TERRASMART_OTA_MQTT_DEVICE_ID
#define TERRASMART_OTA_MQTT_DEVICE_ID 0x00
#endif

#ifndef TERRASMART_OTA_MQTT_TARGET_ID
#define TERRASMART_OTA_MQTT_TARGET_ID 0x00
#endif

#ifndef TERRASMART_OTA_ESPNOW_TARGET_ID
#define TERRASMART_OTA_ESPNOW_TARGET_ID 0x01
#endif

// Shared deployment secrets are preferred. Legacy OTA secrets remain a
// fallback during migration from the former per-subsystem files.
#if __has_include("terrasmart_secrets.h")
#include "terrasmart_secrets.h"
#elif __has_include("terrasmart_ota_secrets.h")
#include "terrasmart_ota_secrets.h"
#endif

#ifndef TERRASMART_OTA_PASSWORD
#ifndef FDRS_OTA_PASSWORD
#ifdef ESP8266
#define FDRS_OTA_PASSWORD "replace-this-in-deployment"
#else
#define FDRS_OTA_PASSWORD "change-this-ota-password"
#endif
#endif
#define TERRASMART_OTA_PASSWORD FDRS_OTA_PASSWORD
#endif
