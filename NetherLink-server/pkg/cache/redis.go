package cache

import (
	"NetherLink-server/config"
	"context"
	"fmt"
	"log"
	"sync"
	"time"

	"github.com/redis/go-redis/v9"
)

var (
	client     *redis.Client
	once       sync.Once
	ctx        = context.Background()
	cacheMutex sync.RWMutex
)

// InitRedis 初始化Redis连接
func InitRedis() error {
	var err error
	once.Do(func() {
		cfg := config.GlobalConfig.Redis
		client = redis.NewClient(&redis.Options{
			Addr:         fmt.Sprintf("%s:%d", cfg.Host, cfg.Port),
			Password:     cfg.Password,
			DB:           cfg.DB,
			PoolSize:     cfg.PoolSize,
			MaxRetries:   cfg.MaxRetries,
			DialTimeout:  5 * time.Second,
			ReadTimeout:  3 * time.Second,
			WriteTimeout: 3 * time.Second,
		})

		// 测试连接
		_, err = client.Ping(ctx).Result()
		if err != nil {
			log.Printf("Failed to connect to Redis: %v", err)
			return
		}

		log.Println("Redis initialized successfully")
	})
	return err
}

// GetClient 获取Redis客户端
func GetClient() (*redis.Client, error) {
	cacheMutex.RLock()
	defer cacheMutex.RUnlock()

	if client == nil {
		return nil, fmt.Errorf("Redis client is not initialized")
	}
	return client, nil
}

// SetUserOnline 设置用户在线状态
func SetUserOnline(userID, gatewayID string) error {
	client, err := GetClient()
	if err != nil {
		return err
	}

	key := fmt.Sprintf("online:user:%s", userID)
	now := time.Now().Unix()

	return client.HSet(ctx, key, map[string]interface{}{
		"gateway_id":     gatewayID,
		"connected_at":   now,
		"last_heartbeat": now,
	}).Err()
}

// GetUserOnlineStatus 获取用户在线状态
func GetUserOnlineStatus(userID string) (map[string]string, error) {
	client, err := GetClient()
	if err != nil {
		return nil, err
	}

	key := fmt.Sprintf("online:user:%s", userID)
	return client.HGetAll(ctx, key).Result()
}

// IsUserOnline 检查用户是否在线
func IsUserOnline(userID string) (bool, error) {
	client, err := GetClient()
	if err != nil {
		return false, err
	}

	key := fmt.Sprintf("online:user:%s", userID)
	exists, err := client.Exists(ctx, key).Result()
	if err != nil {
		return false, err
	}

	return exists > 0, nil
}

// SetUserOffline 设置用户离线
func SetUserOffline(userID string) error {
	client, err := GetClient()
	if err != nil {
		return err
	}

	key := fmt.Sprintf("online:user:%s", userID)
	return client.Del(ctx, key).Err()
}

// UpdateHeartbeat 更新用户心跳时间
func UpdateHeartbeat(userID string) error {
	client, err := GetClient()
	if err != nil {
		return err
	}

	key := fmt.Sprintf("online:user:%s", userID)
	return client.HSet(ctx, key, "last_heartbeat", time.Now().Unix()).Err()
}

// IncrUnreadCount 增加未读消息计数
func IncrUnreadCount(userID, conversationID string) error {
	client, err := GetClient()
	if err != nil {
		return err
	}

	key := fmt.Sprintf("unread:%s", userID)
	return client.HIncrBy(ctx, key, conversationID, 1).Err()
}

// GetUnreadCount 获取未读消息计数
func GetUnreadCount(userID, conversationID string) (int64, error) {
	client, err := GetClient()
	if err != nil {
		return 0, err
	}

	key := fmt.Sprintf("unread:%s", userID)
	count, err := client.HGet(ctx, key, conversationID).Int64()
	if err == redis.Nil {
		return 0, nil
	}
	return count, err
}

// ClearUnreadCount 清除未读消息计数
func ClearUnreadCount(userID, conversationID string) error {
	client, err := GetClient()
	if err != nil {
		return err
	}

	key := fmt.Sprintf("unread:%s", userID)
	return client.HDel(ctx, key, conversationID).Err()
}

// GetAllUnreadCounts 获取用户所有会话的未读消息计数
func GetAllUnreadCounts(userID string) (map[string]string, error) {
	client, err := GetClient()
	if err != nil {
		return nil, err
	}

	key := fmt.Sprintf("unread:%s", userID)
	return client.HGetAll(ctx, key).Result()
}

// SetConnectionID 设置用户连接ID
func SetConnectionID(userID, connectionID string, ttl time.Duration) error {
	client, err := GetClient()
	if err != nil {
		return err
	}

	key := fmt.Sprintf("connection:%s", userID)
	return client.Set(ctx, key, connectionID, ttl).Err()
}

// GetConnectionID 获取用户连接ID
func GetConnectionID(userID string) (string, error) {
	client, err := GetClient()
	if err != nil {
		return "", err
	}

	key := fmt.Sprintf("connection:%s", userID)
	connID, err := client.Get(ctx, key).Result()
	if err == redis.Nil {
		return "", nil
	}
	return connID, err
}

// Close 关闭Redis连接
func Close() {
	cacheMutex.Lock()
	defer cacheMutex.Unlock()

	if client != nil {
		client.Close()
	}
}
