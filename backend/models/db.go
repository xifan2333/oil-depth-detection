package models

import (
	"log"
	"time"

	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

var DB *gorm.DB

// InitDB 初始化数据库连接
func InitDB(dbPath string) error {
	var err error

	// 配置GORM日志
	logConfig := logger.Config{
		SlowThreshold:             time.Second,
		LogLevel:                  logger.Info,
		IgnoreRecordNotFoundError: true,
		Colorful:                  true,
	}

	// 打开数据库连接
	DB, err = gorm.Open(sqlite.Open(dbPath), &gorm.Config{
		Logger: logger.New(log.Default(), logConfig),
	})
	if err != nil {
		return err
	}

	// 自动迁移数据库结构
	err = DB.AutoMigrate(
		&Device{},
		&Location{},
		&OilLevel{},
	)
	if err != nil {
		return err
	}

	return nil
}

// GetDeviceData 获取设备最新数据

// SaveLocation 保存位置信息
func SaveLocation(location *Location) error {
	return DB.Create(location).Error
}

// SaveOilLevel 保存油量数据
func SaveOilLevel(oilLevel *OilLevel) error {
	return DB.Create(oilLevel).Error
}

// CreateDeviceIfNotExists 如果设备不存在则创建
