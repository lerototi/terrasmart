#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#pragma once

// =============================================================================
// COMPATIBILIDADE ENTRE PLATAFORMAS ESP8266/ESP32
// =============================================================================

// File System
#ifdef ESP32
#include <SPIFFS.h>
#define FILESYSTEM SPIFFS
#else
#include <LittleFS.h>
#define FILESYSTEM LittleFS
#endif

// WiFi Library
#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#define ESP8266WiFi WiFi
#define ESP8266WebServer WebServer
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

// GPIO Pin mappings (ESP8266 style to ESP32)
#ifdef ESP32
// ESP32 GPIO mappings (equivalentes aos pinos D do ESP8266)
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 18
#define D6 19
#define D7 21
#define D8 3
#define D9 1
#define D10 5
#endif

#endif // PLATFORM_COMPAT_H