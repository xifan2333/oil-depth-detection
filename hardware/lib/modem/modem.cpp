#include "modem.h"

Modem modem;

Modem::Modem() : _uart(nullptr), _initialized(false) {
}

Modem::~Modem() {
}

bool Modem::begin(HardwareSerial& uart) {
    _uart = &uart;
    _initialized = true;
    
    // 测试AT指令是否响应
    return isReady();
}

String Modem::sendCommand(const String& command, uint32_t timeout) {
    if (!_initialized || !_uart) {
        Serial.println("DEBUG: 调制解调器未初始化");
        return "";
    }

    Serial.println("DEBUG: 清空接收缓冲区");
    flushInput();
    
    Serial.println("DEBUG: 发送命令: " + command);
    _uart->println(command);
    
    String response;
    unsigned long startTime = millis();
    
    Serial.println("DEBUG: 等待响应...");
    while (millis() - startTime < timeout) {
        if (_uart->available()) {
            char c = _uart->read();
            response += c;
            
            // 打印接收到的每个字符
            Serial.printf("DEBUG: 收到字符: 0x%02X ", (uint8_t)c);
            if (isprint(c)) {
                Serial.printf("('%c')", c);
            }
            Serial.println();
            
            // 检查是否收到完整响应
            if (response.endsWith("\r\nOK\r\n") || 
                response.endsWith("\r\nERROR\r\n") ||
                response.endsWith("\r\nNO CARRIER\r\n")) {
                Serial.println("DEBUG: 收到完整响应");
                break;
            }
        }
        yield();
    }
    
    Serial.println("DEBUG: 完整响应: " + response);
    return response;
}

bool Modem::isReady() {
    String response = sendCommand("AT");
    return response.indexOf("OK") >= 0;
}

void Modem::flushInput() {
    if (!_uart) return;
    
    while (_uart->available()) {
        _uart->read();
    }
}

String Modem::getIMEI() {
    String response = sendCommand("AT+GSN");
    if (response.indexOf("OK") < 0) {
        return "";
    }
    
    // 提取IMEI号
    // 响应格式通常为: \r\n123456789012345\r\n\r\nOK\r\n
    response.trim();
    int start = response.indexOf("\r\n") + 2;
    int end = response.indexOf("\r\n", start);
    if (start >= 2 && end > start) {
        return response.substring(start, end);
    }
    return "";
}

bool Modem::dial(const char* apn, const char* username, const char* password) {
    static int connectCount = 0;  // 连接尝试计数
    
    // 如果尝试次数超过5次，返回失败
    if (connectCount >= 5) {
        Serial.println("DEBUG: 连接尝试次数超过5次，放弃连接");
        connectCount = 0;
        return false;
    }

    // 确保在命令模式
    if (_inDataMode && !enterCommandMode()) {
        return false;
    }

    // 1. 检查SIM卡状态
    Serial.println("DEBUG: 检查SIM卡状态");
    String response = sendCommand("AT+CPIN?");
    if (response.indexOf("+CPIN: READY") < 0) {
        Serial.println("DEBUG: SIM卡未就绪");
        return false;
    }

    // 2. 检查网络注册状态
    Serial.println("DEBUG: 检查网络注册状态");
    response = sendCommand("AT+CREG?");
    if (response.indexOf("+CREG: 0,1") < 0 && response.indexOf("+CREG: 0,5") < 0) {
        Serial.println("DEBUG: 等待60秒进行网络注册");
        delay_ms(60000);  // 等待60秒
        response = sendCommand("AT+CREG?");
        if (response.indexOf("+CREG: 0,1") < 0 && response.indexOf("+CREG: 0,5") < 0) {
            Serial.println("DEBUG: 网络注册失败");
            connectCount++;
            return dial(apn, username, password);  // 递归重试
        }
    }

    // 3. 检查PS网络附着状态
    Serial.println("DEBUG: 检查PS网络附着状态");
    response = sendCommand("AT+CGATT?");
    if (response.indexOf("+CGATT: 1") < 0) {
        Serial.println("DEBUG: 尝试PS网络附着");
        if (sendCommand("AT+CGATT=1").indexOf("OK") < 0) {
            Serial.println("DEBUG: PS网络附着失败");
            connectCount++;
            return dial(apn, username, password);
        }
        delay_ms(2000);  // 等待PS附着完成
    }

    // 4. 设置APN
    Serial.println("DEBUG: 设置APN");
    String cmd = "AT+CGDCONT=1,\"IP\",\"";
    cmd += apn;
    cmd += "\"";
    if (sendCommand(cmd).indexOf("OK") < 0) {
        Serial.println("DEBUG: APN设置失败");
        connectCount++;
        return dial(apn, username, password);
    }

    // 如果提供了用户名和密码，设置认证信息
    if (strlen(username) > 0) {
        cmd = "AT+CGAUTH=1,1,\"";
        cmd += username;
        cmd += "\",\"";
        cmd += password;
        cmd += "\"";
        if (sendCommand(cmd).indexOf("OK") < 0) {
            Serial.println("DEBUG: 认证信息设置失败");
            connectCount++;
            return dial(apn, username, password);
        }
    }

    // 5. 开始PPP拨号
    Serial.println("DEBUG: 开始PPP拨号");
    response = sendCommand("ATD*99#", 30000);
    if (response.indexOf("CONNECT") < 0) {
        Serial.println("DEBUG: PPP拨号失败");
        connectCount++;
        return dial(apn, username, password);
    }

    // 拨号成功
    Serial.println("DEBUG: PPP拨号成功");
    _inDataMode = true;
    connectCount = 0;  // 重置计数器
    return true;
}

bool Modem::enterCommandMode() {
    if (!_inDataMode) {
        Serial.println("DEBUG: 已经在命令模式");
        return true;
    }

    Serial.println("DEBUG: 尝试进入命令模式...");
    
    // 等待1秒以上的安静时间
    delay_ms(1100);
    
    // 发送+++前先发送一些无效数据，确保PPP空闲
    _uart->write((const uint8_t*)"   ", 3);
    delay_ms(100);
    
    // 发送+++
    _uart->write((const uint8_t*)"+++", 3);
    _uart->flush();
    
    delay_ms(1100);

    // 清空PPP数据
    flushInput();

    // 发送AT测试
    String testResponse = sendCommand("AT");
    
    if (testResponse.indexOf("OK") >= 0) {
        _inDataMode = false;
        return true;
    }

    return false;
}

bool Modem::enterDataMode() {
    if (_inDataMode) {
        Serial.println("DEBUG: 已经在数据模式");
        return true;
    }

    Serial.println("DEBUG: 检查PDP状态...");
    
    // 检查PDP上下文状态
    String response = sendCommand("AT+CGACT?");
    if (response.indexOf("+CGACT: 1,1") >= 0) {
        // PDP上下文激活，尝试使用ATO恢复
        Serial.println("DEBUG: PDP上下文已激活，尝试恢复数据模式");
        response = sendCommand("ATO");
        if (response.indexOf("CONNECT") >= 0) {
            _inDataMode = true;
            Serial.println("DEBUG: 数据模式恢复成功");
            return true;
        }
        Serial.println("DEBUG: ATO恢复失败，尝试重新拨号");
    }

    // ATO失败或PDP未激活，尝试重新拨号
    Serial.println("DEBUG: 尝试重新拨号");
    return dial("CMNET", "", "");  // 使用中国移动APN重新拨号
}

bool Modem::hangup() {
    // 确保在命令模式
    if (_inDataMode && !enterCommandMode()) {
        return false;
    }

    // 发送挂断命令
    String response = sendCommand("ATH");
    if (response.indexOf("OK") >= 0 || response.indexOf("NO CARRIER") >= 0) {
        _inDataMode = false;
        return true;
    }
    return false;
}

void Modem::delay_ms(uint32_t ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        yield();
    }
}

// 添加一个新方法用于检查PPP连接状态
bool Modem::checkPPPStatus() {
    if (!_inDataMode) {
        return false;
    }

    // 尝试进入命令模式
    if (!enterCommandMode()) {
        return false;
    }

    // 检查IP地址分配
    String response = sendCommand("AT+CGPADDR=1");
    bool hasIP = response.indexOf("+CGPADDR: 1,\"") >= 0;

    // 如果需要，返回数据模式
    if (hasIP) {
        enterDataMode();
    }

    return hasIP;
}


