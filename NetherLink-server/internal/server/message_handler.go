package server

import (
	"NetherLink-server/pkg/database"
	"log"
	"net/http"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
)

// OfflineMessage 离线消息响应结构
type OfflineMessage struct {
	MessageID      int64     `json:"message_id"`
	ConversationID string    `json:"conversation_id"`
	SenderID       string    `json:"sender_id"`
	Content        string    `json:"content"`
	Type           string    `json:"type"`
	Extra          string    `json:"extra"`
	Timestamp      time.Time `json:"timestamp"`
	CreatedAt      time.Time `json:"created_at"`
}

// GetOfflineMessagesHandler 获取离线消息列表
func GetOfflineMessagesHandler(c *gin.Context) {
	// 从JWT中获取用户ID
	userID, exists := c.Get("uid")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"success": false,
			"message": "未授权",
		})
		return
	}

	// 获取分页参数
	page, _ := strconv.Atoi(c.DefaultQuery("page", "1"))
	pageSize, _ := strconv.Atoi(c.DefaultQuery("page_size", "50"))
	conversationID := c.Query("conversation_id") // 可选：按会话过滤

	if page < 1 {
		page = 1
	}
	if pageSize < 1 || pageSize > 100 {
		pageSize = 50
	}

	db, err := database.GetDB()
	if err != nil {
		log.Printf("Failed to get database: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"message": "数据库连接失败",
		})
		return
	}

	// 构建查询
	query := db.Table("offline_message_index AS omi").
		Select(`
			omi.message_id,
			m.conversation,
			m.sender_id,
			m.content,
			m.type,
			m.extra,
			m.timestamp,
			omi.created_at
		`).
		Joins("INNER JOIN message AS m ON omi.message_id = m.id").
		Where("omi.user_id = ? AND omi.synced = ?", userID, false).
		Order("omi.created_at DESC")

	// 如果指定了会话ID，添加过滤
	if conversationID != "" {
		query = query.Where("omi.conversation_id = ?", conversationID)
	}

	// 查询总数
	var total int64
	if err := query.Count(&total).Error; err != nil {
		log.Printf("Failed to count offline messages: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"message": "查询失败",
		})
		return
	}

	// 分页查询
	var messages []OfflineMessage
	offset := (page - 1) * pageSize
	if err := query.Limit(pageSize).Offset(offset).Scan(&messages).Error; err != nil {
		log.Printf("Failed to get offline messages: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"message": "查询失败",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"message": "获取成功",
		"data": gin.H{
			"messages":  messages,
			"total":     total,
			"page":      page,
			"page_size": pageSize,
		},
	})
}

// MarkMessagesSyncedRequest 标记消息已同步请求
type MarkMessagesSyncedRequest struct {
	MessageIDs []int64 `json:"message_ids" binding:"required"`
}

// MarkMessagesSyncedHandler 标记消息为已同步
func MarkMessagesSyncedHandler(c *gin.Context) {
	// 从JWT中获取用户ID
	userID, exists := c.Get("uid")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"success": false,
			"message": "未授权",
		})
		return
	}

	// 解析请求体
	type MarkSyncedRequest struct {
		MessageIDs []int64 `json:"message_ids" binding:"required"`
	}

	var req MarkSyncedRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"success": false,
			"message": "请求参数错误",
		})
		return
	}

	if len(req.MessageIDs) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{
			"success": false,
			"message": "消息ID列表不能为空",
		})
		return
	}

	db, err := database.GetDB()
	if err != nil {
		log.Printf("Failed to get database: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"message": "数据库连接失败",
		})
		return
	}

	// 更新离线消息索引，将指定消息标记为已同步
	result := db.Table("offline_message_index").
		Where("user_id = ? AND message_id IN (?) AND synced = ?", userID, req.MessageIDs, false).
		Updates(map[string]interface{}{
			"synced":    true,
			"synced_at": time.Now(),
		})

	if result.Error != nil {
		log.Printf("Failed to mark messages as synced: %v", result.Error)
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"message": "标记失败",
		})
		return
	}

	log.Printf("Marked %d messages as synced for user %s", result.RowsAffected, userID)

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"message": "标记成功",
		"data": gin.H{
			"marked_count": result.RowsAffected,
		},
	})
}

// GetUnreadCountHandler 获取未读消息数量（按会话）
func GetUnreadCountHandler(c *gin.Context) {
	// 从JWT中获取用户ID
	userID, exists := c.Get("uid")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"success": false,
			"message": "未授权",
		})
		return
	}

	db, err := database.GetDB()
	if err != nil {
		log.Printf("Failed to get database: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"message": "数据库连接失败",
		})
		return
	}

	// 统计每个会话的未读消息数
	type UnreadCount struct {
		ConversationID string `json:"conversation_id"`
		Count          int64  `json:"count"`
	}

	var unreadCounts []UnreadCount
	err = db.Table("offline_message_index").
		Select("conversation_id, COUNT(*) as count").
		Where("user_id = ? AND synced = ?", userID, false).
		Group("conversation_id").
		Scan(&unreadCounts).Error

	if err != nil {
		log.Printf("Failed to get unread count: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"message": "查询失败",
		})
		return
	}

	// 计算总未读数
	var totalUnread int64
	for _, uc := range unreadCounts {
		totalUnread += uc.Count
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"message": "获取成功",
		"data": gin.H{
			"total_unread":           totalUnread,
			"unread_by_conversation": unreadCounts,
		},
	})
}

// ClearOfflineMessagesHandler 清理已同步的离线消息索引（可选的后台清理任务）
func ClearOfflineMessagesHandler(c *gin.Context) {
	// 从JWT中获取用户ID
	userID, exists := c.Get("uid")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"success": false,
			"message": "未授权",
		})
		return
	}

	// 获取保留天数参数（默认保留7天）
	daysToKeep, _ := strconv.Atoi(c.DefaultQuery("days", "7"))
	if daysToKeep < 1 {
		daysToKeep = 7
	}

	db, err := database.GetDB()
	if err != nil {
		log.Printf("Failed to get database: %v", err)
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"message": "数据库连接失败",
		})
		return
	}

	// 删除已同步且超过保留期的离线消息索引
	cutoffTime := time.Now().AddDate(0, 0, -daysToKeep)
	result := db.Table("offline_message_index").
		Where("user_id = ? AND synced = ? AND synced_at < ?", userID, true, cutoffTime).
		Delete(nil)

	if result.Error != nil {
		log.Printf("Failed to clear offline messages: %v", result.Error)
		c.JSON(http.StatusInternalServerError, gin.H{
			"success": false,
			"message": "清理失败",
		})
		return
	}

	log.Printf("Cleared %d offline message indexes for user %s", result.RowsAffected, userID)

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"message": "清理成功",
		"data": gin.H{
			"deleted_count": result.RowsAffected,
		},
	})
}
