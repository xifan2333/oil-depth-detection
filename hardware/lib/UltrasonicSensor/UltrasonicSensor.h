#ifndef ULTRASONIC_SENSOR_H
#define ULTRASONIC_SENSOR_H

#include <Arduino.h>
#include "Config.h"

class UltrasonicSensor {
public:
    UltrasonicSensor(uint8_t trigPin, uint8_t echoPin, Config& config);
    
    // 初始化传感器
    void begin();
    
    // 更新滤波参数（从配置中）
    void updateParams();
    
    // 获取距离测量值
    float getDistance();
    
    // 获取测量置信度
    float getConfidenceScore() const { return confidenceScore; }
    
    // 获取系统稳定性得分
    float getStabilityScore() const { return stabilityScore; }
    
    // 无效距离标记
    static constexpr float INVALID_DISTANCE = -1.0f;

private:
    uint8_t trigPin;
    uint8_t echoPin;
    Config& config;  // 配置引用
    
    // 滤波参数
    float stdThreshold;    // 标准差阈值
    float maxDistance;     // 最大有效距离
    float minDistance;     // 最小有效距离
    int sampleCount;       // 采样次数
    
    // 测量常量 - 5V供电优化
    static constexpr float SOUND_SPEED = 0.0339f;    // 12℃时的声速
    static constexpr int TRIGGER_DURATION = 15;       // 触发时间
    static constexpr int MEASURE_TIMEOUT = 25000;     // 测量超时时间(约4米)
    static constexpr int MEASURE_INTERVAL = 100;      // 测量间隔
    static constexpr int PREHEAT_COUNT = 5;          // 预热次数
    
    // 移动平均滤波缓冲区
    static constexpr int FILTER_SIZE = 3;            // 滤波点数
    float filterBuffer[FILTER_SIZE];                 // 滤波缓冲区
    int filterIndex;                                 // 当前滤波索引
    bool filterFull;                                 // 滤波器是否已填满
    
    // 内部测量函数
    float measureOnce();
    void preHeat();  // 预热函数
    float applyMovingAverage(float newValue);  // 应用移动平均滤波
    
    // 统计函数
    float calculateMean(float* arr, int size);
    float calculateStdDev(float* arr, int size, float mean);
    
    static const int HISTORY_SIZE = 10;  // 历史数据缓存大小
    float historyBuffer[HISTORY_SIZE];   // 历史数据缓冲区
    int historyIndex = 0;                // 历史数据索引
    bool historyFull = false;            // 历史缓冲区是否已满
    float lastValidDistance = 0.0f;      // 上次有效距离
    unsigned long lastMeasureTime = 0;    // 上次测量时间
    float confidenceScore = 1.0f;        // 测量置信度
    
    // 新增的私有方法
    float calculateConfidence(float distance, float mean, float stdDev);
    bool isGradientValid(float newDistance);
    void updateHistory(float distance);
    float predictNextValue();
    
    // 稳定性评估
    static constexpr int STABILITY_WINDOW = 20;     // 稳定性评估窗口
    float stabilityScore = 0.0f;                    // 稳定性得分
    float recentVariations[STABILITY_WINDOW];       // 最近变化率
    int variationIndex = 0;                         // 变化率索引
    bool stabilityWindowFull = false;               // 稳定性窗口是否已满
    
    // 新增的稳定性相关方法
    void updateStabilityScore(float newDistance);
    bool isSystemStable() const { return stabilityScore > 0.8f; }
};

#endif // ULTRASONIC_SENSOR_H 