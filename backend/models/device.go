package models

import (
	"fmt"
	"oil-depth-detection/models/iot"
	"strconv"
	"strings"
	"time"

	"gorm.io/gorm"
)

// 设备状态枚举
type DeviceStatus string

const (
	DeviceStatusOnline  DeviceStatus = "online"
	DeviceStatusOffline DeviceStatus = "offline"
)

// Device 设备基本信息
type Device struct {
	gorm.Model
	DeviceID       string       `gorm:"primaryKey"`
	Status         DeviceStatus `gorm:"type:string;default:'offline'"`
	LowLevelAlert  float64      `gorm:"default:0" json:"low_level_alert"`
	HighLevelAlert float64      `gorm:"default:0" json:"high_level_alert"`
	TankHeight     float64      `gorm:"default:0" json:"tank_height"`
	CreatedAt      time.Time
	UpdatedAt      time.Time
}

// Location 位置信息
type Location struct {
	gorm.Model
	DeviceID  string    `gorm:"index;not null" json:"device_id"`
	Longitude float64   `gorm:"not null" json:"longitude"`
	Latitude  float64   `gorm:"not null" json:"latitude"`
	Type      string    `gorm:"not null" json:"type"` // gps 或 lbs
	Timestamp time.Time `gorm:"not null" json:"timestamp"`
}

// OilLevel 油量数据
type OilLevel struct {
	gorm.Model `json:"-"`
	DeviceID   string    `gorm:"index;not null" json:"-"`
	Level      float64   `gorm:"not null" json:"oil_level"`
	Distance   float64   `gorm:"not null" json:"distance"`
	Confidence float64   `gorm:"not null" json:"confidence"`
	Timestamp  time.Time `gorm:"not null" json:"timestamp"`
}

// DeviceData 设备完整数据（用于API响应）
type DeviceData struct {
	DeviceID       string       `json:"device_id"`
	Status         DeviceStatus `json:"status"`
	Longitude      float64      `json:"longitude"`
	Latitude       float64      `json:"latitude"`
	LocationType   string       `json:"location_type"`
	OilLevel       float64      `json:"oil_level"`
	Distance       float64      `json:"distance"`
	Confidence     float64      `json:"confidence"`
	RemainingOil   float64      `json:"remaining_oil"`
	LowLevelAlert  float64      `json:"low_level_alert"`
	HighLevelAlert float64      `json:"high_level_alert"`
	TankHeight     float64      `json:"tank_height"`
	UpdateAt       int64        `json:"update_at"`
}

// OilLevelHistory 油位历史记录
type OilLevelHistory struct {
	OilLevel     float64   `json:"oil_level"`
	Confidence   float64   `json:"confidence"`
	RemainingOil float64   `json:"remaining_oil"`
	Timestamp    time.Time `json:"timestamp"`
}

// HistoryData 历史数据响应结构
type HistoryData struct {
	DeviceID       string            `json:"device_id"`
	Longitude      float64           `json:"longitude"`
	Latitude       float64           `json:"latitude"`
	LocationType   string            `json:"location_type"`
	LowLevelAlert  float64           `json:"low_level_alert"`
	HighLevelAlert float64           `json:"high_level_alert"`
	TankHeight     float64           `json:"tank_height"`
	History        []OilLevelHistory `json:"history"`
}

// CreateDeviceIfNotExists 如果设备不存在则创建
func CreateDeviceIfNotExists(deviceID string) error {
	var device Device
	result := DB.Where("device_id = ?", deviceID).First(&device)
	if result.Error == gorm.ErrRecordNotFound {
		device = Device{
			DeviceID: deviceID,
			Status:   DeviceStatusOffline,
		}
		return DB.Create(&device).Error
	}
	return result.Error
}

// UpdateDeviceStatus 更新设备状态
func UpdateDeviceStatus(deviceID string, status DeviceStatus) error {
	return DB.Model(&Device{}).Where("device_id = ?", deviceID).Update("status", status).Error
}

// getLatestLocation 获取最新位置数据，优先使用GPS
func getLatestLocation(deviceID string) (*Location, error) {
	// 先尝试获取GPS定位
	var gpsLocation Location
	gpsErr := DB.Where("device_id = ? AND type = ?", deviceID, "gps").
		Order("timestamp desc").
		First(&gpsLocation).Error

	if gpsErr != gorm.ErrRecordNotFound {
		return &gpsLocation, gpsErr
	}

	// 如果没有GPS定位，则获取LBS定位
	var lbsLocation Location
	lbsErr := DB.Where("device_id = ? AND type = ?", deviceID, "lbs").
		Order("timestamp desc").
		First(&lbsLocation).Error

	if lbsErr != gorm.ErrRecordNotFound {
		return &lbsLocation, lbsErr
	}

	return nil, gorm.ErrRecordNotFound
}

// GetDeviceData 获取设备最新数据
func GetDeviceData(deviceID string) (*DeviceData, error) {
	var device Device
	if err := DB.Where("device_id = ?", deviceID).First(&device).Error; err != nil {
		return nil, err
	}

	// 获取最新油量数据
	var oilLevel OilLevel
	oilErr := DB.Where("device_id = ?", deviceID).Order("timestamp desc").First(&oilLevel).Error

	// 获取最新位置数据，优先GPS
	location, _ := getLatestLocation(deviceID)

	// 组合数据
	data := &DeviceData{
		DeviceID:       deviceID,
		Status:         device.Status,
		LowLevelAlert:  device.LowLevelAlert,
		HighLevelAlert: device.HighLevelAlert,
		TankHeight:     device.TankHeight,
		UpdateAt:       device.UpdatedAt.Unix(),
	}

	if oilErr != gorm.ErrRecordNotFound {
		data.OilLevel = oilLevel.Level
		data.Distance = oilLevel.Distance
		data.Confidence = oilLevel.Confidence
		// 计算余量，转换为百分比(0-100)
		if device.TankHeight > 0 {
			data.RemainingOil = (oilLevel.Level / device.TankHeight) * 100
			data.RemainingOil, _ = strconv.ParseFloat(fmt.Sprintf("%.1f", data.RemainingOil), 64)
		}
		// 使用最新的油位数据时间作为更新时间
		data.UpdateAt = oilLevel.Timestamp.Unix()
	}

	if location != nil {
		data.Longitude = location.Longitude
		data.Latitude = location.Latitude
		data.LocationType = location.Type
	}

	return data, nil
}

// GetAllDevices 获取所有设备列表及其最新数据
func GetAllDevices() ([]DeviceData, error) {
	var devices []Device
	if err := DB.Find(&devices).Error; err != nil {
		return nil, err
	}

	var result []DeviceData
	for _, device := range devices {
		// 获取最新油量数据
		var oilLevel OilLevel
		oilErr := DB.Where("device_id = ?", device.DeviceID).Order("timestamp desc").First(&oilLevel).Error

		// 获取最新位置数据，优先GPS
		location, _ := getLatestLocation(device.DeviceID)

		// 组合数据
		data := DeviceData{
			DeviceID:       device.DeviceID,
			Status:         device.Status,
			LowLevelAlert:  device.LowLevelAlert,
			HighLevelAlert: device.HighLevelAlert,
			TankHeight:     device.TankHeight,
			UpdateAt:       device.UpdatedAt.Unix(),
		}

		if oilErr != gorm.ErrRecordNotFound {
			data.OilLevel = oilLevel.Level
			data.Confidence = oilLevel.Confidence
			// 计算余量，转换为百分比(0-100)
			if device.TankHeight > 0 {
				data.RemainingOil = (oilLevel.Level / device.TankHeight) * 100
				data.RemainingOil, _ = strconv.ParseFloat(fmt.Sprintf("%.1f", data.RemainingOil), 64)
			}
			// 使用最新的油位数据时间作为更新时间
			data.UpdateAt = oilLevel.Timestamp.Unix()
		}

		if location != nil {
			data.Longitude = location.Longitude
			data.Latitude = location.Latitude
			data.LocationType = location.Type
		}

		result = append(result, data)
	}

	return result, nil
}

// GetDeviceHistory 获取设备历史数据
func GetDeviceHistory(deviceID string, limit int, startTime, endTime time.Time) (*HistoryData, error) {
	// 获取设备基本信息
	var device Device
	if err := DB.Where("device_id = ?", deviceID).First(&device).Error; err != nil {
		return nil, err
	}

	// 获取最新位置数据，优先GPS
	location, _ := getLatestLocation(deviceID)

	// 查询油量历史
	var oilLevels []OilLevel
	oilQuery := DB.Where("device_id = ?", deviceID)
	if !startTime.IsZero() {
		oilQuery = oilQuery.Where("timestamp >= ?", startTime)
	}
	if !endTime.IsZero() {
		oilQuery = oilQuery.Where("timestamp <= ?", endTime)
	}
	// 始终使用正序（从早到晚）
	if err := oilQuery.Order("timestamp asc").Limit(limit).Find(&oilLevels).Error; err != nil {
		return nil, err
	}

	// 转换历史数据
	history := make([]OilLevelHistory, 0)
	for _, oil := range oilLevels {
		var remaining float64
		if device.TankHeight > 0 {
			remaining = (oil.Level / device.TankHeight) * 100
			remaining, _ = strconv.ParseFloat(fmt.Sprintf("%.1f", remaining), 64)
		}
		history = append(history, OilLevelHistory{
			OilLevel:     oil.Level,
			Confidence:   oil.Confidence,
			RemainingOil: remaining,
			Timestamp:    oil.Timestamp,
		})
	}

	// 构建响应数据
	result := &HistoryData{
		DeviceID:       deviceID,
		LowLevelAlert:  device.LowLevelAlert,
		HighLevelAlert: device.HighLevelAlert,
		TankHeight:     device.TankHeight,
		History:        history,
	}

	if location != nil {
		result.Longitude = location.Longitude
		result.Latitude = location.Latitude
		result.LocationType = location.Type
	}

	return result, nil
}

// UpdateDeviceConfig 更新设备配置信息
func UpdateDeviceConfig(deviceID string, lowLevelAlert, highLevelAlert, tankHeight float64) error {
	return DB.Model(&Device{}).Where("device_id = ?", deviceID).Updates(map[string]interface{}{
		"low_level_alert":  lowLevelAlert,
		"high_level_alert": highLevelAlert,
		"tank_height":      tankHeight,
	}).Error
}

// PublishDeviceConfig 发送设备配置
func PublishDeviceConfig(topic string, config interface{}) error {
	// 从 topic 中提取 productKey 和 deviceName
	// topic 格式: /k277nFFa4zB/868488079671754/user/get
	parts := strings.Split(topic, "/")
	if len(parts) < 4 {
		return fmt.Errorf("无效的topic格式: %s", topic)
	}
	productKey := parts[1]
	deviceName := parts[2]

	// 使用 IoT 客户端发送配置
	if err := iot.PublishMessage(productKey, deviceName, config); err != nil {
		return fmt.Errorf("发送配置失败: %v", err)
	}

	return nil
}

// GetOnlineDevices 获取所有在线设备
func GetOnlineDevices() ([]Device, error) {
	var devices []Device
	err := DB.Where("status = ?", DeviceStatusOnline).Find(&devices).Error
	return devices, err
}

// GetDeviceLastUpdate 获取设备最后一次更新时间
func GetDeviceLastUpdate(deviceID string) (time.Time, error) {
	// 获取最新的油位数据时间
	var oilLevel OilLevel
	err := DB.Where("device_id = ?", deviceID).
		Order("timestamp desc").
		First(&oilLevel).Error

	if err != nil && err != gorm.ErrRecordNotFound {
		return time.Time{}, err
	}

	// 获取最新的位置数据时间
	var location Location
	locErr := DB.Where("device_id = ?", deviceID).
		Order("timestamp desc").
		First(&location).Error

	if locErr != nil && locErr != gorm.ErrRecordNotFound {
		return time.Time{}, locErr
	}

	// 如果两种数据都没有，返回错误
	if err == gorm.ErrRecordNotFound && locErr == gorm.ErrRecordNotFound {
		return time.Time{}, gorm.ErrRecordNotFound
	}

	// 返回最新的时间
	if err == gorm.ErrRecordNotFound {
		return location.Timestamp, nil
	}
	if locErr == gorm.ErrRecordNotFound {
		return oilLevel.Timestamp, nil
	}

	// 返回最新的时间
	if oilLevel.Timestamp.After(location.Timestamp) {
		return oilLevel.Timestamp, nil
	}
	return location.Timestamp, nil
}
