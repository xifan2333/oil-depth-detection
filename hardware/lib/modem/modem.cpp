#include "modem.h"
#include "logger.h"

Modem modem;

Modem::Modem() : _uart(nullptr), _initialized(false)
{
}

Modem::~Modem()
{
}

bool Modem::begin(HardwareSerial &uart)
{
    _uart = &uart;
    _initialized = true;

    LOG_MODULE("MODEM");
    LOG_LEVEL(LogLevel::DEBUG);
    LOG_D("初始化调制解调器");
    return isReady();
}

bool Modem::isCommandMode()
{
    // 先尝试发送AT命令
    flushInput();
    _uart->println("AT");

    // 等待响应，使用较短的超时时间
    String response;
    unsigned long startTime = millis();
    while (millis() - startTime < 1000)
    {
        if (_uart->available())
        {
            char c = _uart->read();
            response += c;
            if (response.endsWith("\r\nOK\r\n"))
            {
                LOG_D("当前在命令模式");
                return true;
            }
        }
        yield();
    }

    LOG_D("当前在数据模式");
    return false;
}

bool Modem::setCommandMode()
{
    if (isCommandMode())
    {
        return true;
    }

    LOG_D("尝试进入命令模式...");

    // 1. 输入+++前至少一秒内不可输入任何字符
    delay_ms(1100);

    // 发送无效数据以打断可能的PPP帧
    _uart->write((const uint8_t *)"   ", 3);
    delay_ms(100);

    // 2. 1秒内输入+++
    _uart->write((const uint8_t *)"+++", 3);
    _uart->flush();

    // 3. 输入+++后1秒内不可输入任何字符
    delay_ms(1100);

    // 清空PPP数据
    flushInput();

    // 检查是否成功进入命令模式
    return isCommandMode();
}

bool Modem::setDataMode()
{
    if (!isCommandMode()) {  // 如果不是命令模式，就是数据模式
        LOG_D("已经在数据模式");
        return true;
    }

    String response = sendCommand("ATO");
    if (response.indexOf("CONNECT") >= 0) {
        LOG_D("数据模式恢复成功");
        return true;
    }
    return connect("CMNET", "", "");
}

String Modem::sendCommand(const String &command, uint32_t timeout)
{
    if (!_initialized || !_uart)
    {
        LOG_E("调制解调器未初始化");
        return "";
    }

    // 检查并确保在命令模式
    if (!isCommandMode() && !setCommandMode())
    {
        LOG_E("无法进入命令模式");
        return "";
    }

    LOG_D("清空接收缓冲区");
    flushInput();

    LOG_D("发送命令: " + command);
    _uart->println(command);

    String response;
    unsigned long startTime = millis();

    LOG_D("等待响应...");
    while (millis() - startTime < timeout)
    {
        if (_uart->available())
        {
            char c = _uart->read();
            response += c;

            LOG_F("收到字符: 0x%02X %c", (uint8_t)c, isprint(c) ? c : ' ');

            if (response.endsWith("\r\nOK\r\n") ||
                response.endsWith("\r\nERROR\r\n") ||
                response.endsWith("\r\nNO CARRIER\r\n"))
            {
                LOG_D("收到完整响应");
                break;
            }
        }
        yield();
    }

    LOG_D("完整响应: " + response);
    return response;
}

bool Modem::isReady()
{
    String response = sendCommand("AT");
    return response.indexOf("OK") >= 0;
}

void Modem::flushInput()
{
    if (!_uart)
        return;

    while (_uart->available())
    {
        _uart->read();
    }
}

String Modem::getIMEI()
{
    String response = sendCommand("AT+GSN");
    if (response.indexOf("OK") < 0)
    {
        return "";
    }

    // 提取IMEI号
    // 响应格式通常为: \r\n123456789012345\r\n\r\nOK\r\n
    response.trim();
    int start = response.indexOf("\r\n") + 2;
    int end = response.indexOf("\r\n", start);
    if (start >= 2 && end > start)
    {
        return response.substring(start, end);
    }
    return "";
}

time_t Modem::getNetworkTime() {
    // 确保在命令模式
    if (!isCommandMode() && !setCommandMode()) {
        return 0;
    }

    // 设置时区为中国(GMT+8)
    String response = sendCommand("AT+CTZR=1");
    if (response.indexOf("OK") < 0) {
        LOG_E("设置时区报告失败");
        return 0;
    }

    // 查询网络时间
    response = sendCommand("AT+CCLK?");
    if (response.indexOf("+CCLK:") < 0) {
        LOG_E("获取网络时间失败");
        return 0;
    }

    // 解析时间字符串
    // 格式: "+CCLK: \"yy/MM/dd,HH:mm:ss+zz\""
    int start = response.indexOf("\"") + 1;
    int end = response.indexOf("\"", start);
    if (start < 1 || end < start) {
        LOG_E("时间格式错误");
        return 0;
    }

    String timeStr = response.substring(start, end);
    LOG_D("获取到时间字符串: " + timeStr);

    // 解析各个时间字段
    struct tm timeinfo = {0};
    int zone;
    if (sscanf(timeStr.c_str(), "%d/%d/%d,%d:%d:%d+%d",
               &timeinfo.tm_year,
               &timeinfo.tm_mon,
               &timeinfo.tm_mday,
               &timeinfo.tm_hour,
               &timeinfo.tm_min,
               &timeinfo.tm_sec,
               &zone) != 7) {
        LOG_E("解析时间失败");
        return 0;
    }

    // 调整年份和月份
    timeinfo.tm_year += 2000 - 1900;  // 年份从1900年开始
    timeinfo.tm_mon -= 1;             // 月份从0开始
    
    // 调整为东八区时间
    timeinfo.tm_hour += 8;  // 直接加8小时
    
    // 处理跨天的情况
    if (timeinfo.tm_hour >= 24) {
        timeinfo.tm_hour -= 24;
        timeinfo.tm_mday += 1;
    }

    // 转换为时间戳
    time_t timestamp = mktime(&timeinfo);
    if (timestamp == -1) {
        LOG_E("转换时间戳失败");
        return 0;
    }

    LOG_I("获取到网络时间戳: " + String(timestamp));

    // 设置系统RTC时间
    struct timeval tv = {.tv_sec = timestamp};
    if (settimeofday(&tv, NULL) != 0) {
        LOG_E("设置系统时间失败");
        return 0;
    }

    // 更新Logger时间
    LOGGER.setTime(timestamp);
    
    LOG_I("系统时间已更新");
    return timestamp;
}

bool Modem::connect(const char *apn, const char *username, const char *password)
{
    static int connectCount = 0; // 连接尝试计数

    // 如果尝试次数超过5次，返回失败
    if (connectCount >= 5)
    {
        LOG_E("连接尝试次数超过5次，放弃连接");
        connectCount = 0;
        return false;
    }

    // 确保在命令模式
    if (!isCommandMode() && !setCommandMode())
    {
        return false;
    }

    // 1. 检查SIM卡状态 (AT+CPIN?)
    LOG_D("检查SIM卡状态");
    String response = sendCommand("AT+CPIN?");
    if (response.indexOf("+CPIN: READY") < 0)
    {
        LOG_E("SIM卡未就绪");
        delay_ms(1000); // 等待1秒后重试
        connectCount++;
        return connect(apn, username, password);
    }

    // 2. 检查网络注册状态 (AT+CREG?)
    LOG_D("检查网络注册状态");
    response = sendCommand("AT+CREG?");
    if (response.indexOf("+CREG: 0,1") < 0 && response.indexOf("+CREG: 0,5") < 0)
    {
        // 如果未注册，等待3秒后重试
        LOG_W("等待3秒进行网络注册");
        delay_ms(3000);
        response = sendCommand("AT+CREG?");
        if (response.indexOf("+CREG: 0,1") < 0 && response.indexOf("+CREG: 0,5") < 0)
        {
            LOG_E("网络注册失败");
            connectCount++;
            return connect(apn, username, password);
        }
    }

    // 网络注册成功后更新时间
    LOG_D("尝试更新网络时间");
    if (getNetworkTime() > 0) {
        LOG_I("网络时间更新成功");
    } else {
        LOG_W("网络时间更新失败");
    }

    // 3. 检查PS网络附着状态 (AT+CGATT?)
    LOG_D("检查PS网络附着状态");
    response = sendCommand("AT+CGATT?");
    if (response.indexOf("+CGATT: 1") < 0)
    {
        // 如果未附着，尝试附着
        LOG_D("尝试PS网络附着");
        response = sendCommand("AT+CGATT=1");
        if (response.indexOf("OK") < 0)
        {
            LOG_E("PS网络附着失败");
            connectCount++;
            return connect(apn, username, password);
        }
        delay_ms(1000); // 等待1秒让PS附着完成
    }

    // 4. 设置PDP上下文 (AT+CGDCONT)
    LOG_D("设置APN");
    String cmd = "AT+CGDCONT=1,\"IP\",\"";
    cmd += apn;
    cmd += "\"";
    response = sendCommand(cmd);
    if (response.indexOf("OK") < 0)
    {
        LOG_E("APN设置失败");
        connectCount++;
        return connect(apn, username, password);
    }

    // 5. 如果有用户名密码，设置认证 (AT+CGAUTH)
    if (strlen(username) > 0)
    {
        cmd = "AT+CGAUTH=1,1,\"";
        cmd += username;
        cmd += "\",\"";
        cmd += password;
        cmd += "\"";
        response = sendCommand(cmd);
        if (response.indexOf("OK") < 0)
        {
            LOG_E("认证信息设置失败");
            connectCount++;
            return connect(apn, username, password);
        }
    }

    // 6. 激活PDP上下文并进行PPP拨号
    LOG_D("开始PPP拨号");
    response = sendCommand("ATD*99#", 15000);
    if (response.indexOf("CONNECT") < 0)
    {
        LOG_E("PPP拨号失败");
        connectCount++;
        return connect(apn, username, password);
    }

    // 拨号成功，设置数据模式标志
    LOG_I("PPP拨号成功");
    return true;
}

bool Modem::hangup()
{
    // 确保在命令模式
    if (!isCommandMode() && !setCommandMode())
    {
        return false;
    }

    // 发送挂断命令
    String response = sendCommand("ATH");
    if (response.indexOf("OK") >= 0 || response.indexOf("NO CARRIER") >= 0)
    {
        return true;
    }
    return false;
}

void Modem::delay_ms(uint32_t ms)
{
    unsigned long start = millis();
    while (millis() - start < ms)
    {
        yield();
    }
}

