package main

import (
	"context"
	"log"
	"net/http"
	"oil-depth-detection/amqp"
	"oil-depth-detection/api"
	"oil-depth-detection/models"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
)

func main() {
	// 加载环境变量
	if err := godotenv.Load(); err != nil {
		log.Fatal("Error loading .env file")
	}

	// 初始化数据库
	if err := models.InitDB("app.db"); err != nil {
		log.Fatal(err)
	}

	// 创建 AMQP 客户端
	amqpClient := amqp.NewAmqpClient(
		os.Getenv("CLIENT_ID"),
		os.Getenv("ALIYUN_ACCESS_KEY_ID"),
		os.Getenv("ALIYUN_ACCESS_KEY_SECRET"),
		os.Getenv("CONSUMER_GROUP_ID"),
		os.Getenv("IOT_INSTANCE_ID"),
		os.Getenv("IOT_AMQP_HOST"),
	)

	// 创建上下文和取消函数
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// 连接 AMQP 服务器
	if err := amqpClient.Connect(ctx); err != nil {
		log.Fatal(err)
	}

	// 在后台启动 AMQP 消息接收
	amqpErrChan := make(chan error, 1)
	go func() {
		if err := amqpClient.StartReceive(ctx); err != nil && err != context.Canceled {
			log.Printf("AMQP接收错误: %v", err)
			amqpErrChan <- err
		}
		close(amqpErrChan)
	}()

	// 设置 Gin 路由
	r := gin.Default()

	// 配置 CORS
	config := cors.DefaultConfig()
	config.AllowAllOrigins = true
	config.AllowHeaders = []string{"Origin", "Content-Length", "Content-Type"}
	r.Use(cors.New(config))

	// 设置 API 路由
	api.SetupRoutes(r)

	// 设置静态文件服务
	r.Static("/static", "./static")
	r.NoRoute(func(c *gin.Context) {
		path := c.Request.URL.Path

		// 如果是 API 请求但路由不存在，返回 404
		if strings.HasPrefix(path, "/api/") {
			c.JSON(http.StatusNotFound, gin.H{"error": "API not found"})
			return
		}

		// 对于其他请求，尝试提供静态文件
		if _, err := os.Stat("static" + path); err == nil {
			c.File("static" + path)
			return
		}

		// 如果文件不存在，返回 index.html（用于 SPA 路由）
		c.File("static/index.html")
	})

	// 创建 HTTP 服务器
	srv := &http.Server{
		Addr:    ":8080",
		Handler: r,
	}

	// 在后台启动 HTTP 服务器
	go func() {
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Printf("HTTP服务器错误: %v", err)
		}
	}()

	// 等待中断信号
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("收到关闭信号，正在优雅关闭...")

	// 创建一个用于关闭的上下文，设置超时
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer shutdownCancel()

	// 取消主上下文，触发 AMQP 客户端关闭
	cancel()

	// 关闭 AMQP 客户端
	log.Println("正在关闭 AMQP 客户端...")
	amqpClient.Close()

	// 优雅关闭 HTTP 服务器
	log.Println("正在关闭 HTTP 服务器...")
	if err := srv.Shutdown(shutdownCtx); err != nil {
		log.Printf("HTTP服务器关闭出错: %v", err)
	}

	// 等待 AMQP 错误通道关闭或超时
	select {
	case <-amqpErrChan:
		log.Println("AMQP 客户端已关闭")
	case <-shutdownCtx.Done():
		log.Println("关闭超时")
	}

	log.Println("服务已完全关闭")
}
