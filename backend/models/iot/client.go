package iot

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"os"

	openapi "github.com/alibabacloud-go/darabonba-openapi/v2/client"
	iot20180120 "github.com/alibabacloud-go/iot-20180120/v6/client"
	util "github.com/alibabacloud-go/tea-utils/v2/service"
	"github.com/alibabacloud-go/tea/tea"
)

var Client *iot20180120.Client

// InitIoTClient 初始化阿里云物联网平台客户端
func InitIoTClient() error {
	accessKeyId := os.Getenv("ALIYUN_ACCESS_KEY_ID")
	accessKeySecret := os.Getenv("ALIYUN_ACCESS_KEY_SECRET")
	instanceId := os.Getenv("IOT_INSTANCE_ID")

	config := &openapi.Config{
		AccessKeyId:     &accessKeyId,
		AccessKeySecret: &accessKeySecret,
		RegionId:        tea.String("cn-shanghai"),
		Endpoint:        tea.String("iot.cn-shanghai.aliyuncs.com"),
	}

	var err error
	Client, err = iot20180120.NewClient(config)
	if err != nil {
		return fmt.Errorf("初始化IoT客户端失败: %v", err)
	}

	// 验证实例ID是否存在
	if instanceId == "" {
		return fmt.Errorf("IOT_INSTANCE_ID 环境变量未设置")
	}

	return nil
}

// PublishMessage 发送消息到指定Topic
func PublishMessage(productKey, deviceName string, message interface{}) error {
	// 构建带有 config 外层的消息结构
	wrappedMessage := map[string]interface{}{
		"config": message,
	}

	// 将消息转换为JSON
	messageBytes, err := json.Marshal(wrappedMessage)
	if err != nil {
		return fmt.Errorf("消息序列化失败: %v", err)
	}

	// base64编码
	messageContent := base64.StdEncoding.EncodeToString(messageBytes)

	// 构建请求
	pubRequest := &iot20180120.PubRequest{
		ProductKey:     tea.String(productKey),
		DeviceName:     tea.String(deviceName),
		TopicFullName:  tea.String(fmt.Sprintf("/%s/%s/user/get", productKey, deviceName)),
		MessageContent: tea.String(messageContent),
		IotInstanceId:  tea.String(os.Getenv("IOT_INSTANCE_ID")),
		Qos:            tea.Int32(0), // 添加 QoS 参数
	}

	runtime := &util.RuntimeOptions{}
	_, err = Client.PubWithOptions(pubRequest, runtime)
	if err != nil {
		return fmt.Errorf("发送消息失败: %v", err)
	}

	return nil
}
