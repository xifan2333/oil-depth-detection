#ifndef CONFIG_H
#define CONFIG_H

#include <Preferences.h>

// 引脚定义
#define TRIG_PIN 12  // 超声波传感器触发引脚
#define ECHO_PIN 14  // 超声波传感器回声引脚
#define DTU_RX_PIN 16  // ESP32的RX2引脚
#define DTU_TX_PIN 17  // ESP32的TX2引脚

class Config {
public:
    // 构造函数和析构函数
    Config();
    ~Config();

    // 初始化配置
    void begin();
    
    // 保存所有配置到Flash
    void save();
    
    // 重置为默认配置
    void resetToDefault();

    // Getter方法
    float getTankHeight() const { return tankHeight; }
    float getSensorOffset() const { return sensorOffset; }
    int getSampleInterval() const { return sampleInterval; }
    int getSampleCount() const { return sampleCount; }
    int getWindowSize() const { return windowSize; }
    float getStdThreshold() const { return stdThreshold; }
    float getLowLevelAlert() const { return lowLevelAlert; }
    float getHighLevelAlert() const { return highLevelAlert; }
    const char* getDeviceName() const { return deviceName; }
    const char* getWiFiSSID() const { return wifiSSID; }
    const char* getWiFiPassword() const { return wifiPassword; }

    // Setter方法
    void setTankHeight(float value);
    void setSensorOffset(float value);
    void setSampleInterval(int value);
    void setSampleCount(int value);
    void setWindowSize(int value);
    void setStdThreshold(float value);
    void setLowLevelAlert(float value);
    void setHighLevelAlert(float value);
    void setDeviceName(const char* name);
    void setWiFiSSID(const char* ssid);
    void setWiFiPassword(const char* password);

private:
    Preferences preferences;
    
    // 配置存储的命名空间
    static const char* NAMESPACE;
    
    // 配置项的键名
    static const char* KEY_TANK_HEIGHT;
    static const char* KEY_SENSOR_OFFSET;
    static const char* KEY_SAMPLE_INTERVAL;
    static const char* KEY_SAMPLE_COUNT;
    static const char* KEY_WINDOW_SIZE;
    static const char* KEY_STD_THRESHOLD;
    static const char* KEY_LOW_LEVEL_ALERT;
    static const char* KEY_HIGH_LEVEL_ALERT;
    static const char* KEY_DEVICE_NAME;
    static const char* KEY_WIFI_SSID;
    static const char* KEY_WIFI_PASSWORD;

    // 配置项默认值
    static constexpr float DEFAULT_TANK_HEIGHT = 200.0f;
    static constexpr float DEFAULT_SENSOR_OFFSET = 5.0f;
    static constexpr int DEFAULT_SAMPLE_INTERVAL = 2000;
    static constexpr int DEFAULT_SAMPLE_COUNT = 10;
    static constexpr int DEFAULT_WINDOW_SIZE = 5;
    static constexpr float DEFAULT_STD_THRESHOLD = 5.0f;
    static constexpr float DEFAULT_LOW_LEVEL_ALERT = 20.0f;
    static constexpr float DEFAULT_HIGH_LEVEL_ALERT = 150.0f;
    static const char DEFAULT_DEVICE_NAME[];
    static const char DEFAULT_WIFI_SSID[];
    static const char DEFAULT_WIFI_PASSWORD[];

    // 当前配置值
    float tankHeight;
    float sensorOffset;
    int sampleInterval;
    int sampleCount;
    int windowSize;
    float stdThreshold;
    float lowLevelAlert;
    float highLevelAlert;
    char deviceName[32];
    char wifiSSID[32];
    char wifiPassword[64];

    // 从Preferences加载配置
    void loadConfig();
};

#endif // CONFIG_H 