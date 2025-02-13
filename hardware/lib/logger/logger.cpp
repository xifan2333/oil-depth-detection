#include "logger.h"

void Logger::begin(HardwareSerial& serial, LogLevel level) {
    _serial = &serial;
    _level = level;
}

void Logger::setLogLevel(LogLevel level) {
    _level = level;
}

void Logger::setModule(const char* moduleName) {
    strncpy(_moduleName, moduleName, sizeof(_moduleName) - 1);
    _moduleName[sizeof(_moduleName) - 1] = '\0';
}

void Logger::debug(const String& message) {
    if (_level <= LogLevel::DEBUG) {
        _print("DEBUG", message);
    }
}

void Logger::info(const String& message) {
    if (_level <= LogLevel::INFO) {
        _print("INFO", message);
    }
}

void Logger::warning(const String& message) {
    if (_level <= LogLevel::WARNING) {
        _print("WARN", message);
    }
}

void Logger::error(const String& message) {
    if (_level <= LogLevel::ERROR) {
        _print("ERROR", message);
    }
}

void Logger::_formatTime(char* timeStr, size_t maxLen) {
    time_t now = (millis() / 1000) + _timeOffset;
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    strftime(timeStr, maxLen, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

void Logger::_print(const char* level, const String& message) {
    char timeStr[32];
    _formatTime(timeStr, sizeof(timeStr));
    
    _serial->print(timeStr);
    _serial->print(" - ");
    if (_moduleName[0] != '\0') {
        _serial->print(_moduleName);
        _serial->print(" - ");
    }
    _serial->print(level);
    _serial->print(" - ");
    _serial->println(message);
} 