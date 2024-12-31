package api

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// SetupRoutes 设置API路由
func SetupRoutes(r *gin.Engine) {
	// 健康检查
	r.GET("/api/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"status": "ok",
		})
	})

	// API 路由组
	api := r.Group("/api")
	{
		// 设备相关路由
		api.GET("/devices", GetDevices)
		api.GET("/devices/:device_id", GetDeviceStatus)
		api.GET("/devices/:device_id/history", GetDeviceHistory)
		api.POST("/devices/:device_id/config", ConfigDevice)
	}
}
