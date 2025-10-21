package main

import (
	"NetherLink-server/config"
	"NetherLink-server/internal/server"
	"NetherLink-server/internal/service"
	"NetherLink-server/pkg/cache"
	"NetherLink-server/pkg/database"
	"NetherLink-server/pkg/mq"
	"log"

	"github.com/gin-gonic/gin"
	"golang.org/x/sync/errgroup"
)

func main() {

	if err := config.Init(); err != nil {
		log.Fatal("Failed to load config:", err)
	}

	if _, err := database.GetDB(); err != nil {
		log.Fatal("Failed to connect to database:", err)
	}

	// 初始化Redis
	if err := cache.InitRedis(); err != nil {
		log.Fatal("Failed to connect to Redis:", err)
	}
	defer cache.Close()

	// 初始化RabbitMQ
	if err := mq.InitRabbitMQ(); err != nil {
		log.Fatal("Failed to connect to RabbitMQ:", err)
	}
	defer mq.Close()

	engine := gin.Default()

	httpServer := server.NewHTTPServer(engine)

	wsServer := server.NewWSServer()

	// 启动消息处理服务
	persistenceService := service.NewMessagePersistenceService()
	if err := persistenceService.Start(); err != nil {
		log.Fatal("Failed to start message persistence service:", err)
	}

	dispatchService := service.NewMessageDispatchService(wsServer)
	if err := dispatchService.Start(); err != nil {
		log.Fatal("Failed to start message dispatch service:", err)
	}

	pushService := service.NewPushService()
	if err := pushService.Start(); err != nil {
		log.Fatal("Failed to start push service:", err)
	}

	var g errgroup.Group

	g.Go(func() error {
		return httpServer.Run()
	})

	g.Go(func() error {
		return wsServer.Run()
	})

	if err := g.Wait(); err != nil {
		log.Fatal("Server error:", err)
	}
}
