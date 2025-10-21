package server

import (
	"NetherLink-server/internal/model"
	"NetherLink-server/pkg/database"
	"net/http"

	"github.com/gin-gonic/gin"
)

// GetContactsHandler 获取联系人列表
func GetContactsHandler(c *gin.Context) {
	uid, _ := c.Get("user_id")
	userID := uid.(string)

	db, err := database.GetDB()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "数据库连接失败"})
		return
	}

	response := model.ContactResponse{}

	var userResult struct {
		Name      string `gorm:"column:name"`
		AvatarURL string `gorm:"column:avatar_url"`
	}
	if err := db.Table("users").
		Select("name, avatar_url").
		Where("uid = ?", userID).
		First(&userResult).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"code": -1, "message": "获取用户信息失败"})
		return
	}

	response.User = model.UserInfo{
		Name:   userResult.Name,
		Avatar: userResult.AvatarURL,
	}

	type FriendResult struct {
		UserID    string `gorm:"column:uid"`
		Name      string `gorm:"column:name"`
		AvatarURL string `gorm:"column:avatar_url"`
		Signature string `gorm:"column:signature"`
		Status    int    `gorm:"column:status"`
	}

	var friends []FriendResult
	if err := db.Table("users").
		Select("users.uid, users.name, users.avatar_url, users.signature, users.status").
		Joins("INNER JOIN friends ON users.uid = friends.friend_id").
		Where("friends.user_id = ?", userID).
		Find(&friends).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "获取好友列表失败"})
		return
	}

	for _, f := range friends {
		response.Friends = append(response.Friends, model.FriendInfo{
			UserID:    f.UserID,
			Name:      f.Name,
			Avatar:    f.AvatarURL,
			Signature: f.Signature,
			Status:    f.Status,
		})
	}

	type GroupResult struct {
		GID     int    `gorm:"column:gid"`
		Name    string `gorm:"column:name"`
		Avatar  string `gorm:"column:avatar"`
		OwnerID string `gorm:"column:owner_id"`
	}

	var groupResults []GroupResult
	if err := db.Table("group_members").
		Select("chat_groups.gid, chat_groups.name, chat_groups.avatar, chat_groups.owner_id").
		Joins("LEFT JOIN chat_groups ON group_members.gid = chat_groups.gid").
		Where("group_members.uid = ?", userID).
		Find(&groupResults).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "获取群组列表失败"})
		return
	}

	for _, gr := range groupResults {
		var members []model.GroupMemberInfo
		if err := db.Table("group_members").
			Select("users.uid, users.name, users.avatar_url as avatar, group_members.role").
			Joins("LEFT JOIN users ON group_members.uid = users.uid").
			Where("group_members.gid = ?", gr.GID).
			Find(&members).Error; err != nil {
			continue
		}

		response.Groups = append(response.Groups, model.GroupInfo{
			GID:     gr.GID,
			Name:    gr.Name,
			Avatar:  gr.Avatar,
			OwnerID: gr.OwnerID,
			Members: members,
		})
	}

	c.JSON(http.StatusOK, response)
}
