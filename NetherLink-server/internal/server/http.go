package server

import (
	"NetherLink-server/config"
	"NetherLink-server/internal/middleware"
	"fmt"
	"log"
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

type HTTPServer struct {
	engine *gin.Engine
}

func NewHTTPServer(engine *gin.Engine) *HTTPServer {
	server := &HTTPServer{
		engine: engine,
	}
	server.setupRoutes()
	return server
}

func (s *HTTPServer) setupRoutes() {
	// 静态文件
	s.engine.Static(config.GlobalConfig.Image.URLPrefix, config.GlobalConfig.Image.UploadDir)
	s.engine.Static("/uploads/posts", "uploads/posts")
	s.engine.GET("/favicon.ico", func(c *gin.Context) {
		c.Status(http.StatusNoContent)
	})

	// 公开接口 - 不需要认证
	s.engine.POST("/api/send_code", SendCodeHandler)       // 发送验证码
	s.engine.POST("/api/upload_image", UploadImageHandler) // 上传图片
	s.engine.POST("/api/register", RegisterHandler)        // 注册
	s.engine.POST("/api/login", LoginHandler)              // 登录

	// 需要认证的接口
	auth := middleware.AuthMiddleware()
	s.engine.GET("/api/contacts", auth, GetContactsHandler)                   // 获取联系人列表
	s.engine.GET("/api/search/users", auth, SearchUsersHandler)               // 搜索用户
	s.engine.GET("/api/search/groups", auth, SearchGroupsHandler)             // 搜索群组
	s.engine.GET("/api/posts", auth, GetPostsHandler)                         // 获取帖子列表
	s.engine.POST("/api/posts", auth, CreatePostHandler)                      // 创建帖子
	s.engine.GET("/api/posts/:post_id", auth, GetPostDetailHandler)           // 获取帖子详情
	s.engine.POST("/api/posts/:post_id/comments", auth, CreateCommentHandler) // 创建评论
	s.engine.POST("/api/posts/:post_id/like", auth, TogglePostLikeHandler)    // 点赞/取消点赞

	// 离线消息相关接口
	s.engine.GET("/api/messages/offline", auth, GetOfflineMessagesHandler)            // 获取离线消息列表
	s.engine.POST("/api/messages/mark_synced", auth, MarkMessagesSyncedHandler)       // 标记消息已同步
	s.engine.GET("/api/messages/unread_count", auth, GetUnreadCountHandler)           // 获取未读消息数
	s.engine.DELETE("/api/messages/offline/clear", auth, ClearOfflineMessagesHandler) // 清除离线消息

	// WebSocket相关接口
	s.engine.GET("/ws/ai", auth, s.handleAIWebSocket) // AI对话WebSocket
	// 注意: WebRTC信令已移至WebSocket服务器(8081端口)的 /ws/webrtc 路由
}

// handleAIWebSocket 处理AI对话的WebSocket连接
func (s *HTTPServer) handleAIWebSocket(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "未授权"})
		return
	}

	upgrader := websocket.Upgrader{
		ReadBufferSize:  1024,
		WriteBufferSize: 1024,
		CheckOrigin: func(r *http.Request) bool {
			return true
		},
	}

	conn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
	if err != nil {
		log.Printf("WebSocket升级失败: %v", err)
		return
	}

	handler := NewAIHandler(conn, userID)
	defer handler.Close()

	for {
		messageType, message, err := conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.Printf("WebSocket错误: %v", err)
			}
			break
		}

		if err := handler.HandleMessage(messageType, message); err != nil {
			log.Printf("处理消息错误: %v", err)
			handler.sendError(err.Error())
		}
	}
}

func (s *HTTPServer) Run() error {
	return s.engine.Run(fmt.Sprintf(":%d", config.GlobalConfig.Server.HTTP.Port))
}
