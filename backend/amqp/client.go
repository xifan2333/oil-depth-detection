package amqp

import (
	"context"
	"crypto/hmac"
	"crypto/sha1"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"log"
	"oil-depth-detection/models"
	"strconv"
	"strings"
	"sync"
	"time"

	"pack.ag/amqp"
)

type AmqpClient struct {
	clientID        string
	accessKey       string
	accessSecret    string
	consumerGroupID string
	iotInstanceID   string
	host            string
	client          *amqp.Client
	session         *amqp.Session
	receiver        *amqp.Receiver
	wg              sync.WaitGroup // 用于追踪正在处理的消息
	stopChan        chan struct{}  // 用于通知消息处理停止
	isClosed        bool           // 标记客户端是否已关闭
	mu              sync.Mutex     // 保护 isClosed 字段
}

func NewAmqpClient(clientID, accessKey, accessSecret, consumerGroupID, iotInstanceID, host string) *AmqpClient {
	// 将 mqtt 域名转换为 amqp 域名

	return &AmqpClient{
		clientID:        clientID,
		accessKey:       accessKey,
		accessSecret:    accessSecret,
		consumerGroupID: consumerGroupID,
		iotInstanceID:   iotInstanceID,
		host:            host,
		stopChan:        make(chan struct{}),
	}
}

func (c *AmqpClient) Connect(ctx context.Context) error {
	address := fmt.Sprintf("amqps://%s:5671", c.host)
	timestamp := time.Now().Nanosecond() / 1000000

	// 构建userName
	userName := fmt.Sprintf("%s|authMode=aksign,signMethod=Hmacsha1,consumerGroupId=%s,authId=%s,iotInstanceId=%s,timestamp=%d|",
		c.clientID, c.consumerGroupID, c.accessKey, c.iotInstanceID, timestamp)

	// 构建签名
	stringToSign := fmt.Sprintf("authId=%s&timestamp=%d", c.accessKey, timestamp)
	hmacKey := hmac.New(sha1.New, []byte(c.accessSecret))
	hmacKey.Write([]byte(stringToSign))
	password := base64.StdEncoding.EncodeToString(hmacKey.Sum(nil))

	log.Printf("Connecting to %s with username: %s", address, userName)

	var err error
	c.client, err = amqp.Dial(address, amqp.ConnSASLPlain(userName, password))
	if err != nil {
		return fmt.Errorf("连接AMQP服务器失败: %v", err)
	}

	c.session, err = c.client.NewSession()
	if err != nil {
		c.client.Close()
		return fmt.Errorf("创建AMQP会话失败: %v", err)
	}

	c.receiver, err = c.session.NewReceiver(
		amqp.LinkSourceAddress("default"), // 使用默认队列名
		amqp.LinkCredit(20),
	)
	if err != nil {
		c.session.Close(ctx)
		c.client.Close()
		return fmt.Errorf("创建接收器失败: %v", err)
	}

	return nil
}

func (c *AmqpClient) Close() {
	c.mu.Lock()
	if c.isClosed {
		c.mu.Unlock()
		return
	}
	c.isClosed = true

	// 保存当前连接的本地副本
	receiver := c.receiver
	session := c.session
	client := c.client

	// 立即清空原始引用
	c.receiver = nil
	c.session = nil
	c.client = nil

	c.mu.Unlock()

	log.Println("正在关闭AMQP连接...")

	// 通知停止接收新消息
	close(c.stopChan)

	// 使用带超时的上下文快速关闭
	ctx, cancel := context.WithTimeout(context.Background(), 1*time.Second)
	defer cancel()

	// 快速关闭所有连接
	if receiver != nil {
		_ = receiver.Close(ctx)
	}
	if session != nil {
		_ = session.Close(ctx)
	}
	if client != nil {
		_ = client.Close()
	}

	log.Println("AMQP连接已关闭")
}

func (c *AmqpClient) StartReceive(ctx context.Context) error {
	errChan := make(chan error, 1)

	go func() {
		for {
			select {
			case <-ctx.Done():
				log.Println("收到停止信号，正在退出消息接收...")
				errChan <- nil
				return
			case <-c.stopChan:
				log.Println("收到关闭信号，停止接收新消息")
				errChan <- nil
				return
			default:
				msg, err := c.receiver.Receive(ctx)
				if err != nil {
					if ctx.Err() != nil || c.isClosed {
						errChan <- nil
						return
					}
					log.Printf("接收消息失败: %v", err)
					if err := c.Connect(ctx); err != nil {
						errChan <- err
						return
					}
					continue
				}

				// 直接处理消息，不使用等待组
				go func(msg *amqp.Message) {
					if err := c.processMessage(msg); err != nil {
						log.Printf("处理消息失败: %v", err)
					}
					msg.Accept()
				}(msg)
			}
		}
	}()

	select {
	case err := <-errChan:
		return err
	case <-ctx.Done():
		return ctx.Err()
	}
}

// 定义消息结构
type LocationMessage struct {
	Longitude string `json:"longitude"`
	Latitude  string `json:"latitude"`
	Type      string `json:"type"`
	Timestamp int64  `json:"timestamp"`
}

// DeviceStatusMessage 设备状态消息结构（包含油位、配置等信息）
type DeviceStatusMessage struct {
	OilLevel       float64 `json:"oilLevel"`       // 当前油位高度
	Confidence     float64 `json:"confidence"`     // 测量置信度
	Timestamp      int64   `json:"timestamp"`      // 测量时间戳
	LowLevelAlert  float64 `json:"lowLevelAlert"`  // 低油位警报阈值
	HighLevelAlert float64 `json:"highLevelAlert"` // 高油位警报阈值
	TankHeight     float64 `json:"tankHeight"`     // 油罐总高度
}

func (c *AmqpClient) processMessage(msg *amqp.Message) error {
	data := msg.GetData()
	rawData := string(data)
	log.Printf("原始消息数据: %s", rawData)
	log.Printf("消息属性: %+v", msg.ApplicationProperties)

	// 从topic中提取设备ID
	topic, ok := msg.ApplicationProperties["topic"].(string)
	if !ok {
		return fmt.Errorf("消息缺少topic属性")
	}

	// 获取消息生成时间
	generateTime, ok := msg.ApplicationProperties["generateTime"].(int64)
	if !ok {
		// 如果没有generateTime，使用当前时间
		generateTime = time.Now().UnixMilli()
	}
	messageTime := time.UnixMilli(generateTime)

	// topic格式: /k277nFFa4zB/868488079671754/user/update
	parts := strings.Split(topic, "/")
	if len(parts) < 3 {
		return fmt.Errorf("无效的topic格式: %s", topic)
	}
	deviceID := parts[2] // 获取设备ID
	log.Printf("设备ID: %s, 消息时间: %v", deviceID, messageTime)

	// 尝试解析为位置消息
	var locMsg LocationMessage
	if err := json.Unmarshal([]byte(rawData), &locMsg); err == nil && locMsg.Type != "" {
		// 转换经纬度字符串为float64
		lon, err := strconv.ParseFloat(locMsg.Longitude, 64)
		if err != nil {
			return fmt.Errorf("解析经度失败: %v, 原始值: %s", err, locMsg.Longitude)
		}
		lat, err := strconv.ParseFloat(locMsg.Latitude, 64)
		if err != nil {
			return fmt.Errorf("解析纬度失败: %v, 原始值: %s", err, locMsg.Latitude)
		}

		log.Printf("收到%s类型的位置消息: 经度=%.6f, 纬度=%.6f", locMsg.Type, lon, lat)

		// 保存位置信息
		location := &models.Location{
			DeviceID:  deviceID,
			Longitude: lon,
			Latitude:  lat,
			Type:      locMsg.Type,
			Timestamp: messageTime,
		}
		if err := models.SaveLocation(location); err != nil {
			return fmt.Errorf("保存位置信息失败: %v", err)
		}

		// 确保设备存在并更新状态
		if err := models.CreateDeviceIfNotExists(deviceID); err != nil {
			log.Printf("创建设备记录失败: %v", err)
		}
		if err := models.UpdateDeviceStatus(deviceID, models.DeviceStatusOnline); err != nil {
			log.Printf("更新设备状态失败: %v", err)
		}
		return nil
	}

	// 尝试解析为设备状态消息
	var statusMsg DeviceStatusMessage
	if err := json.Unmarshal([]byte(rawData), &statusMsg); err == nil && statusMsg.OilLevel != 0 {
		log.Printf("收到设备状态消息: 油位=%.2f, 置信度=%.2f, 油罐高度=%.2f",
			statusMsg.OilLevel, statusMsg.Confidence, statusMsg.TankHeight)

		// 保存油量数据
		oilData := &models.OilLevel{
			DeviceID:   deviceID,
			Level:      statusMsg.OilLevel,
			Confidence: statusMsg.Confidence,
			Timestamp:  messageTime,
		}
		if err := models.SaveOilLevel(oilData); err != nil {
			return fmt.Errorf("保存油量数据失败: %v", err)
		}

		// 更新设备配置信息
		if err := models.UpdateDeviceConfig(deviceID, statusMsg.LowLevelAlert, statusMsg.HighLevelAlert, statusMsg.TankHeight); err != nil {
			log.Printf("更新设备配置失败: %v", err)
		}

		// 确保设备存在并更新状态
		if err := models.CreateDeviceIfNotExists(deviceID); err != nil {
			log.Printf("创建设备记录失败: %v", err)
		}
		if err := models.UpdateDeviceStatus(deviceID, models.DeviceStatusOnline); err != nil {
			log.Printf("更新设备状态失败: %v", err)
		}
		return nil
	}

	return fmt.Errorf("未知的消息格式: %s", rawData)
}
