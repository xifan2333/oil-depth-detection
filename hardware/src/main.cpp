#include <Arduino.h>
#include "Config.h"
#include "UltrasonicSensor.h"
#include "WebManager.h"
#include "DTUManager.h"

// 配置实例
Config config;

// 其他组件实例声明（延迟初始化）
UltrasonicSensor* sensor = nullptr;
WebManager* webManager = nullptr;
DTUManager* dtuManager = nullptr;

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
    
    // 在配置加载完成后，创建其他组件实例
    sensor = new UltrasonicSensor(TRIG_PIN, ECHO_PIN, config);
    webManager = new WebManager(config);
    dtuManager = new DTUManager(Serial2, DTU_RX_PIN, DTU_TX_PIN, config);
    
    // 初始化传感器
    Serial.println("初始化传感器...");
    sensor->begin();
    
    // 初始化Web服务
    Serial.println("初始化Web服务...");
    webManager->begin();
    
    // 初始化DTU
    Serial.println("初始化DTU...");
    dtuManager->begin(115200);
    
    Serial.println("系统初始化完成!");
}

void loop() {
    // 更新Web服务
    webManager->loop();
    
    // 处理DTU接收的数据
    dtuManager->handleReceive();
    
    // 定时测量
    unsigned long currentTime = millis();
    if (currentTime - lastMeasureTime >= MEASURE_INTERVAL) {
        // 获取滤波后的距离
        float distance = sensor->getDistance();
        
        // 打印测量距离
        Serial.print("距离: ");
        Serial.print(distance);
        Serial.println(" cm");
        
        // 如果测量有效
        if (distance != UltrasonicSensor::INVALID_DISTANCE) {
            // 计算油位
            float tankHeight = config.getTankHeight();
            float sensorOffset = config.getSensorOffset();
            float oilLevel = tankHeight - (distance - sensorOffset);
            
            // 更新Web显示
            webManager->updateMeasurement(oilLevel);
            
            // 检查是否需要发送数据到云平台
            if (currentTime - lastSendTime >= SEND_INTERVAL) {
                if (dtuManager->sendData(distance, sensor->getConfidenceScore())) {  // 发送滤波后的距离
                    Serial.println("数据已成功发送到云平台");
                    lastSendTime = currentTime;
                } else {
                    Serial.println("发送数据到云平台失败");
                }
            }
        } else {
            Serial.println("测量无效!");
        }
        
        lastMeasureTime = currentTime;
    }
    
    // 允许其他任务执行
    yield();
}