#ifndef WEBMANAGER_H
#define WEBMANAGER_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "Config.h"

class WebManager {
public:
    WebManager(Config& config);
    ~WebManager();

    // 初始化WiFi和Web服务器
    void begin();
    
    // 处理循环任务
    void loop();
    
    // 更新测量数据
    void updateMeasurement(float oilLevel);

    // 设置数据更新回调
    typedef std::function<void(JsonObject&)> DataCallback;
    void setDataCallback(DataCallback callback) {
        dataCallback = callback;
    }

private:
    Config& config;
    AsyncWebServer server;
    DataCallback dataCallback;
    
    // 当前测量数据
    float currentOilLevel;
    unsigned long lastUpdateTime;

    // 设置路由处理器
    void setupRoutes();
    
    // API处理函数
    void handleGetConfig(AsyncWebServerRequest* request);
    void handleSetConfig(AsyncWebServerRequest* request, JsonVariant& json);
    void handleResetConfig(AsyncWebServerRequest* request);
    void handleGetData(AsyncWebServerRequest* request);
    
    // 文件处理函数
    void handleFileRead(AsyncWebServerRequest* request);
    String getContentType(const String& filename);
    
    // 响应助手函数
    void sendJsonResponse(AsyncWebServerRequest* request, const JsonDocument& doc);
    void sendErrorResponse(AsyncWebServerRequest* request, int code, const String& message);
};

#endif // WEBMANAGER_H 