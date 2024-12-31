#include "DTUManager.h"

DTUManager::DTUManager(HardwareSerial& serial, int rxPin, int txPin, Config& config)
    : dtuSerial(serial), rxPin(rxPin), txPin(txPin), isInitialized(false), config(config) {
}

void DTUManager::begin(unsigned long baudRate) {
    // 初始化串口
    dtuSerial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
    isInitialized = true;
}

bool DTUManager::sendData(float distance, float confidence) {
    if (!isInitialized) {
        return false;
    }
    
    // 获取配置值并检查有效性
    float tankHeight = config.getTankHeight();
    float sensorOffset = config.getSensorOffset();
    
    // 如果关键参数无效，打印调试信息
    if (tankHeight <= 0) {
        Serial.println("警告: 油箱高度无效 (0)");
        Serial.printf("当前配置值:\n");
        Serial.printf("储油井深度: %.1f cm\n", config.getTankHeight());
        Serial.printf("传感器偏移: %.1f cm\n", config.getSensorOffset());
        return false;
    }
    
    // 计算油位
    float oilLevel = tankHeight - (distance - sensorOffset);
    
    // 构建JSON数据
    StaticJsonDocument<200> doc;
    doc["distance"] = distance;           // 原始测量距离
    doc["sensorOffset"] = sensorOffset;   // 传感器偏移量
    doc["oilLevel"] = oilLevel;          // 计算得到的油位
    doc["confidence"] = confidence;       // 测量置信度
    doc["timestamp"] = millis();         // 时间戳
    doc["tankHeight"] = tankHeight;      // 油箱高度
    doc["lowLevelAlert"] = config.getLowLevelAlert();    // 低油位警告
    doc["highLevelAlert"] = config.getHighLevelAlert();  // 高油位警告
    
    // 序列化并发送
    String jsonString;
    serializeJson(doc, jsonString);
    dtuSerial.println(jsonString);
    
    // 打印发送的数据
    Serial.print("DTU发送数据: ");
    Serial.println(jsonString);
    
    return true;
}

bool DTUManager::checkStatus() {
    return isInitialized && dtuSerial;
}

void DTUManager::handleReceive() {
    if (!isInitialized) return;
    
    if (dtuSerial.available()) {
        String data = readSerialData();
        if (data.length() > 0) {
            // 解析JSON数据
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, data);
            
            if (!error) {
                // 检查是否包含配置对象
                JsonObject configObj = doc["config"];
                if (!configObj.isNull()) {
                    processConfig(configObj);
                }
            }
        }
    }
}

String DTUManager::readSerialData() {
    String data = "";
    // 等待接收完整的数据
    unsigned long startTime = millis();
    while (millis() - startTime < 1000) {  // 1秒超时
        if (dtuSerial.available()) {
            char c = dtuSerial.read();
            data += c;
            startTime = millis();  // 重置超时计时器
        }
        yield();  // 允许其他任务执行
    }
    return data;
}

void DTUManager::processConfig(JsonObject& configObj) {
    // 更新配置
    if (configObj.containsKey("tankHeight")) {
        config.setTankHeight(configObj["tankHeight"]);
    }
    if (configObj.containsKey("sensorOffset")) {
        config.setSensorOffset(configObj["sensorOffset"]);
    }
    if (configObj.containsKey("sampleInterval")) {
        config.setSampleInterval(configObj["sampleInterval"]);
    }
    if (configObj.containsKey("sampleCount")) {
        config.setSampleCount(configObj["sampleCount"]);
    }
    if (configObj.containsKey("windowSize")) {
        config.setWindowSize(configObj["windowSize"]);
    }
    if (configObj.containsKey("stdThreshold")) {
        config.setStdThreshold(configObj["stdThreshold"]);
    }
    if (configObj.containsKey("lowLevelAlert")) {
        config.setLowLevelAlert(configObj["lowLevelAlert"]);
    }
    if (configObj.containsKey("highLevelAlert")) {
        config.setHighLevelAlert(configObj["highLevelAlert"]);
    }
    if (configObj.containsKey("deviceName")) {
        config.setDeviceName(configObj["deviceName"]);
    }
    if (configObj.containsKey("wifiSSID")) {
        config.setWiFiSSID(configObj["wifiSSID"]);
    }
    if (configObj.containsKey("wifiPassword")) {
        config.setWiFiPassword(configObj["wifiPassword"]);
    }
    
    // 保存配置
    config.save();
    
    // 发送确认消息
    StaticJsonDocument<128> response;
    response["status"] = "config_updated";
    String responseStr;
    serializeJson(response, responseStr);
    dtuSerial.println(responseStr);
} 