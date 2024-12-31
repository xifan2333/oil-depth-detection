#include <Arduino.h>
#include "Config.h"
#include "UltrasonicSensor.h"
#include "WebManager.h"
#include "DTUManager.h"

// 配置实例
Config config;

// 传感器实例
UltrasonicSensor sensor(TRIG_PIN, ECHO_PIN, config);

// Web管理器实例
WebManager webManager(config);

// DTU管理器实例 (使用硬件串口2)
DTUManager dtuManager(Serial2, DTU_RX_PIN, DTU_TX_PIN, config);

// 上次发送数据的时间
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 10000;  // 发送间隔10秒

// 测量更新间隔
const unsigned long MEASURE_INTERVAL = 1000;  // 测量间隔1秒
unsigned long lastMeasureTime = 0;

void setup() {
    // 初始化串口用于调试
    Serial.begin(115200);
    Serial.println("\n系统启动中...");
    
    // 首先初始化配置
    Serial.println("初始化配置...");
    config.begin();
    
    // 等待配置初始化完成
    delay(100);
    
    // 打印当前配置值确认
    Serial.printf("确认配置值:\n");
    Serial.printf("储油井深度: %.1f cm\n", config.getTankHeight());
    Serial.printf("低油位警告: %.1f%%\n", config.getLowLevelAlert());
    Serial.printf("高油位警告: %.1f%%\n", config.getHighLevelAlert());
    
    // 初始化传感器
    Serial.println("初始化传感器...");
    sensor.begin();
    
    // 初始化Web服务
    Serial.println("初始化Web服务...");
    webManager.begin();
    
    // 初始化DTU
    Serial.println("初始化DTU...");
    dtuManager.begin(115200);
    
    Serial.println("系统初始化完成!");
}

void loop() {
    // 更新Web服务
    webManager.loop();
    
    // 处理DTU接收的数据
    dtuManager.handleReceive();
    
    // 定时测量
    unsigned long currentTime = millis();
    if (currentTime - lastMeasureTime >= MEASURE_INTERVAL) {
        // 获取测量值
        float distance = sensor.getDistance();
        
        // 如果测量有效
        if (distance != UltrasonicSensor::INVALID_DISTANCE) {
            // 计算油位
            float tankHeight = config.getTankHeight();
            float sensorOffset = config.getSensorOffset();
            float oilLevel = tankHeight - (distance - sensorOffset);
            
            // 更新Web显示
            webManager.updateMeasurement(oilLevel);
            
            // 检查是否需要发送数据到云平台
            if (currentTime - lastSendTime >= SEND_INTERVAL) {
                if (dtuManager.sendData(oilLevel, sensor.getConfidenceScore())) {
                    Serial.println("Data sent to cloud successfully");
                    lastSendTime = currentTime;
                } else {
                    Serial.println("Failed to send data to cloud");
                }
            }
        }
        
        lastMeasureTime = currentTime;
    }
    
    // 允许其他任务执行
    yield();
}