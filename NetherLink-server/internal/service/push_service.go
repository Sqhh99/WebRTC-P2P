package service

import (
	"NetherLink-server/config"
	"NetherLink-server/internal/model"
	"NetherLink-server/pkg/database"
	"NetherLink-server/pkg/mq"
	"NetherLink-server/pkg/utils"
	"encoding/json"
	"fmt"
	"log"
	"time"
)

// PushService 推送服务
type PushService struct{}

// PushTask 推送任务
type PushTask struct {
	UserID         string    `json:"user_id"`
	MessageID      int64     `json:"message_id"`
	SenderID       string    `json:"sender_id"`
	Content        string    `json:"content"`
	ConversationID string    `json:"conversation_id"`
	Timestamp      time.Time `json:"timestamp"`
}

// NewPushService 创建推送服务
func NewPushService() *PushService {
	return &PushService{}
}

// Start 启动推送服务
func (s *PushService) Start() error {
	log.Println("Starting Push Service...")

	tasks, err := mq.ConsumeMessages(config.GlobalConfig.RabbitMQ.Queue.PushTasks)
	if err != nil {
		return fmt.Errorf("failed to consume push tasks: %v", err)
	}

	go func() {
		for task := range tasks {
			if err := s.processTask(task.Body); err != nil {
				log.Printf("Failed to process push task: %v", err)
				task.Nack(false, true)
			} else {
				task.Ack(false)
			}
		}
	}()

	log.Println("Push Service started successfully")
	return nil
}

// processTask 处理推送任务
func (s *PushService) processTask(body []byte) error {
	var task PushTask
	if err := json.Unmarshal(body, &task); err != nil {
		return fmt.Errorf("failed to unmarshal push task: %v", err)
	}

	db, err := database.GetDB()
	if err != nil {
		return fmt.Errorf("failed to get database: %v", err)
	}

	// 获取接收者信息
	var receiver model.User
	if err := db.Where("uid = ?", task.UserID).First(&receiver).Error; err != nil {
		return fmt.Errorf("failed to get receiver info: %v", err)
	}

	// 获取发送者信息
	var sender model.User
	if err := db.Where("uid = ?", task.SenderID).First(&sender).Error; err != nil {
		return fmt.Errorf("failed to get sender info: %v", err)
	}

	pushStatus := "success"
	var errorMsg string

	// 根据配置选择推送方式
	pushed := false

	// APNs推送（iOS）
	if config.GlobalConfig.Push.APNS.Enabled {
		if err := s.pushViaAPNs(task, receiver, sender); err != nil {
			log.Printf("APNs push failed: %v", err)
			errorMsg = err.Error()
			pushStatus = "failed"
		} else {
			pushed = true
		}
	}

	// FCM推送（Android）
	if config.GlobalConfig.Push.FCM.Enabled {
		if err := s.pushViaFCM(task, receiver, sender); err != nil {
			log.Printf("FCM push failed: %v", err)
			if errorMsg != "" {
				errorMsg += "; "
			}
			errorMsg += err.Error()
			pushStatus = "failed"
		} else {
			pushed = true
		}
	}

	// 邮件推送（备选方案）
	if config.GlobalConfig.Push.Email.Enabled && !pushed {
		if err := s.pushViaEmail(task, receiver, sender); err != nil {
			log.Printf("Email push failed: %v", err)
			if errorMsg != "" {
				errorMsg += "; "
			}
			errorMsg += err.Error()
			pushStatus = "failed"
		} else {
			pushed = true
			pushStatus = "success"
		}
	}

	// 记录推送日志
	type PushLog struct {
		ID         int64      `gorm:"column:id;primary_key;auto_increment"`
		UserID     string     `gorm:"column:user_id"`
		MessageID  int64      `gorm:"column:message_id"`
		PushType   string     `gorm:"column:push_type"`
		PushStatus string     `gorm:"column:push_status"`
		PushTime   *time.Time `gorm:"column:push_time"`
		ErrorMsg   string     `gorm:"column:error_msg"`
		CreatedAt  time.Time  `gorm:"column:created_at"`
	}

	now := time.Now()
	pushLog := PushLog{
		UserID:     task.UserID,
		MessageID:  task.MessageID,
		PushType:   s.getPushType(),
		PushStatus: pushStatus,
		PushTime:   &now,
		ErrorMsg:   errorMsg,
		CreatedAt:  now,
	}

	if err := db.Table("push_log").Create(&pushLog).Error; err != nil {
		log.Printf("Failed to save push log: %v", err)
	}

	if !pushed {
		// 如果所有推送方式都未启用，记录调试信息而不是返回错误
		log.Printf("Push skipped for message %d: all push methods are disabled", task.MessageID)
		return nil
	}

	log.Printf("Push task for message %d processed successfully", task.MessageID)
	return nil
}

// pushViaAPNs 通过APNs推送（iOS）
func (s *PushService) pushViaAPNs(task PushTask, receiver, sender interface{}) error {
	// TODO: 实现APNs推送逻辑
	// 需要集成 github.com/sideshow/apns2 库
	log.Printf("APNs push not implemented yet")
	return fmt.Errorf("APNs push not implemented")
}

// pushViaFCM 通过FCM推送（Android）
func (s *PushService) pushViaFCM(task PushTask, receiver, sender interface{}) error {
	// TODO: 实现FCM推送逻辑
	// 需要集成 Firebase Cloud Messaging SDK
	log.Printf("FCM push not implemented yet")
	return fmt.Errorf("FCM push not implemented")
}

// pushViaEmail 通过邮件推送
func (s *PushService) pushViaEmail(task PushTask, receiver, sender interface{}) error {
	r := receiver.(model.User)
	snd := sender.(model.User)

	subject := fmt.Sprintf("来自 %s 的新消息", snd.ID)
	content := task.Content
	if len(content) > 50 {
		content = content[:50] + "..."
	}

	body := fmt.Sprintf(`
		<html>
		<body>
			<h2>您有一条新消息</h2>
			<p><strong>发送者：</strong>%s</p>
			<p><strong>内容：</strong>%s</p>
			<p><strong>时间：</strong>%s</p>
			<hr>
			<p><small>这是一条离线消息通知，请登录应用查看完整内容。</small></p>
		</body>
		</html>
	`, snd.ID, content, task.Timestamp.Format("2006-01-02 15:04:05"))

	// 使用现有的邮件配置
	emailSender := utils.NewEmailSender(
		config.GlobalConfig.Email.SMTPHost,
		config.GlobalConfig.Email.SMTPPort,
		config.GlobalConfig.Email.Sender,
		config.GlobalConfig.Email.DisplayName,
		config.GlobalConfig.Email.Password,
		config.GlobalConfig.Email.UseSSL,
	)

	return emailSender.Send(r.Email, subject, body)
}

// getPushType 获取推送类型
func (s *PushService) getPushType() string {
	if config.GlobalConfig.Push.APNS.Enabled {
		return "apns"
	}
	if config.GlobalConfig.Push.FCM.Enabled {
		return "fcm"
	}
	if config.GlobalConfig.Push.Email.Enabled {
		return "email"
	}
	return "none"
}
