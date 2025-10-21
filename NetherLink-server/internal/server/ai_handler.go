package server

import (
	"NetherLink-server/config"
	"NetherLink-server/internal/model"
	"NetherLink-server/pkg/database"
	"context"
	"encoding/json"
	"log"
	"sync"
	"time"

	"github.com/google/uuid"
	"github.com/gorilla/websocket"
	"google.golang.org/genai"
)

// AIHandler 处理AI对话的WebSocket连接
type AIHandler struct {
	conn        *websocket.Conn
	userID      string
	isStreaming bool
	streamLock  sync.Mutex
	activeConv  string
}

// NewAIHandler 创建新的AI处理器
func NewAIHandler(conn *websocket.Conn, userID string) *AIHandler {
	return &AIHandler{
		conn:   conn,
		userID: userID,
	}
}

// HandleMessage 处理接收到的消息
func (h *AIHandler) HandleMessage(messageType int, message []byte) error {
	// 获取流式传输锁
	h.streamLock.Lock()
	if h.isStreaming {
		h.streamLock.Unlock()
		return h.sendError("当前正在处理其他请求，请稍后再试")
	}
	h.isStreaming = true
	h.streamLock.Unlock()

	// 函数结束时清理状态
	defer func() {
		h.streamLock.Lock()
		h.isStreaming = false
		h.streamLock.Unlock()
	}()

	// 解析请求
	var req model.AIRequest
	if err := json.Unmarshal(message, &req); err != nil {
		return h.sendError("无效的请求格式")
	}

	// 处理新对话请求
	if req.ConversationID == "" {
		// 如果存在未完成的对话，先清理
		if h.activeConv != "" {
			if err := h.cleanupIncompleteConversation(h.activeConv); err != nil {
				log.Printf("清理未完成对话失败: %v", err)
			}
		}

		// 创建新对话
		conversationID := uuid.New().String()
		title := req.Message
		if len(title) > 30 {
			title = title[:30] + "..."
		}
		conversation := model.AIConversation{
			ConversationID: conversationID,
			UserID:         h.userID,
			Title:          title,
			CreatedAt:      time.Now(),
			UpdatedAt:      time.Now(),
		}

		db, err := database.GetDB()
		if err != nil {
			return h.sendError("数据库连接失败")
		}

		if err := db.Create(&conversation).Error; err != nil {
			return h.sendError("创建对话失败")
		}

		h.activeConv = conversationID
		req.ConversationID = conversationID
	} else {
		// 验证对话所有权
		db, err := database.GetDB()
		if err != nil {
			return h.sendError("数据库连接失败")
		}

		var conv model.AIConversation
		if err := db.Where("conversation_id = ? AND user_id = ?", req.ConversationID, h.userID).First(&conv).Error; err != nil {
			return h.sendError("无效的对话ID或无权访问")
		}

		h.activeConv = req.ConversationID
	}

	// 获取数据库连接
	db, err := database.GetDB()
	if err != nil {
		return h.sendError("数据库连接失败")
	}

	// 保存用户消息
	userMessage := model.AIMessage{
		ConversationID: req.ConversationID,
		Role:           "user",
		Content:        req.Message,
		CreatedAt:      time.Now(),
	}

	if err := db.Create(&userMessage).Error; err != nil {
		return h.sendError("保存消息失败")
	}

	// 获取历史消息
	var messages []model.AIMessage
	if err := db.Where("conversation_id = ?", req.ConversationID).
		Order("created_at ASC").
		Limit(config.GlobalConfig.AI.MaxHistory).
		Find(&messages).Error; err != nil {
		return h.sendError("获取历史消息失败")
	}

	// 发送开始标志
	startMsg := model.WebSocketMessage{
		Type: "start",
		Data: map[string]string{
			"conversation_id": req.ConversationID,
		},
	}
	if err := h.sendJSON(startMsg); err != nil {
		return err
	}

	// 调用 Gemini API
	ctx := context.Background()
	client, err := genai.NewClient(ctx, &genai.ClientConfig{
		APIKey: config.GlobalConfig.AI.APIKey,
	})
	if err != nil {
		return h.sendError("创建AI客户端失败: " + err.Error())
	}

	// 构建历史消息内容
	var contents []*genai.Content
	for _, msg := range messages {
		role := "user"
		if msg.Role == "assistant" {
			role = "model"
		}
		// 使用 genai.Text 直接生成 Content
		textContents := genai.Text(msg.Content)
		for _, content := range textContents {
			content.Role = role
			contents = append(contents, content)
		}
	}

	// 添加当前消息
	currentContents := genai.Text(req.Message)
	for _, content := range currentContents {
		content.Role = "user"
		contents = append(contents, content)
	}

	// 使用 GenerateContentStream 进行流式生成
	result := client.Models.GenerateContentStream(
		ctx,
		config.GlobalConfig.AI.Model,
		contents,
		nil,
	)

	var fullResponse string
	for resp, err := range result {
		if err != nil {
			return h.sendError("生成内容失败: " + err.Error())
		}

		// 直接获取文本内容
		content := resp.Text()
		fullResponse += content

		// 发送流式响应
		msg := model.WebSocketMessage{
			Type:    "message",
			Content: content,
		}
		if err := h.sendJSON(msg); err != nil {
			return err
		}
	}

	// 保存AI响应
	aiMessage := model.AIMessage{
		ConversationID: req.ConversationID,
		Role:           "assistant",
		Content:        fullResponse,
		CreatedAt:      time.Now(),
	}

	// 重新获取数据库连接（因为可能已经超时）
	db, err = database.GetDB()
	if err != nil {
		return h.sendError("数据库连接失败")
	}

	if err := db.Create(&aiMessage).Error; err != nil {
		return h.sendError("保存AI响应失败")
	}

	// 更新对话时间
	if err := db.Model(&model.AIConversation{}).
		Where("conversation_id = ?", req.ConversationID).
		Update("updated_at", time.Now()).Error; err != nil {
		log.Printf("更新对话时间失败: %v", err)
	}

	// 发送结束标志
	endMsg := model.WebSocketMessage{
		Type: "end",
	}
	return h.sendJSON(endMsg)
}

// cleanupIncompleteConversation 清理未完成的对话
func (h *AIHandler) cleanupIncompleteConversation(conversationID string) error {
	db, err := database.GetDB()
	if err != nil {
		return err
	}

	// 标记对话为已完成或添加结束标记
	if err := db.Model(&model.AIConversation{}).
		Where("conversation_id = ?", conversationID).
		Update("updated_at", time.Now()).Error; err != nil {
		return err
	}

	return nil
}

// Close 关闭处理器
func (h *AIHandler) Close() {
	// 清理当前活动的对话
	if h.activeConv != "" {
		if err := h.cleanupIncompleteConversation(h.activeConv); err != nil {
			log.Printf("关闭时清理对话失败: %v", err)
		}
	}
}

// sendJSON 发送JSON消息
func (h *AIHandler) sendJSON(message interface{}) error {
	data, err := json.Marshal(message)
	if err != nil {
		return err
	}
	return h.conn.WriteMessage(websocket.TextMessage, data)
}

// sendError 发送错误消息
func (h *AIHandler) sendError(message string) error {
	errMsg := model.WebSocketMessage{
		Type:    "error",
		Content: message,
	}
	return h.sendJSON(errMsg)
}
