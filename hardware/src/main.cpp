#include <Arduino.h>
#include <modem.h>
#include <PPP.h>
#include "logger.h"

HardwareSerial modemSerial(1);

void testModemBasicFunctions() {
    Serial.println("\n========= 基础功能测试 =========");
    
    // 测试AT指令
    Serial.println("\n1. 测试AT指令响应:");
    String response = modem.sendCommand("AT");
    Serial.println("发送: AT");
    Serial.println("响应: " + response);

    // 获取信号强度
    Serial.println("\n2. 获取信号强度:");
    response = modem.sendCommand("AT+CSQ");
    Serial.println("发送: AT+CSQ");
    Serial.println("响应: " + response);

    // 获取模块信息
    Serial.println("\n3. 获取模块信息:");
    response = modem.sendCommand("ATI");
    Serial.println("发送: ATI");
    Serial.println("响应: " + response);

    // 获取IMEI号
    Serial.println("\n4. 获取IMEI号:");
    String imei = modem.getIMEI();
    Serial.println("IMEI: " + imei);
}

void testPPPconnect() {
    Serial.println("\n========= PPP拨号测试 =========");
    
    Serial.println("\n1. 开始拨号...");
    if (modem.connect("CMNET", "", "")) {  // 修改这里使用中国移动APN
        Serial.println("拨号成功！");
        
        // 等待5秒
        delay(5000);
        
        Serial.println("\n2. 测试命令模式切换...");
        if (modem.setCommandMode()) {
            Serial.println("切换到命令模式成功");
            
            // 在命令模式下查询信号强度
            String response = modem.sendCommand("AT+CSQ");
            Serial.println("信号强度查询: " + response);
            
            Serial.println("\n3. 切换回数据模式...");
            if (modem.setDataMode()) {
                Serial.println("切换到数据模式成功");
            } else {
                Serial.println("切换到数据模式失败");
            }
        } else {
            Serial.println("切换到命令模式失败");
        }
        
        Serial.println("\n4. 断开连接...");
        if (modem.hangup()) {
            Serial.println("断开连接成功");
        } else {
            Serial.println("断开连接失败");
        }
    } else {
        Serial.println("拨号失败！");
    }
}

void testModemMode() {
    Serial.println("\n========= 模式检测测试 =========");
    
    // 测试命令模式
    Serial.println("\n1. 测试命令模式检测:");
    if (modem.isCommandMode()) {
        Serial.println("当前在命令模式");
        String response = modem.sendCommand("AT+CSQ");
        Serial.println("信号强度查询: " + response);
    } else {
        Serial.println("当前不在命令模式");
    }

    // 尝试PPP拨号
    Serial.println("\n2. 尝试PPP拨号:");
    if (modem.connect("CMNET", "", "")) {
        Serial.println("PPP拨号成功");
        delay(2000); // 等待PPP协商完成
        
        // 检查数据模式
        Serial.println("\n3. 检查数据模式:");
        if (!modem.isCommandMode()) {
           Serial.println("当前在数据模式");
        } else {
            Serial.println("未检测到PPP数据流");
        }
    } else {
        Serial.println("PPP拨号失败");
    }
}

void processSerialCommand() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        // 特殊命令处理
        if (command == "test") {
            testModemBasicFunctions();
            return;
        } else if (command == "connect") {
            testPPPconnect();
            return;
        }
        
        // 普通AT指令处理
        if (command.length() > 0) {
            Serial.println("\n发送命令: " + command);
            String response = modem.sendCommand(command);
            Serial.println("响应: " + response);
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    // 初始化日志系统
    LOGGER.begin(Serial, LogLevel::DEBUG);
    
    // 设置RTC时间（假设从NTP或其他来源获取）
    time_t rtcTime = 1677649200; // 2023-03-01 12:00:00 UTC
    LOGGER.setTime(rtcTime);
    
    LOG_MODULE("MAIN");
    LOG_I("系统启动");
    
    // 初始化调试串口
    Serial.println("\n============================");
    Serial.println("调制解调器测试程序启动...");
    Serial.println("============================");

    // 初始化modem串口
    modemSerial.begin(115200, SERIAL_8N1, 16, 17);
    
    // 初始化modem
    if (!modem.begin(modemSerial)) {
        Serial.println("调制解调器初始化失败!");
        return;
    }
    Serial.println("调制解调器初始化成功!");
    
    // 获取网络时间并更新RTC
    time_t networkTime = modem.getNetworkTime();
    if (networkTime > 0) {
        Serial.println("网络时间同步成功: " + String(networkTime));
    } else {
        Serial.println("网络时间同步失败");
    }
    
    // 执行基础功能测试
    testModemBasicFunctions();
    
    // 添加模式检测测试
    testModemMode();
    
    Serial.println("\n============================");
    Serial.println("可用命令:");
    Serial.println("1. test  - 执行基础功能测试");
    Serial.println("2. connect  - 执行PPP拨号测试");
    Serial.println("3. 直接输入AT指令");
    Serial.println("============================\n");
}

void loop() {
    // 处理串口命令
    processSerialCommand();
}
