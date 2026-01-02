#pragma once

#include <map>
#include <string>

// Mock de LittleFS para testes unitários
class LittleFSMock {
private:
    static std::map<std::string, std::string> files;
    static bool initialized;

public:
    static void reset() {
        files.clear();
        initialized = false;
    }

    static bool begin() {
        initialized = true;
        return true;
    }

    static bool exists(const char* path) {
        return files.find(path) != files.end();
    }

    static void writeFile(const char* path, const std::string& content) {
        files[path] = content;
    }

    static std::string readFile(const char* path) {
        if (files.find(path) != files.end()) {
            return files[path];
        }
        return "";
    }

    static void deleteFile(const char* path) {
        files.erase(path);
    }

    static std::map<std::string, std::string> getFiles() {
        return files;
    }
};

// Inicializar o mapa estático
std::map<std::string, std::string> LittleFSMock::files;
bool LittleFSMock::initialized = false;
