#include "WebManager.h"

WebManager::WebManager(Config& config) 
    : config(config), server(80) {
}

WebManager::~WebManager() {
}

void WebManager::begin() {
    // 初始化文件系统
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS挂载失败");
        return;
    }

    // 配置WiFi AP模式
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.getWiFiSSID(), config.getWiFiPassword());

    Serial.print("AP IP地址: ");
    Serial.println(WiFi.softAPIP());

    // 设置CORS头
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    // 设置路由
    setupRoutes();

    // 启动服务器
    server.begin();
    Serial.println("Web服务器已启动");
}

void WebManager::loop() {
    // ESPAsyncWebServer 是完全异步的，不需要在 loop 中处理
}

void WebManager::setupRoutes() {
    // 配置API路由
    AsyncCallbackJsonWebHandler* setConfigHandler = new AsyncCallbackJsonWebHandler(
        "/api/config",
        [this](AsyncWebServerRequest* request, JsonVariant& json) {
            this->handleSetConfig(request, json);
        }
    );
    server.addHandler(setConfigHandler);

    // 获取配置
    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetConfig(request);
    });

    // 重置配置
    server.on("/api/config/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleResetConfig(request);
    });

    // 获取实时数据
    server.on("/api/data", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetData(request);
    });

    // 处理静态文件
    server.onNotFound([this](AsyncWebServerRequest* request) {
        this->handleFileRead(request);
    });
}

void WebManager::handleGetConfig(AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(1024);
    
    doc["tankHeight"] = config.getTankHeight();
    doc["sensorOffset"] = config.getSensorOffset();
    doc["sampleInterval"] = config.getSampleInterval();
    doc["sampleCount"] = config.getSampleCount();
    doc["windowSize"] = config.getWindowSize();
    doc["stdThreshold"] = config.getStdThreshold();
    doc["lowLevelAlert"] = config.getLowLevelAlert();
    doc["highLevelAlert"] = config.getHighLevelAlert();
    doc["deviceName"] = config.getDeviceName();
    doc["wifiSSID"] = config.getWiFiSSID();
    // 出于安全考虑，不返回WiFi密码

    sendJsonResponse(request, doc);
}

void WebManager::handleSetConfig(AsyncWebServerRequest* request, JsonVariant& json) {
    JsonObject jsonObj = json.as<JsonObject>();
    
    if (jsonObj.isNull()) {
        sendErrorResponse(request, 400, "无效的JSON数据");
        return;
    }

    // 更新配置
    if (jsonObj.containsKey("tankHeight")) config.setTankHeight(jsonObj["tankHeight"]);
    if (jsonObj.containsKey("sensorOffset")) config.setSensorOffset(jsonObj["sensorOffset"]);
    if (jsonObj.containsKey("sampleInterval")) config.setSampleInterval(jsonObj["sampleInterval"]);
    if (jsonObj.containsKey("sampleCount")) config.setSampleCount(jsonObj["sampleCount"]);
    if (jsonObj.containsKey("windowSize")) config.setWindowSize(jsonObj["windowSize"]);
    if (jsonObj.containsKey("stdThreshold")) config.setStdThreshold(jsonObj["stdThreshold"]);
    if (jsonObj.containsKey("lowLevelAlert")) config.setLowLevelAlert(jsonObj["lowLevelAlert"]);
    if (jsonObj.containsKey("highLevelAlert")) config.setHighLevelAlert(jsonObj["highLevelAlert"]);
    if (jsonObj.containsKey("deviceName")) config.setDeviceName(jsonObj["deviceName"]);
    if (jsonObj.containsKey("wifiSSID")) config.setWiFiSSID(jsonObj["wifiSSID"]);
    if (jsonObj.containsKey("wifiPassword")) config.setWiFiPassword(jsonObj["wifiPassword"]);

    // 保存配置
    config.save();

    // 返回成功响应
    request->send(200, "application/json", "{\"status\":\"success\"}");
}

void WebManager::handleResetConfig(AsyncWebServerRequest* request) {
    config.resetToDefault();
    config.save();
    request->send(200, "application/json", "{\"status\":\"success\"}");
}

void WebManager::handleGetData(AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(512);
    JsonObject data = doc.to<JsonObject>();
    
    // 直接使用最新的测量数据
    data["oilLevel"] = currentOilLevel;
    data["timestamp"] = lastUpdateTime;
    data["deviceName"] = config.getDeviceName();
    data["lowLevelAlert"] = config.getLowLevelAlert();
    data["highLevelAlert"] = config.getHighLevelAlert();
    data["tankHeight"] = config.getTankHeight();
    
    // 如果有回调，则允许回调添加额外数据
    if (dataCallback) {
        dataCallback(data);
    }
    
    sendJsonResponse(request, doc);
}

void WebManager::handleFileRead(AsyncWebServerRequest* request) {
    String path = request->url();
    
    // 如果路径是根目录，则返回dashboard.html
    if (path.endsWith("/")) path += "dashboard.html";
    
    String contentType = getContentType(path);
    
    // 尝试从LittleFS打开文件
    if (LittleFS.exists(path)) {
        request->send(LittleFS, path, contentType);
    } else {
        request->send(404, "text/plain", "文件未找到");
    }
}

String WebManager::getContentType(const String& filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".json")) return "application/json";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

void WebManager::sendJsonResponse(AsyncWebServerRequest* request, const JsonDocument& doc) {
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebManager::sendErrorResponse(AsyncWebServerRequest* request, int code, const String& message) {
    DynamicJsonDocument doc(128);
    doc["error"] = message;
    String response;
    serializeJson(doc, response);
    request->send(code, "application/json", response);
}

void WebManager::updateMeasurement(float oilLevel) {
    currentOilLevel = oilLevel;
    lastUpdateTime = millis();
    
    // 不在这里调用回调，而是在handleGetData中使用存储的数据
    Serial.print("油位: ");
    Serial.print(oilLevel);
    Serial.println(" cm");
} 