/*
 * 调制解调器接口定义
 */
#pragma once

#include <Arduino.h>
#include <NetworkInterface.h>
#include "logger.h" // 添加logger头文件
#include <lwip/opt.h>
#include <lwip/sys.h>
#include <lwip/netif.h>
#include <netif/ppp/pppapi.h>
#include <netif/ppp/pppos.h>
#include <esp_netif.h>

class Modem
{
public:
    Modem();
    ~Modem();

    /**
     * 初始化调制解调器
     * @param uart 串口对象
     * @return 是否初始化成功
     */
    bool begin(HardwareSerial &uart);

    /**
     * 检查调制解调器是否就绪
     * @return 是否就绪
     */
    bool isReady();

    /**
     * 获取模块IMEI号
     * @return IMEI号字符串，获取失败返回空字符串
     */
    String getIMEI();

    /**
     * 从网络获取时间并更新RTC
     * @return 获取到的时间戳，失败返回0
     */
    time_t getNetworkTime();

    /**
     * 检查当前是否处于命令模式
     * @return true: 命令模式, false: 数据模式
     */
    bool isCommandMode();

    /**
     * 切换到命令模式
     * @return 是否切换成功
     */
    bool setCommandMode();

    /**
     * 切换到数据模式
     * @return 是否切换成功
     */
    bool setDataMode();

    /**
     * 发送AT指令并等待响应
     * @param command AT指令
     * @param timeout 超时时间(ms)
     * @return 调制解调器返回的响应字符串
     */
    String sendCommand(const String &command, uint32_t timeout = 1000);

    /**
     * 进行PPP拨号
     * @param apn APN名称
     * @param username 用户名(可选)
     * @param password 密码(可选)
     * @return 是否拨号成功
     */
    bool connect(const char *apn, const char *username = "", const char *password = "");

    /**
     * 断开PPP连接
     * @return 是否断开成功
     */
    bool hangup();

    /**
     * 检查PPP连接状态
     * @return 是否连接正常且已分配IP地址
     */
    bool checkPPPStatus();

private:
    HardwareSerial *_uart;    // 串口对象指针
    bool _initialized;        // 初始化标志

    // PPP相关成员
    ppp_pcb *_ppp_pcb;       // 改名为_ppp_pcb以避免混淆
    struct netif _ppp_netif;  // PPP网络接口
    bool _ppp_connected;      // PPP连接状态
    
    // PPP相关方法
    static u32_t _pppOutputCallback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx);
    static void _pppLinkStatusCallback(ppp_pcb *pcb, int err_code, void *ctx);
    void _pppInputTask();
    bool _initPPP();
    void _cleanupPPP();

    /**
     * 清空串口缓冲区
     */
    void flushInput();

    /**
     * 等待指定时间
     * @param ms 等待时间(毫秒)
     */
    void delay_ms(uint32_t ms);
};

extern Modem modem;
