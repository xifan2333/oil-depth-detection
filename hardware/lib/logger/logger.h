#pragma once

#include <Arduino.h>
#include <time.h>

// 日志级别定义
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    NONE
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void begin(HardwareSerial& serial = Serial, LogLevel level = LogLevel::DEBUG);
    void setLogLevel(LogLevel level);
    void setModule(const char* moduleName);
    
    void debug(const String& message);
    void info(const String& message);
    void warning(const String& message);
    void error(const String& message);

    template<typename... Args>
    void debugf(const char* format, Args... args) {
        if (_level <= LogLevel::DEBUG) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), format, args...);
            _print("DEBUG", buffer);
        }
    }

    // 设置RTC时间
    void setTime(time_t timestamp) {
        _timeOffset = timestamp - (millis() / 1000);
    }

private:
    Logger() : _serial(&Serial), _level(LogLevel::DEBUG), _timeOffset(0) {
        _moduleName[0] = '\0';
    }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void _print(const char* level, const String& message);
    void _formatTime(char* timeStr, size_t maxLen);

    HardwareSerial* _serial;
    LogLevel _level;
    char _moduleName[32];
    time_t _timeOffset;  // RTC时间与系统时间的偏移量
};

// 全局宏定义
#define LOGGER Logger::getInstance()
#define LOG_MODULE(name) LOGGER.setModule(name)
#define LOG_D(msg) LOGGER.debug(msg)
#define LOG_I(msg) LOGGER.info(msg)
#define LOG_W(msg) LOGGER.warning(msg)
#define LOG_E(msg) LOGGER.error(msg)
#define LOG_F(...) LOGGER.debugf(__VA_ARGS__) 