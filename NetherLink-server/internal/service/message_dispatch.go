package service

import (
	"NetherLink-server/config"
	"NetherLink-server/internal/server"
	"NetherLink-server/pkg/cache"
	"NetherLink-server/pkg/database"
	"NetherLink-server/pkg/mq"
	"encoding/json"
	"fmt"
	"log"
	"time"
)

// MessageDispatchService 消息分发服务
type MessageDispatchService struct {
	wsServer *server.WSServer
}

// NewMessageDispatchService 创建消息分发服务
func NewMessageDispatchService(wsServer *server.WSServer) *MessageDispatchService {
	return &MessageDispatchService{
		wsServer: wsServer,
	}
}

// Start 启动消息分发服务
func (s *MessageDispatchService) Start() error {
	log.Println("Starting Message Dispatch Service...")

	messages, err := mq.ConsumeMessages(config.GlobalConfig.RabbitMQ.Queue.OfflineMessages)
	if err != nil {
		return fmt.Errorf("failed to consume messages: %v", err)
	}

	go func() {
		log.Println("Message Dispatch Service goroutine started, waiting for messages...")
		for msg := range messages {
			log.Printf("Dispatch service received message, size: %d bytes", len(msg.Body))
			if err := s.processMessage(msg.Body); err != nil {
				log.Printf("Failed to dispatch message: %v", err)
				msg.Nack(false, true)
			} else {
				log.Printf("Message dispatched successfully")
				msg.Ack(false)
			}
		}
		log.Println("Message Dispatch Service goroutine stopped")
	}()

	log.Println("Message Dispatch Service started successfully")
	return nil
}

// processMessage 处理消息分发
func (s *MessageDispatchService) processMessage(body []byte) error {
	log.Printf("Dispatching message: %s", string(body))

	var payload MessagePayload
	if err := json.Unmarshal(body, &payload); err != nil {
		return fmt.Errorf("failed to unmarshal message: %v", err)
	}

	log.Printf("Dispatch - From: %s, To: %s, Content: %s",
		payload.SenderID, payload.ReceiverID, payload.Content)

	// 检查接收者是否在线
	online, err := cache.IsUserOnline(payload.ReceiverID)
	if err != nil {
		log.Printf("Failed to check user online status: %v", err)
		// 如果Redis出错，假设用户离线
		online = false
	}

	log.Printf("Receiver %s online status: %v", payload.ReceiverID, online)

	if online {
		// 用户在线，实时推送消息
		if err := s.sendToOnlineUser(payload); err != nil {
			log.Printf("Failed to send to online user: %v", err)
			// 如果推送失败，转为离线消息处理
			return s.handleOfflineMessage(payload)
		}
	} else {
		// 用户离线，处理离线消息
		return s.handleOfflineMessage(payload)
	}

	return nil
}

// sendToOnlineUser 发送消息给在线用户
func (s *MessageDispatchService) sendToOnlineUser(payload MessagePayload) error {
	// 构建WebSocket消息
	chatResponse := server.ChatResponse{
		Success:      true,
		Message:      "新消息",
		MessageID:    payload.MessageID,
		From:         payload.SenderID,
		Content:      payload.Content,
		Type:         payload.MessageType,
		Extra:        payload.Extra,
		Timestamp:    payload.Timestamp,
		Conversation: payload.ConversationID,
		IsGroup:      payload.IsGroup,
	}

	responseData, err := json.Marshal(chatResponse)
	if err != nil {
		return fmt.Errorf("failed to marshal chat response: %v", err)
	}

	wsMessage := server.WSMessage{
		Type:    "chat",
		Payload: responseData,
	}

	wsMessageBytes, err := json.Marshal(wsMessage)
	if err != nil {
		return fmt.Errorf("failed to marshal ws message: %v", err)
	}

	// 通过WebSocket服务器发送消息
	if err := s.wsServer.SendMessage(payload.ReceiverID, wsMessageBytes); err != nil {
		return fmt.Errorf("failed to send message via websocket: %v", err)
	}

	log.Printf("Message %d sent to online user %s", payload.MessageID, payload.ReceiverID)
	return nil
}

// handleOfflineMessage 处理离线消息
func (s *MessageDispatchService) handleOfflineMessage(payload MessagePayload) error {
	db, err := database.GetDB()
	if err != nil {
		return fmt.Errorf("failed to get database: %v", err)
	}

	// 1. 增加未读计数（Redis）
	if err := cache.IncrUnreadCount(payload.ReceiverID, payload.ConversationID); err != nil {
		log.Printf("Failed to increment unread count: %v", err)
	}

	// 2. 记录离线消息索引
	type OfflineMessageIndex struct {
		ID             int64      `gorm:"column:id;primary_key;auto_increment"`
		UserID         string     `gorm:"column:user_id"`
		MessageID      int64      `gorm:"column:message_id"`
		ConversationID string     `gorm:"column:conversation_id"`
		CreatedAt      time.Time  `gorm:"column:created_at"`
		Synced         bool       `gorm:"column:synced"`
		SyncedAt       *time.Time `gorm:"column:synced_at"`
	}

	offlineMsg := OfflineMessageIndex{
		UserID:         payload.ReceiverID,
		MessageID:      payload.MessageID,
		ConversationID: payload.ConversationID,
		CreatedAt:      time.Now(),
		Synced:         false,
	}

	if err := db.Table("offline_message_index").Create(&offlineMsg).Error; err != nil {
		return fmt.Errorf("failed to create offline message index: %v", err)
	}

	// 3. 发送到推送任务队列
	if config.GlobalConfig.Push.Enabled {
		pushTask := PushTask{
			UserID:         payload.ReceiverID,
			MessageID:      payload.MessageID,
			SenderID:       payload.SenderID,
			Content:        payload.Content,
			ConversationID: payload.ConversationID,
			Timestamp:      payload.Timestamp,
		}

		taskData, err := json.Marshal(pushTask)
		if err != nil {
			log.Printf("Failed to marshal push task: %v", err)
		} else {
			if err := mq.PublishMessage(
				config.GlobalConfig.RabbitMQ.Exchange.Notification,
				"push.task",
				taskData,
			); err != nil {
				log.Printf("Failed to publish push task: %v", err)
			}
		}
	}

	log.Printf("Offline message %d for user %s processed", payload.MessageID, payload.ReceiverID)
	return nil
}
