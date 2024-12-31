package api

import (
	"fmt"
	"log"
	"net/http"
	"oil-depth-detection/models"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
)

// 设备数据响应结构
type DeviceResponse struct {
	DeviceID     string    `json:"device_id"`
	Status       string    `json:"status"`
	LastSeen     time.Time `json:"last_seen"`
	OilLevel     float64   `json:"oil_level"`
	Confidence   float64   `json:"confidence"`
	Longitude    float64   `json:"longitude"`
	Latitude     float64   `json:"latitude"`
	LocationType string    `json:"location_type"`
}

// DeviceConfig 设备配置结构
type DeviceConfig struct {
	TankHeight     float64 `json:"tankHeight"`
	SensorOffset   float64 `json:"sensorOffset"`
	SampleInterval int     `json:"sampleInterval"`
	SampleCount    int     `json:"sampleCount"`
	WindowSize     int     `json:"windowSize"`
	StdThreshold   float64 `json:"stdThreshold"`
	LowLevelAlert  float64 `json:"lowLevelAlert"`
	HighLevelAlert float64 `json:"highLevelAlert"`
	DeviceName     string  `json:"deviceName"`
	WifiSSID       string  `json:"wifiSSID"`
	WifiPassword   string  `json:"wifiPassword"`
}

// ConfigRequest 配置请求结构
type ConfigRequest struct {
	Config DeviceConfig `json:"config"`
}

// GetDevices 获取所有设备列表
func GetDevices(c *gin.Context) {
	devices, err := models.GetAllDevices()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, devices)
}

// GetDeviceStatus 获取设备最新状态
func GetDeviceStatus(c *gin.Context) {
	deviceID := c.Param("device_id")
	if deviceID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "设备ID不能为空"})
		return
	}

	data, err := models.GetDeviceData(deviceID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "设备不存在或无数据"})
		return
	}

	c.JSON(http.StatusOK, data)
}

// GetDeviceHistory 获取设备历史数据
func GetDeviceHistory(c *gin.Context) {
	deviceID := c.Param("device_id")
	if deviceID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "设备ID不能为空"})
		return
	}

	// 获取查询参数
	limitStr := c.DefaultQuery("limit", "100")
	limit, err := strconv.Atoi(limitStr)
	if err != nil || limit <= 0 {
		limit = 100
	}

	startTimeStr := c.Query("start_time")
	endTimeStr := c.Query("end_time")

	var startTime, endTime time.Time
	if startTimeStr != "" {
		startTime, err = time.Parse(time.RFC3339, startTimeStr)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "开始时间格式错误"})
			return
		}
	}
	if endTimeStr != "" {
		endTime, err = time.Parse(time.RFC3339, endTimeStr)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "结束时间格式错误"})
			return
		}
	}

	history, err := models.GetDeviceHistory(deviceID, limit, startTime, endTime)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, history)
}

// ConfigDevice 配置设备
func ConfigDevice(c *gin.Context) {
	deviceID := c.Param("device_id")
	if deviceID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "设备ID不能为空"})
		return
	}

	var config DeviceConfig
	if err := c.ShouldBindJSON(&config); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的请求数据"})
		return
	}

	// 构建MQTT消息
	topic := fmt.Sprintf("/k277nFFa4zB/%s/user/get", deviceID)
	if err := models.PublishDeviceConfig(topic, config); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": fmt.Sprintf("发送配置失败: %v", err)})
		return
	}

	// 更新数据库中的配置
	if err := models.UpdateDeviceConfig(deviceID, config.LowLevelAlert, config.HighLevelAlert, config.TankHeight); err != nil {
		log.Printf("更新设备配置失败: %v", err)
	}

	c.JSON(http.StatusOK, gin.H{"message": "配置已发送，请重启设备"})
}

// ConfigureDevice 配置设备
func ConfigureDevice(c *gin.Context) {
	deviceID := c.Param("id")
	var config struct {
		TankHeight     float64 `json:"tankHeight"`
		SensorOffset   float64 `json:"sensorOffset"`
		SampleInterval int     `json:"sampleInterval"`
		SampleCount    int     `json:"sampleCount"`
		WindowSize     int     `json:"windowSize"`
		StdThreshold   float64 `json:"stdThreshold"`
		LowLevelAlert  float64 `json:"lowLevelAlert"`
		HighLevelAlert float64 `json:"highLevelAlert"`
		DeviceName     string  `json:"deviceName"`
		WifiSSID       string  `json:"wifiSSID"`
		WifiPassword   string  `json:"wifiPassword"`
	}

	if err := c.ShouldBindJSON(&config); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	topic := fmt.Sprintf("/k277nFFa4zB/%s/user/get", deviceID)
	if err := models.PublishDeviceConfig(topic, config); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"status": "ok", "message": "配置已发送，请重启设备"})
}
