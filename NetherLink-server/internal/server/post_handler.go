package server

import (
	"NetherLink-server/config"
	"NetherLink-server/internal/model"
	"NetherLink-server/pkg/database"
	"NetherLink-server/pkg/utils"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

type createPostRequest struct {
	Title   string `json:"title"`
	Content string `json:"content"`
}

type createCommentRequest struct {
	Content string `json:"content" binding:"required"`
}

// UploadImageHandler 上传图片处理器
func UploadImageHandler(c *gin.Context) {
	file, header, err := c.Request.FormFile("file")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "未找到文件"})
		return
	}
	defer file.Close()

	ext := strings.ToLower(filepath.Ext(header.Filename))
	if ext != ".jpg" && ext != ".jpeg" && ext != ".png" && ext != ".gif" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "仅支持jpg、jpeg、png、gif格式"})
		return
	}

	filename := utils.GenerateImageFilename(ext)
	savePath := utils.GetImageSavePath(filename)

	if err := os.MkdirAll(filepath.Dir(savePath), 0755); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "创建目录失败"})
		return
	}

	out, err := os.Create(savePath)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "保存文件失败"})
		return
	}
	defer out.Close()

	if _, err := io.Copy(out, file); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "写入文件失败"})
		return
	}

	url := utils.GetFullImageURL(filename)
	c.JSON(http.StatusOK, gin.H{"url": url})
}

// GetPostsHandler 获取帖子列表
func GetPostsHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"code": -1, "message": "未授权"})
		return
	}

	db, err := database.GetDB()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "数据库连接失败"})
		return
	}

	var posts []struct {
		PostID     int64     `gorm:"column:post_id"`
		Title      string    `gorm:"column:title"`
		UserID     string    `gorm:"column:user_id"`
		UserName   string    `gorm:"column:name"`
		UserAvatar string    `gorm:"column:avatar_url"`
		ImageURL   string    `gorm:"column:image_url"`
		CreatedAt  time.Time `gorm:"column:created_at"`
	}

	err = db.Table("posts").
		Select("posts.post_id, posts.title, posts.user_id, posts.image_url, users.name, users.avatar_url, posts.created_at").
		Joins("LEFT JOIN users ON posts.user_id = users.uid").
		Order("posts.created_at DESC").
		Find(&posts).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "获取帖子列表失败"})
		return
	}

	var result []model.PostPreview
	for _, post := range posts {
		var preview model.PostPreview
		preview.PostID = post.PostID
		preview.Title = post.Title
		preview.UserID = post.UserID
		preview.UserName = post.UserName
		preview.UserAvatar = &post.UserAvatar
		preview.CreatedAt = post.CreatedAt.Format("2006-01-02 15:04:05")
		if post.ImageURL != "" {
			preview.FirstImage = &post.ImageURL
		}

		var likesCount int64
		if err := db.Model(&model.PostLike{}).
			Where("post_id = ?", post.PostID).
			Count(&likesCount).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "获取点赞数失败"})
			return
		}
		preview.LikesCount = likesCount

		var likeCount int64
		if err := db.Model(&model.PostLike{}).
			Where("post_id = ? AND user_id = ?", post.PostID, userID).
			Count(&likeCount).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "检查点赞状态失败"})
			return
		}
		preview.IsLiked = likeCount > 0

		result = append(result, preview)
	}

	c.JSON(http.StatusOK, gin.H{
		"posts": result,
	})
}

// CreatePostHandler 创建帖子
func CreatePostHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"code": -1, "message": "未授权"})
		return
	}

	form, err := c.MultipartForm()
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"code": -1, "message": "无效的请求格式"})
		return
	}

	jsonData := form.Value["data"]
	if len(jsonData) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"code": -1, "message": "缺少帖子数据"})
		return
	}

	var req createPostRequest
	if err := json.Unmarshal([]byte(jsonData[0]), &req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"code": -1, "message": "请求数据格式错误"})
		return
	}

	if req.Title == "" || req.Content == "" {
		c.JSON(http.StatusBadRequest, gin.H{"code": -1, "message": "标题和内容不能为空"})
		return
	}

	db, err := database.GetDB()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "数据库连接失败"})
		return
	}

	post := &model.Post{
		UserID:    userID,
		Title:     req.Title,
		Content:   req.Content,
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}

	if err := db.Create(post).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "创建帖子失败"})
		return
	}

	files := form.File["images"]
	if len(files) > 0 {
		file := files[0]

		contentType := file.Header.Get("Content-Type")
		if !strings.HasPrefix(contentType, "image/") {
			c.JSON(http.StatusBadRequest, gin.H{"code": -1, "message": "只能上传图片文件"})
			return
		}

		ext := filepath.Ext(file.Filename)
		filename := fmt.Sprintf("post_%d%s", post.PostID, ext)
		savePath := filepath.Join("uploads/posts", filename)

		if err := c.SaveUploadedFile(file, savePath); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "保存图片失败"})
			return
		}

		imageURL := fmt.Sprintf("%s/uploads/posts/%s", config.GlobalConfig.Server.HTTP.BaseURL, filename)
		if err := db.Model(post).Update("image_url", imageURL).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "更新图片URL失败"})
			return
		}
		post.ImageURL = imageURL
	}

	var postInfo struct {
		PostID     int64     `gorm:"column:post_id"`
		Title      string    `gorm:"column:title"`
		Content    string    `gorm:"column:content"`
		ImageURL   string    `gorm:"column:image_url"`
		UserID     string    `gorm:"column:user_id"`
		UserName   string    `gorm:"column:name"`
		UserAvatar string    `gorm:"column:avatar_url"`
		CreatedAt  time.Time `gorm:"column:created_at"`
	}

	err = db.Table("posts").
		Select("posts.post_id, posts.title, posts.content, posts.image_url, posts.user_id, users.name, users.avatar_url, posts.created_at").
		Joins("LEFT JOIN users ON posts.user_id = users.uid").
		Where("posts.post_id = ?", post.PostID).
		First(&postInfo).Error

	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "获取帖子信息失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code": 0,
		"data": gin.H{
			"post_id":     postInfo.PostID,
			"title":       postInfo.Title,
			"content":     postInfo.Content,
			"user_id":     postInfo.UserID,
			"user_name":   postInfo.UserName,
			"user_avatar": postInfo.UserAvatar,
			"image_url":   postInfo.ImageURL,
			"created_at":  postInfo.CreatedAt.Format("2006-01-02 15:04:05"),
		},
	})
}

// GetPostDetailHandler 获取帖子详情
func GetPostDetailHandler(c *gin.Context) {
	postID, err := strconv.ParseInt(c.Param("post_id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的帖子ID"})
		return
	}

	userID := c.GetString("user_id")

	db, err := database.GetDB()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "数据库连接失败"})
		return
	}

	var post model.Post
	if err := db.Preload("Comments").Preload("Likes").First(&post, postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "帖子不存在"})
		} else {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "查询帖子失败"})
		}
		return
	}

	var author model.User
	if err := db.First(&author, "uid = ?", post.UserID).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "查询作者信息失败"})
		return
	}

	isLiked := false
	for _, like := range post.Likes {
		if like.UserID == userID {
			isLiked = true
			break
		}
	}

	var commentUsers = make(map[string]model.User)
	var userIDs []string
	for _, comment := range post.Comments {
		userIDs = append(userIDs, comment.UserID)
	}
	if len(userIDs) > 0 {
		var users []model.User
		if err := db.Where("uid IN ?", userIDs).Find(&users).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "查询评论用户信息失败"})
			return
		}
		for _, user := range users {
			commentUsers[user.UID] = user
		}
	}

	var commentsWithUser []gin.H
	for _, comment := range post.Comments {
		user, exists := commentUsers[comment.UserID]
		commentData := gin.H{
			"comment_id": comment.CommentID,
			"post_id":    comment.PostID,
			"user_id":    comment.UserID,
			"content":    comment.Content,
			"created_at": comment.CreatedAt.Format("2006-01-02 15:04:05"),
		}
		if exists {
			commentData["user_name"] = user.Name
			commentData["user_avatar"] = user.AvatarURL
		} else {
			commentData["user_name"] = "未知用户"
			commentData["user_avatar"] = ""
		}
		commentsWithUser = append(commentsWithUser, commentData)
	}

	response := gin.H{
		"post_id": post.PostID,
		"title":   post.Title,
		"content": post.Content,
		"author": gin.H{
			"user_id":   author.UID,
			"user_name": author.Name,
			"avatar":    author.AvatarURL,
		},
		"is_liked":    isLiked,
		"likes_count": len(post.Likes),
		"comments":    commentsWithUser,
	}

	c.JSON(http.StatusOK, response)
}

// CreateCommentHandler 创建评论
func CreateCommentHandler(c *gin.Context) {
	postID, err := strconv.ParseInt(c.Param("post_id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的帖子ID"})
		return
	}

	userID := c.GetString("user_id")

	var req createCommentRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "无效的请求参数"})
		return
	}

	if strings.TrimSpace(req.Content) == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "评论内容不能为空"})
		return
	}

	db, err := database.GetDB()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "数据库连接失败"})
		return
	}

	var post model.Post
	if err := db.First(&post, postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"error": "帖子不存在"})
		} else {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "查询帖子失败"})
		}
		return
	}

	comment := model.Comment{
		PostID:    postID,
		UserID:    userID,
		Content:   req.Content,
		CreatedAt: time.Now(),
	}

	if err := db.Create(&comment).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "创建评论失败"})
		return
	}

	var user model.User
	if err := db.First(&user, "uid = ?", userID).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "查询用户信息失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"comment_id":  comment.CommentID,
		"post_id":     comment.PostID,
		"user_id":     comment.UserID,
		"user_name":   user.Name,
		"user_avatar": user.AvatarURL,
		"content":     comment.Content,
		"created_at":  comment.CreatedAt.Format("2006-01-02 15:04:05"),
	})
}

// TogglePostLikeHandler 切换点赞状态
func TogglePostLikeHandler(c *gin.Context) {
	userID := c.GetString("user_id")
	if userID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"code": -1, "message": "未授权"})
		return
	}

	postID, err := strconv.ParseInt(c.Param("post_id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"code": -1, "message": "无效的帖子ID"})
		return
	}

	db, err := database.GetDB()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "数据库连接失败"})
		return
	}

	var post model.Post
	if err := db.First(&post, postID).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			c.JSON(http.StatusNotFound, gin.H{"code": -1, "message": "帖子不存在"})
		} else {
			c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "查询帖子失败"})
		}
		return
	}

	tx := db.Begin()
	if tx.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "开启事务失败"})
		return
	}
	defer func() {
		if r := recover(); r != nil {
			tx.Rollback()
		}
	}()

	var like model.PostLike
	err = tx.Where("post_id = ? AND user_id = ?", postID, userID).First(&like).Error
	wasLiked := err != gorm.ErrRecordNotFound

	if !wasLiked {
		like = model.PostLike{
			PostID:  postID,
			UserID:  userID,
			LikedAt: time.Now(),
		}
		if err := tx.Create(&like).Error; err != nil {
			tx.Rollback()
			c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "创建点赞记录失败"})
			return
		}
	} else {
		if err := tx.Delete(&like).Error; err != nil {
			tx.Rollback()
			c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "删除点赞记录失败"})
			return
		}
	}

	if err := tx.Commit().Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "提交事务失败"})
		return
	}

	var likesCount int64
	if err := db.Model(&model.PostLike{}).Where("post_id = ?", postID).Count(&likesCount).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "获取点赞数失败"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"code": 0,
		"data": gin.H{
			"is_liked":    !wasLiked,
			"likes_count": likesCount,
		},
	})
}
