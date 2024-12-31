#ifndef DTU_MANAGER_H
#define DTU_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"

class DTUManager {
public:
    DTUManager(HardwareSerial& serial, int rxPin, int txPin, Config& config);
    
    // 初始化DTU
    void begin(unsigned long baudRate = 115200);
    
    // 发送数据到阿里云
    // @param distance: 原始测量距离(cm)
    // @param confidence: 测量置信度(0-1)
    bool sendData(float distance, float confidence);
    
    // 检查DTU状态
    bool checkStatus();
    
    // 处理接收到的数据
    void handleReceive();

private:
    HardwareSerial& dtuSerial;
    int rxPin;
    int txPin;
    bool isInitialized;
    Config& config;
    
    // 内部方法
    void processConfig(JsonObject& configObj);
    String readSerialData();
};

#endif // DTU_MANAGER_H 