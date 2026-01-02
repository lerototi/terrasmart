#pragma once

#include <string>

// Mock de WiFi para testes unitários
class WiFiMock {
private:
    static wl_status_t status;
    static std::string ssid;
    static std::string password;

public:
    static void reset() {
        status = WL_DISCONNECTED;
        ssid = "";
        password = "";
    }

    static void setSSID(const std::string& s) {
        ssid = s;
    }

    static void setPassword(const std::string& p) {
        password = p;
    }

    static void setStatus(wl_status_t s) {
        status = s;
    }

    static wl_status_t status_value() {
        return status;
    }

    static std::string getSSID() {
        return ssid;
    }

    static std::string getPassword() {
        return password;
    }
};

// Inicializar valores padrão
wl_status_t WiFiMock::status = WL_DISCONNECTED;
std::string WiFiMock::ssid = "";
std::string WiFiMock::password = "";

// Enums WiFi
enum wl_status_t {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6,
    WL_NO_SHIELD = 255
};
