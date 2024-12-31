#include "Config.h"
#include <string.h>

// 静态成员初始化
const char* Config::NAMESPACE = "oiltank";

// 配置项键名初始化
const char* Config::KEY_TANK_HEIGHT = "tank_height";
const char* Config::KEY_SENSOR_OFFSET = "sensor_offset";
const char* Config::KEY_SAMPLE_INTERVAL = "sample_interval";
const char* Config::KEY_SAMPLE_COUNT = "sample_count";
const char* Config::KEY_WINDOW_SIZE = "window_size";
const char* Config::KEY_STD_THRESHOLD = "std_threshold";
const char* Config::KEY_LOW_LEVEL_ALERT = "low_alert";
const char* Config::KEY_HIGH_LEVEL_ALERT = "high_alert";
const char* Config::KEY_DEVICE_NAME = "device_name";
const char* Config::KEY_WIFI_SSID = "wifi_ssid";
const char* Config::KEY_WIFI_PASSWORD = "wifi_pass";

// 默认值字符串定义
const char Config::DEFAULT_DEVICE_NAME[] = "OilTank001";
const char Config::DEFAULT_WIFI_SSID[] = "OilTankMonitor";
const char Config::DEFAULT_WIFI_PASSWORD[] = "12345678";

Config::Config() {
    // 构造函数中初始化默认值
    resetToDefault();
}

Config::~Config() {
    // 析构函数中确保关闭Preferences
    preferences.end();
}

void Config::begin() {
    Serial.println("开始初始化配置...");
    
    // 打开Preferences
    if (preferences.begin(NAMESPACE, false)) {
        Serial.println("Preferences初始化成功");
        loadConfig();
        
        // 首次运行时保存默认配置
        if (!preferences.isKey(KEY_TANK_HEIGHT)) {
            Serial.println("首次运行，保存默认配置...");
            resetToDefault();
            save();
        }
        
        // 验证关键参数
        if (tankHeight <= 0) {
            Serial.println("警告: 油箱高度无效，重置为默认值");
            tankHeight = DEFAULT_TANK_HEIGHT;
            save();
        }
        
        // 打印当前配置
        Serial.println("\n当前配置：");
        Serial.printf("储油井深度: %.1f cm\n", tankHeight);
        Serial.printf("传感器偏移: %.1f cm\n", sensorOffset);
        Serial.printf("采样间隔: %d ms\n", sampleInterval);
        Serial.printf("采样次数: %d\n", sampleCount);
        Serial.printf("窗口大小: %d\n", windowSize);
        Serial.printf("标准差阈值: %.1f\n", stdThreshold);
        Serial.printf("低油位警告: %.1f%%\n", lowLevelAlert);
        Serial.printf("高油位警告: %.1f%%\n", highLevelAlert);
        Serial.printf("设备名称: %s\n", deviceName);
        Serial.printf("WiFi名称: %s\n", wifiSSID);
        Serial.println("------------------------");
    } else {
        Serial.println("Preferences初始化失败，使用默认配置");
        resetToDefault();
    }
}

void Config::resetToDefault() {
    tankHeight = DEFAULT_TANK_HEIGHT;
    sensorOffset = DEFAULT_SENSOR_OFFSET;
    sampleInterval = DEFAULT_SAMPLE_INTERVAL;
    sampleCount = DEFAULT_SAMPLE_COUNT;
    windowSize = DEFAULT_WINDOW_SIZE;
    stdThreshold = DEFAULT_STD_THRESHOLD;
    lowLevelAlert = DEFAULT_LOW_LEVEL_ALERT;
    highLevelAlert = DEFAULT_HIGH_LEVEL_ALERT;
    strcpy(deviceName, DEFAULT_DEVICE_NAME);
    strcpy(wifiSSID, DEFAULT_WIFI_SSID);
    strcpy(wifiPassword, DEFAULT_WIFI_PASSWORD);
}

void Config::loadConfig() {
    // 从Preferences加载所有配置，如果不存在则使用默认值
    tankHeight = preferences.getFloat(KEY_TANK_HEIGHT, DEFAULT_TANK_HEIGHT);
    sensorOffset = preferences.getFloat(KEY_SENSOR_OFFSET, DEFAULT_SENSOR_OFFSET);
    sampleInterval = preferences.getInt(KEY_SAMPLE_INTERVAL, DEFAULT_SAMPLE_INTERVAL);
    sampleCount = preferences.getInt(KEY_SAMPLE_COUNT, DEFAULT_SAMPLE_COUNT);
    windowSize = preferences.getInt(KEY_WINDOW_SIZE, DEFAULT_WINDOW_SIZE);
    stdThreshold = preferences.getFloat(KEY_STD_THRESHOLD, DEFAULT_STD_THRESHOLD);
    lowLevelAlert = preferences.getFloat(KEY_LOW_LEVEL_ALERT, DEFAULT_LOW_LEVEL_ALERT);
    highLevelAlert = preferences.getFloat(KEY_HIGH_LEVEL_ALERT, DEFAULT_HIGH_LEVEL_ALERT);
    
    // 读取字符串配置，如果不存在则使用默认值
    if (preferences.getString(KEY_DEVICE_NAME, deviceName, sizeof(deviceName)) == 0) {
        strcpy(deviceName, DEFAULT_DEVICE_NAME);
    }
    if (preferences.getString(KEY_WIFI_SSID, wifiSSID, sizeof(wifiSSID)) == 0) {
        strcpy(wifiSSID, DEFAULT_WIFI_SSID);
    }
    if (preferences.getString(KEY_WIFI_PASSWORD, wifiPassword, sizeof(wifiPassword)) == 0) {
        strcpy(wifiPassword, DEFAULT_WIFI_PASSWORD);
    }

    // 打印当前配置
    Serial.println("\n当前配置：");
    Serial.printf("储油井深度: %.1f cm\n", tankHeight);
    Serial.printf("传感器偏移: %.1f cm\n", sensorOffset);
    Serial.printf("采样间隔: %d ms\n", sampleInterval);
    Serial.printf("采样次数: %d\n", sampleCount);
    Serial.printf("窗口大小: %d\n", windowSize);
    Serial.printf("标准差阈值: %.1f\n", stdThreshold);
    Serial.printf("低油位警告: %.1f%%\n", lowLevelAlert);
    Serial.printf("高油位警告: %.1f%%\n", highLevelAlert);
    Serial.printf("设备名称: %s\n", deviceName);
    Serial.printf("WiFi名称: %s\n", wifiSSID);
    Serial.println("------------------------");
}

void Config::save() {
    // 保存所有配置到Preferences
    preferences.putFloat(KEY_TANK_HEIGHT, tankHeight);
    preferences.putFloat(KEY_SENSOR_OFFSET, sensorOffset);
    preferences.putInt(KEY_SAMPLE_INTERVAL, sampleInterval);
    preferences.putInt(KEY_SAMPLE_COUNT, sampleCount);
    preferences.putInt(KEY_WINDOW_SIZE, windowSize);
    preferences.putFloat(KEY_STD_THRESHOLD, stdThreshold);
    preferences.putFloat(KEY_LOW_LEVEL_ALERT, lowLevelAlert);
    preferences.putFloat(KEY_HIGH_LEVEL_ALERT, highLevelAlert);
    preferences.putString(KEY_DEVICE_NAME, deviceName);
    preferences.putString(KEY_WIFI_SSID, wifiSSID);
    preferences.putString(KEY_WIFI_PASSWORD, wifiPassword);
    
    Serial.println("配置已保存");
}

// Setter方法实现
void Config::setTankHeight(float value) {
    if (value > 0) {
        tankHeight = value;
    }
}

void Config::setSensorOffset(float value) {
    if (value >= 0) {
        sensorOffset = value;
    }
}

void Config::setSampleInterval(int value) {
    if (value >= 100) {  // 最小100ms
        sampleInterval = value;
    }
}

void Config::setSampleCount(int value) {
    if (value >= 1 && value <= 30) {  // 增加最大采样次数限制
        sampleCount = value;
    }
}

void Config::setWindowSize(int value) {
    if (value >= 1 && value <= 50) {
        windowSize = value;
    }
}

void Config::setStdThreshold(float value) {
    if (value > 0) {
        stdThreshold = value;
    }
}

void Config::setLowLevelAlert(float value) {
    if (value >= 0 && value < highLevelAlert) {
        lowLevelAlert = value;
    }
}

void Config::setHighLevelAlert(float value) {
    if (value > lowLevelAlert && value <= 100) {
        highLevelAlert = value;
    }
}

void Config::setDeviceName(const char* name) {
    if (name && strlen(name) < sizeof(deviceName)) {
        strcpy(deviceName, name);
    }
}

void Config::setWiFiSSID(const char* ssid) {
    if (ssid && strlen(ssid) < sizeof(wifiSSID)) {
        strcpy(wifiSSID, ssid);
    }
}

void Config::setWiFiPassword(const char* password) {
    if (password && strlen(password) < sizeof(wifiPassword)) {
        strcpy(wifiPassword, password);
    }
} 