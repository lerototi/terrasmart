#pragma once

// OTA command subscription. Restrict publish access to this topic in the MQTT
// broker ACL; commands are accepted only while the gateway is connected.
#define TERRASMART_OTA_TOPIC "terrasmart/ota"

// Stable OTA target identifiers are independent of the FDRS reading IDs.
// Keep these unique across every device behind the same gateway.
#ifndef TERRASMART_OTA_GATEWAY_ID
#define TERRASMART_OTA_GATEWAY_ID 0x01
#endif

#ifndef TERRASMART_OTA_MQTT_DEVICE_ID
#define TERRASMART_OTA_MQTT_DEVICE_ID 0x00
#endif

// SHA-256 of the downloaded firmware is required. HTTPS certificate
// validation can be enabled by setting this to a PEM root CA in the local
// secrets header. Never use an insecure TLS client in a deployment.
#if __has_include("terrasmart_ota_secrets.h")
#include "terrasmart_ota_secrets.h"
#endif

#ifndef TERRASMART_OTA_PASSWORD
#ifndef FDRS_OTA_PASSWORD
#define FDRS_OTA_PASSWORD "change-this-ota-password"
#endif
#define TERRASMART_OTA_PASSWORD FDRS_OTA_PASSWORD
#endif
