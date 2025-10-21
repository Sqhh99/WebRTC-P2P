package server

import (
	"NetherLink-server/config"
	"NetherLink-server/internal/middleware"
	"NetherLink-server/internal/model"
	"NetherLink-server/pkg/database"
	"NetherLink-server/pkg/utils"
	"crypto/md5"
	"encoding/hex"
	"fmt"
	"math/rand"
	"net/http"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

var (
	codeStore      = make(map[string]codeEntry)
	codeStoreMutex sync.Mutex
)

type codeEntry struct {
	Code      string
	ExpiresAt time.Time
}

type sendCodeRequest struct {
	Email string `json:"email" binding:"required,email"`
}

type registerRequest struct {
	Email      string `json:"email" binding:"required,email"`
	User       string `json:"user" binding:"required"`
	Passwd     string `json:"passwd" binding:"required"`
	VarifyCode string `json:"varifycode" binding:"required"`
	AvatarURL  string `json:"avatar_url"`
}

type loginRequest struct {
	Email  string `json:"email" binding:"required,email"`
	Passwd string `json:"passwd" binding:"required"`
}

type loginResponse struct {
	UID       string `json:"uid"`
	User      string `json:"user"`
	Email     string `json:"email"`
	AvatarURL string `json:"avatar_url"`
	Token     string `json:"token"`
}

// SendCodeHandler 发送验证码
func SendCodeHandler(c *gin.Context) {
	var req sendCodeRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "参数错误"})
		return
	}

	code := generateCode(6)
	expiresAt := time.Now().Add(3 * time.Minute)

	codeStoreMutex.Lock()
	codeStore[req.Email] = codeEntry{Code: code, ExpiresAt: expiresAt}
	codeStoreMutex.Unlock()

	emailCfg := config.GlobalConfig.Email
	sender := utils.NewEmailSender(emailCfg.SMTPHost, emailCfg.SMTPPort, emailCfg.Sender, emailCfg.DisplayName, emailCfg.Password, emailCfg.UseSSL)

	template := utils.GetEmailTemplate(code)
	err := sender.Send(req.Email, "NetherLink 注册验证码", template)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "邮件发送失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "验证码已发送"})
}

// RegisterHandler 注册处理器
func RegisterHandler(c *gin.Context) {
	var req registerRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "参数错误"})
		return
	}

	// 校验验证码
	codeStoreMutex.Lock()
	entry, ok := codeStore[req.Email]
	codeStoreMutex.Unlock()
	if !ok || entry.Code != req.VarifyCode || time.Now().After(entry.ExpiresAt) {
		c.JSON(http.StatusBadRequest, gin.H{"error": "验证码无效或已过期"})
		return
	}

	db, err := database.GetDB()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "数据库连接失败"})
		return
	}

	// 检查邮箱是否已存在
	var count int64
	db.Model(&model.User{}).Where("email = ?", req.Email).Count(&count)
	if count > 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "邮箱已存在"})
		return
	}

	// md5加密密码
	h := md5.New()
	h.Write([]byte(req.Passwd))
	passwdHash := hex.EncodeToString(h.Sum(nil))

	// 生成UUID并获取前8位作为用户ID
	fullUUID := generateUUID()
	userID := fullUUID[:8]

	user := model.User{
		UID:       fullUUID,
		ID:        userID,
		Email:     req.Email,
		Name:      req.User,
		Password:  passwdHash,
		AvatarURL: req.AvatarURL,
		Status:    0,
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}

	if err := db.Create(&user).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "创建用户失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"uid":        user.UID,
		"user":       user.ID,
		"email":      user.Email,
		"avatar_url": user.AvatarURL,
	})
}

// LoginHandler 登录处理器
func LoginHandler(c *gin.Context) {
	var req loginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "参数错误"})
		return
	}

	db, err := database.GetDB()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "数据库连接失败"})
		return
	}

	// md5加密密码
	h := md5.New()
	h.Write([]byte(req.Passwd))
	passwdHash := hex.EncodeToString(h.Sum(nil))

	var user model.User
	if err := db.Where("email = ? AND password = ?", req.Email, passwdHash).First(&user).Error; err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "邮箱或密码错误"})
		return
	}

	token, err := middleware.GenerateJWT(user.UID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "生成token失败"})
		return
	}

	c.JSON(http.StatusOK, loginResponse{
		UID:       user.UID,
		User:      user.ID,
		Email:     user.Email,
		AvatarURL: user.AvatarURL,
		Token:     token,
	})
}

// SearchUsersHandler 搜索用户处理器
func SearchUsersHandler(c *gin.Context) {
	keyword := c.Query("keyword")
	if keyword == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "搜索关键词不能为空"})
		return
	}

	currentUID := c.GetString("user_id")
	db, err := database.GetDB()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "数据库连接失败"})
		return
	}

	var users []SearchUserResponse

	scoreQuery := `
    CASE 
        WHEN id = ? THEN 100
        WHEN name = ? THEN 90
        WHEN id LIKE ? THEN 80
        WHEN name LIKE ? THEN 70
        WHEN id LIKE ? THEN 60
        WHEN name LIKE ? THEN 50
        ELSE 0
    END AS relevance_score`

	if err := db.Table("users u").
		Select("u.uid, u.id, u.name, u.avatar_url, u.signature, u.status, "+scoreQuery,
			keyword, keyword,
			keyword+"%", keyword+"%",
			"%"+keyword+"%", "%"+keyword+"%",
		).
		Joins("LEFT JOIN friends f ON f.friend_id = u.uid AND f.user_id = ?", currentUID).
		Where("u.uid != ?", currentUID).
		Where("f.friend_id IS NULL").
		Where(db.Where("u.id LIKE ?", "%"+keyword+"%").
			Or("u.name LIKE ?", "%"+keyword+"%"),
		).
		Order("relevance_score DESC").
		Limit(20).
		Find(&users).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "搜索用户失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"users": users})
}

// SearchGroupsHandler 搜索群组处理器
func SearchGroupsHandler(c *gin.Context) {
	keyword := c.Query("keyword")
	if keyword == "" {
		c.JSON(http.StatusBadRequest, gin.H{
			"error": "搜索关键词不能为空",
		})
		return
	}

	db, err := database.GetDB()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error": "数据库连接失败",
		})
		return
	}

	var groups []SearchGroupResponse

	memberCountSubQuery := db.Table("group_members").
		Select("gid, COUNT(*) as member_count").
		Group("gid")

	query := db.Table("chat_groups").
		Select("chat_groups.gid, chat_groups.name, chat_groups.avatar, IFNULL(member_counts.member_count, 0) as member_count").
		Joins("LEFT JOIN (?) as member_counts ON chat_groups.gid = member_counts.gid", memberCountSubQuery)

	scoreQuery := `
		CASE 
			WHEN name = ? THEN 100
			WHEN name LIKE ? THEN 80
			WHEN name LIKE ? THEN 60
			ELSE 0
		END as relevance_score`

	searchQuery := query.Select("*, "+scoreQuery,
		keyword,
		keyword+"%",
		"%"+keyword+"%",
	).Where(
		"name LIKE ?", "%"+keyword+"%",
	).Order("relevance_score DESC").
		Limit(20)

	if err := searchQuery.Find(&groups).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error": "搜索群聊失败",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"groups": groups,
	})
}

// SearchUserResponse 用户搜索响应结构
type SearchUserResponse struct {
	UID       string `json:"uid"`
	ID        string `json:"id"`
	Name      string `json:"name"`
	AvatarURL string `json:"avatar_url"`
	Signature string `json:"signature"`
	Status    int    `json:"status"`
}

// SearchGroupResponse 群聊搜索响应结构
type SearchGroupResponse struct {
	GID         int    `json:"gid"`
	Name        string `json:"name"`
	Avatar      string `json:"avatar"`
	MemberCount int    `json:"member_count"`
}

// 辅助函数
func generateCode(length int) string {
	rand.Seed(time.Now().UnixNano())
	min := int64(100000)
	max := int64(999999)
	return fmt.Sprintf("%06d", rand.Int63n(max-min+1)+min)
}

func generateUUID() string {
	return uuid.New().String()
}
