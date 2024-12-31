#include "UltrasonicSensor.h"

UltrasonicSensor::UltrasonicSensor(uint8_t trigPin, uint8_t echoPin, Config& config)
    : trigPin(trigPin), echoPin(echoPin), config(config), filterIndex(0), filterFull(false) {
    // 初始化参数
    updateParams();
    // 初始化滤波缓冲区
    for(int i = 0; i < FILTER_SIZE; i++) {
        filterBuffer[i] = 0.0f;
    }
}

void UltrasonicSensor::begin() {
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    digitalWrite(trigPin, LOW);  // 确保初始状态为低电平
    preHeat();  // 初始化时进行预热
}

void UltrasonicSensor::preHeat() {
    // 进行多次预热测量，丢弃结果
    for(int i = 0; i < PREHEAT_COUNT; i++) {
        measureOnce();
        delay(MEASURE_INTERVAL);
    }
}

void UltrasonicSensor::updateParams() {
    // 从配置对象更新参数
    stdThreshold = config.getStdThreshold();
    maxDistance = config.getTankHeight() + config.getSensorOffset();
    minDistance = 2.0f;  // 最小距离保持固定值
    sampleCount = config.getSampleCount();
}

float UltrasonicSensor::applyMovingAverage(float newValue) {
    // 更新滤波缓冲区
    filterBuffer[filterIndex] = newValue;
    filterIndex = (filterIndex + 1) % FILTER_SIZE;
    if (filterIndex == 0) {
        filterFull = true;
    }
    
    // 计算移动平均
    float sum = 0;
    int count = filterFull ? FILTER_SIZE : filterIndex;
    for(int i = 0; i < count; i++) {
        sum += filterBuffer[i];
    }
    return sum / count;
}

float UltrasonicSensor::calculateConfidence(float distance, float mean, float stdDev) {
    float confidence = 1.0f;
    
    // 1. 距离偏差影响（降低对偏差的敏感度）
    float deviation = abs(distance - mean);
    if (stdDev > 0) {
        confidence *= exp(-deviation / (3 * stdDev));  // 从2倍提高到3倍标准差
    }
    
    // 2. 时间连续性影响（降低时间间隔的影响）
    unsigned long currentTime = millis();
    float timeGap = (currentTime - lastMeasureTime) / 1000.0f;
    confidence *= exp(-timeGap / 10.0f);  // 从5秒增加到10秒
    
    // 3. 与历史数据的一致性（考虑趋势）
    float predicted = predictNextValue();
    if (predicted > 0) {
        float predictionError = abs(distance - predicted);
        // 检查是否处于下降趋势
        bool isDescending = (historyIndex >= 2) && 
            (historyBuffer[(historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE] >
             historyBuffer[(historyIndex - 2 + HISTORY_SIZE) % HISTORY_SIZE]);
             
        if (isDescending && distance < predicted) {
            // 如果在下降趋势中且新值低于预测值，提高容忍度
            confidence *= exp(-predictionError / 20.0f);  // 从10cm增加到20cm
        } else {
            confidence *= exp(-predictionError / 10.0f);
        }
    }
    
    // 4. 考虑系统稳定性（降低稳定性的权重）
    confidence *= (0.7f + 0.3f * stabilityScore);  // 从50%降低到30%
    
    return constrain(confidence, 0.0f, 1.0f);
}

bool UltrasonicSensor::isGradientValid(float newDistance) {
    if (lastValidDistance == 0.0f) return true;
    
    // 计算变化率（cm/s）
    float timeGap = (millis() - lastMeasureTime) / 1000.0f;
    if (timeGap <= 0) return true;
    
    float gradient = abs(newDistance - lastValidDistance) / timeGap;
    
    // 动态变化率限制：
    // 1. 正常模式：100cm/s
    // 2. 快速变化模式：300cm/s（当连续检测到快速变化时）
    static bool fastChangeMode = false;
    static int fastChangeCount = 0;
    
    if (gradient > 100.0f) {
        fastChangeCount++;
        if (fastChangeCount >= 2) {  // 连续2次快速变化进入快速模式
            fastChangeMode = true;
        }
    } else {
        fastChangeCount = 0;
        if (fastChangeMode && gradient < 50.0f) {  // 当变化率小于50cm/s时退出快速模式
            fastChangeMode = false;
        }
    }
    
    return gradient <= (fastChangeMode ? 300.0f : 100.0f);
}

void UltrasonicSensor::updateHistory(float distance) {
    historyBuffer[historyIndex] = distance;
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
    if (historyIndex == 0) {
        historyFull = true;
    }
}

float UltrasonicSensor::predictNextValue() {
    if (!historyFull && historyIndex < 2) return 0;
    
    int count = historyFull ? HISTORY_SIZE : historyIndex;
    if (count < 2) return 0;
    
    // 使用简单线性回归预测
    float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (int i = 0; i < count; i++) {
        float x = i;
        float y = historyBuffer[i];
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }
    
    float n = count;
    float slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    float intercept = (sumY - slope * sumX) / n;
    
    // 预测下一个值
    return intercept + slope * count;
}

float UltrasonicSensor::getDistance() {
    updateParams();
    
    float* samples = new float[sampleCount];
    int validCount = 0;
    
    // 收集样本
    for (int i = 0; i < sampleCount * 3 && validCount < sampleCount; i++) {
        float distance = measureOnce();
        if (distance != INVALID_DISTANCE && isGradientValid(distance)) {
            samples[validCount++] = distance;
        }
        delay(MEASURE_INTERVAL);
    }
    
    if (validCount < sampleCount / 2) {
        delete[] samples;
        return INVALID_DISTANCE;
    }
    
    float mean = calculateMean(samples, validCount);
    float stdDev = calculateStdDev(samples, validCount, mean);
    
    float sum = 0;
    float weightSum = 0;
    
    for (int i = 0; i < validCount; i++) {
        float confidence = calculateConfidence(samples[i], mean, stdDev);
        sum += samples[i] * confidence;
        weightSum += confidence;
    }
    
    delete[] samples;
    
    float result;
    if (weightSum > 0) {
        result = sum / weightSum;
        confidenceScore = weightSum / validCount;
        lastValidDistance = result;
        lastMeasureTime = millis();
        updateHistory(result);
        updateStabilityScore(result);  // 更新稳定性得分
    } else {
        result = INVALID_DISTANCE;
        confidenceScore = 0;
    }
    
    return applyMovingAverage(result);
}

float UltrasonicSensor::measureOnce() {
    // 确保上一次测量完全结束
    digitalWrite(trigPin, LOW);
    delayMicroseconds(5);
    
    // 发送触发信号
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(TRIGGER_DURATION);
    digitalWrite(trigPin, LOW);
    
    // 读取回声时间
    long duration = pulseIn(echoPin, HIGH, MEASURE_TIMEOUT);
    
    // 如果超时，返回无效值
    if (duration == 0) {
        return INVALID_DISTANCE;
    }
    
    // 计算距离
    float distance = duration * SOUND_SPEED / 2;
    
    // 检查是否在有效范围内
    if (distance < minDistance || distance > maxDistance) {
        return INVALID_DISTANCE;
    }
    
    return distance;
}

float UltrasonicSensor::calculateMean(float* arr, int size) {
    if (size == 0) return 0;
    
    float sum = 0;
    for (int i = 0; i < size; i++) {
        sum += arr[i];
    }
    return sum / size;
}

float UltrasonicSensor::calculateStdDev(float* arr, int size, float mean) {
    if (size <= 1) return 0;
    
    float sumSquares = 0;
    for (int i = 0; i < size; i++) {
        float diff = arr[i] - mean;
        sumSquares += diff * diff;
    }
    return sqrt(sumSquares / (size - 1));  // 使用n-1以获得无偏估计
}

void UltrasonicSensor::updateStabilityScore(float newDistance) {
    if (lastValidDistance == 0.0f) {
        lastValidDistance = newDistance;
        return;
    }

    // 计算相对变化率
    float variation = abs(newDistance - lastValidDistance) / lastValidDistance;
    
    // 更新变化率历史
    recentVariations[variationIndex] = variation;
    variationIndex = (variationIndex + 1) % STABILITY_WINDOW;
    if (variationIndex == 0) {
        stabilityWindowFull = true;
    }
    
    // 计算稳定性得分
    float totalVariation = 0;
    int count = stabilityWindowFull ? STABILITY_WINDOW : variationIndex;
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            totalVariation += recentVariations[i];
        }
        // 使用改进的稳定性计算，降低快速变化的惩罚
        float baseScore = exp(-totalVariation / count * 5);  // 降低惩罚系数从10到5
        
        // 检测持续下降趋势
        int descendingCount = 0;
        for (int i = 1; i < count && i < 5; i++) {  // 只检查最近5个样本
            if (historyBuffer[(historyIndex - i + HISTORY_SIZE) % HISTORY_SIZE] >
                historyBuffer[(historyIndex - i + 1 + HISTORY_SIZE) % HISTORY_SIZE]) {
                descendingCount++;
            }
        }
        
        // 如果检测到持续下降，提高稳定性得分
        if (descendingCount >= 3) {  // 3/5的样本显示下降趋势
            baseScore = (baseScore + 1.0f) / 2.0f;  // 提升稳定性得分
        }
        
        stabilityScore = baseScore;
    }
} 