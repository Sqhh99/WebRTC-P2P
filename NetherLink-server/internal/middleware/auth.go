package middleware

import (
	"NetherLink-server/config"
	"net/http"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
)

// AuthMiddleware JWT认证中间件
func AuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.JSON(http.StatusUnauthorized, gin.H{"code": -1, "message": "未提供认证信息"})
			c.Abort()
			return
		}

		// 检查Authorization格式
		parts := strings.SplitN(authHeader, " ", 2)
		if !(len(parts) == 2 && parts[0] == "Bearer") {
			c.JSON(http.StatusUnauthorized, gin.H{"code": -1, "message": "认证格式错误"})
			c.Abort()
			return
		}

		// 解析JWT token
		claims := jwt.MapClaims{}
		token, err := jwt.ParseWithClaims(parts[1], claims, func(token *jwt.Token) (interface{}, error) {
			return []byte(config.GlobalConfig.JWT.Secret), nil
		})

		if err != nil || !token.Valid {
			c.JSON(http.StatusUnauthorized, gin.H{"code": -1, "message": "无效的token"})
			c.Abort()
			return
		}

		// 将用户信息存储到上下文
		uid, ok := claims["uid"].(string)
		if !ok {
			c.JSON(http.StatusUnauthorized, gin.H{"code": -1, "message": "无效的token格式"})
			c.Abort()
			return
		}

		c.Set("user_id", uid)
		c.Next()
	}
}

// GenerateJWT 生成JWT token
func GenerateJWT(uid string) (string, error) {
	claims := jwt.MapClaims{
		"uid": uid,
		"iss": "netherlink",
		"exp": time.Now().Add(config.GlobalConfig.JWT.Expire).Unix(),
	}
	t := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return t.SignedString([]byte(config.GlobalConfig.JWT.Secret))
}
