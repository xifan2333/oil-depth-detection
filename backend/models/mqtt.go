package models

import (
	"crypto/hmac"
	"crypto/sha1"
	"crypto/tls"
	"encoding/base64"
	"fmt"
	"log"
	"os"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
)

var MQTTClient *MQTTManager

type MQTTManager struct {
	client mqtt.Client
}

// InitMQTT 初始化MQTT客户端
func InitMQTT() error {
	clientID := os.Getenv("CLIENT_ID")
	accessSecret := os.Getenv("ALIYUN_ACCESS_KEY_SECRET")
	host := os.Getenv("IOT_MQTT_HOST")
	productKey := os.Getenv("IOT_PRODUCT_KEY")

	// 构建MQTT连接参数
	timestamp := time.Now().UnixNano() / 1e6
	groupID := os.Getenv("CONSUMER_GROUP_ID")

	// 构建 clientId，格式为: ${clientId}@@@${groupId}
	mqttClientID := fmt.Sprintf("%s@@@%s", clientID, groupID)

	// 构建签名
	signContent := fmt.Sprintf("clientId%sdeviceName%sgroupId%sproductKey%stimestamp%d",
		clientID, clientID, groupID, productKey, timestamp)
	hmacKey := hmac.New(sha1.New, []byte(accessSecret))
	hmacKey.Write([]byte(signContent))
	sign := base64.StdEncoding.EncodeToString(hmacKey.Sum(nil))

	// 构建 username，格式为：
	// ${clientId}|securemode=2,signmethod=hmacsha1,timestamp=${timestamp}|
	userName := fmt.Sprintf("%s|securemode=2,signmethod=hmacsha1,timestamp=%d|",
		clientID, timestamp)

	// MQTT配置
	opts := mqtt.NewClientOptions()
	opts.AddBroker(fmt.Sprintf("ssl://%s:8883", host))
	opts.SetClientID(mqttClientID)
	opts.SetUsername(userName)
	opts.SetPassword(sign)
	opts.SetKeepAlive(60 * time.Second)
	opts.SetAutoReconnect(true)
	opts.SetCleanSession(false) // 持久化会话
	opts.SetProtocolVersion(4)  // 使用 MQTT 3.1.1
	opts.SetOrderMatters(false) // 禁用顺序匹配

	// 设置 TLS 配置
	tlsConfig := &tls.Config{
		InsecureSkipVerify: true, // 跳过服务器证书验证（生产环境应该使用正确的证书）
	}
	opts.SetTLSConfig(tlsConfig)

	// 连接处理
	opts.SetOnConnectHandler(func(client mqtt.Client) {
		log.Printf("MQTT已连接，ClientID: %s", mqttClientID)
	})
	opts.SetConnectionLostHandler(func(client mqtt.Client, err error) {
		log.Printf("MQTT连接断开: %v", err)
	})

	// 创建客户端
	client := mqtt.NewClient(opts)
	token := client.Connect()
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("MQTT连接失败: %v", token.Error())
	}

	MQTTClient = &MQTTManager{
		client: client,
	}

	return nil
}

// Publish 发送消息
func (m *MQTTManager) Publish(topic string, payload []byte) error {
	token := m.client.Publish(topic, 1, false, payload)
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("发送消息失败: %v", token.Error())
	}
	return nil
}

// Close 关闭连接
func (m *MQTTManager) Close() {
	if m.client != nil && m.client.IsConnected() {
		m.client.Disconnect(250)
	}
}
