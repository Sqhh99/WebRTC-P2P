package service

import (
	"NetherLink-server/config"
	"NetherLink-server/internal/model"
	"NetherLink-server/pkg/database"
	"NetherLink-server/pkg/mq"
	"encoding/json"
	"fmt"
	"log"
	"time"

	"gorm.io/gorm"
)

// MessagePersistenceService 消息持久化服务
type MessagePersistenceService struct{}

// conversationModel 会话模型(用于指定表名)
type conversationModel struct {
	ID              int64     `gorm:"column:id;primary_key;auto_increment"`
	ConversationID  string    `gorm:"column:conversation_id"`
	UserID          string    `gorm:"column:user_id"`
	TargetID        string    `gorm:"column:target_id"`
	IsGroup         bool      `gorm:"column:is_group"`
	LastMessage     string    `gorm:"column:last_message"`
	LastMessageTime time.Time `gorm:"column:last_message_time"`
	UnreadCount     int       `gorm:"column:unread_count"`
	CreatedAt       time.Time `gorm:"column:created_at"`
	UpdatedAt       time.Time `gorm:"column:updated_at"`
}

// TableName 指定表名为 conversation (单数)
func (conversationModel) TableName() string {
	return "conversation"
}

// MessagePayload 消息队列中的消息载体
type MessagePayload struct {
	MessageID      int64     `json:"message_id"`
	ConversationID string    `json:"conversation_id"`
	SenderID       string    `json:"sender_id"`
	ReceiverID     string    `json:"receiver_id"`
	MessageType    string    `json:"message_type"`
	Content        string    `json:"content"`
	Extra          string    `json:"extra"`
	IsGroup        bool      `json:"is_group"`
	Timestamp      time.Time `json:"timestamp"`
}

// NewMessagePersistenceService 创建消息持久化服务
func NewMessagePersistenceService() *MessagePersistenceService {
	return &MessagePersistenceService{}
}

// Start 启动消息持久化服务
func (s *MessagePersistenceService) Start() error {
	log.Println("Starting Message Persistence Service...")

	messages, err := mq.ConsumeMessages(config.GlobalConfig.RabbitMQ.Queue.PrivateMessages)
	if err != nil {
		return fmt.Errorf("failed to consume messages: %v", err)
	}

	go func() {
		log.Println("Message Persistence Service goroutine started, waiting for messages...")
		for msg := range messages {
			log.Printf("Received message from queue, size: %d bytes", len(msg.Body))
			if err := s.processMessage(msg.Body); err != nil {
				log.Printf("Failed to process message: %v", err)
				// 消息处理失败，拒绝并重新入队
				msg.Nack(false, true)
			} else {
				// 确认消息处理成功
				log.Printf("Message processed successfully, acknowledging...")
				msg.Ack(false)
			}
		}
		log.Println("Message Persistence Service goroutine stopped")
	}()

	log.Println("Message Persistence Service started successfully")
	return nil
}

// processMessage 处理单条消息
func (s *MessagePersistenceService) processMessage(body []byte) error {
	log.Printf("Processing message: %s", string(body))

	var payload MessagePayload
	if err := json.Unmarshal(body, &payload); err != nil {
		return fmt.Errorf("failed to unmarshal message: %v", err)
	}

	log.Printf("Message parsed - ID: %d, From: %s, To: %s, Content: %s",
		payload.MessageID, payload.SenderID, payload.ReceiverID, payload.Content)

	db, err := database.GetDB()
	if err != nil {
		return fmt.Errorf("failed to get database: %v", err)
	}

	// 检查消息是否已存在(去重)
	var existingMessage model.Message
	if err := db.Where("id = ?", payload.MessageID).First(&existingMessage).Error; err == nil {
		log.Printf("Message %d already exists in database, skipping...", payload.MessageID)
		return nil // 消息已存在,直接返回成功
	}

	// 开启事务
	tx := db.Begin()
	defer func() {
		if r := recover(); r != nil {
			tx.Rollback()
		}
	}()

	// 1. 保存消息到数据库
	message := model.Message{
		ID:           payload.MessageID,
		Conversation: payload.ConversationID,
		SenderID:     payload.SenderID,
		Timestamp:    payload.Timestamp,
		Type:         model.MessageType(payload.MessageType),
		Content:      payload.Content,
	}

	if payload.Extra != "" {
		var extra model.MessageExtra
		if err := json.Unmarshal([]byte(payload.Extra), &extra); err == nil {
			message.Extra = extra
		}
	}

	if err := tx.Create(&message).Error; err != nil {
		tx.Rollback()
		return fmt.Errorf("failed to save message: %v", err)
	}

	// 2. 更新发送者的会话列表
	if err := s.updateConversation(tx, payload.SenderID, payload.ReceiverID, payload.ConversationID, payload.Content, payload.Timestamp, false); err != nil {
		tx.Rollback()
		return fmt.Errorf("failed to update sender conversation: %v", err)
	}

	// 3. 更新接收者的会话列表
	if err := s.updateConversation(tx, payload.ReceiverID, payload.SenderID, payload.ConversationID, payload.Content, payload.Timestamp, true); err != nil {
		tx.Rollback()
		return fmt.Errorf("failed to update receiver conversation: %v", err)
	}

	// 提交事务
	if err := tx.Commit().Error; err != nil {
		return fmt.Errorf("failed to commit transaction: %v", err)
	}

	// 4. 发送到消息分发队列
	if err := s.dispatchMessage(payload); err != nil {
		log.Printf("Failed to dispatch message: %v", err)
		// 分发失败不影响消息持久化
	}

	log.Printf("Message %d persisted successfully", payload.MessageID)
	return nil
}

// updateConversation 更新会话列表
func (s *MessagePersistenceService) updateConversation(txInterface interface{}, userID, targetID, conversationID, lastMessage string, timestamp time.Time, incrUnread bool) error {
	// 将 interface{} 转换为 *gorm.DB
	tx, ok := txInterface.(*gorm.DB)
	if !ok {
		return fmt.Errorf("invalid transaction type")
	}

	var conv conversationModel
	err := tx.Where("user_id = ? AND conversation_id = ?", userID, conversationID).First(&conv).Error

	if err != nil {
		// 会话不存在，创建新会话
		conv = conversationModel{
			ConversationID:  conversationID,
			UserID:          userID,
			TargetID:        targetID,
			IsGroup:         false,
			LastMessage:     lastMessage,
			LastMessageTime: timestamp,
			UnreadCount:     0,
			CreatedAt:       time.Now(),
			UpdatedAt:       time.Now(),
		}
		if incrUnread {
			conv.UnreadCount = 1
		}
		return tx.Create(&conv).Error
	}

	// 更新现有会话
	updates := map[string]interface{}{
		"last_message":      lastMessage,
		"last_message_time": timestamp,
		"updated_at":        time.Now(),
	}
	if incrUnread {
		updates["unread_count"] = conv.UnreadCount + 1
	}

	return tx.Model(&conv).Where("id = ?", conv.ID).Updates(updates).Error
}

// dispatchMessage 分发消息到消息分发队列
func (s *MessagePersistenceService) dispatchMessage(payload MessagePayload) error {
	data, err := json.Marshal(payload)
	if err != nil {
		return err
	}

	return mq.PublishMessage(
		config.GlobalConfig.RabbitMQ.Exchange.Chat,
		"offline.message",
		data,
	)
}
