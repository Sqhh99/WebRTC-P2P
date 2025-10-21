package config

import (
	"time"

	"github.com/spf13/viper"
)

type Config struct {
	Server   ServerConfig   `mapstructure:"server"`
	Database DatabaseConfig `mapstructure:"database"`
	JWT      JWTConfig      `mapstructure:"jwt"`
	AI       AIConfig       `mapstructure:"ai"`
	Email    EmailConfig    `mapstructure:"email"`
	Image    ImageConfig    `mapstructure:"image"`
	RabbitMQ RabbitMQConfig `mapstructure:"rabbitmq"`
	Redis    RedisConfig    `mapstructure:"redis"`
	Push     PushConfig     `mapstructure:"push"`
}

type ServerConfig struct {
	HTTP      HTTPConfig      `mapstructure:"http"`
	WebSocket WebSocketConfig `mapstructure:"websocket"`
}

type HTTPConfig struct {
	Port    int    `mapstructure:"port"`
	Mode    string `mapstructure:"mode"`
	BaseURL string `mapstructure:"base_url"`
}

type WebSocketConfig struct {
	Port int `mapstructure:"port"`
}

type DatabaseConfig struct {
	Driver       string `mapstructure:"driver"`
	Host         string `mapstructure:"host"`
	Port         int    `mapstructure:"port"`
	Username     string `mapstructure:"username"`
	Password     string `mapstructure:"password"`
	DBName       string `mapstructure:"dbname"`
	Charset      string `mapstructure:"charset"`
	ParseTime    bool   `mapstructure:"parse_time"`
	Loc          string `mapstructure:"loc"`
	MaxIdleConns int    `mapstructure:"max_idle_conns"`
	MaxOpenConns int    `mapstructure:"max_open_conns"`
}

type JWTConfig struct {
	Secret string        `mapstructure:"secret"`
	Expire time.Duration `mapstructure:"expire"`
}

type AIConfig struct {
	Model      string `mapstructure:"model"`
	APIKey     string `mapstructure:"api_key"`
	MaxHistory int    `mapstructure:"max_history"`
}

type EmailConfig struct {
	SMTPHost    string `mapstructure:"smtp_host"`
	SMTPPort    int    `mapstructure:"smtp_port"`
	Sender      string `mapstructure:"sender"`
	DisplayName string `mapstructure:"display_name"`
	Password    string `mapstructure:"password"`
	UseSSL      bool   `mapstructure:"use_ssl"`
}

type ImageConfig struct {
	UploadDir string `mapstructure:"upload_dir"`
	URLPrefix string `mapstructure:"url_prefix"`
}

type RabbitMQConfig struct {
	Host     string                 `mapstructure:"host"`
	Port     int                    `mapstructure:"port"`
	Username string                 `mapstructure:"username"`
	Password string                 `mapstructure:"password"`
	VHost    string                 `mapstructure:"vhost"`
	Exchange RabbitMQExchangeConfig `mapstructure:"exchange"`
	Queue    RabbitMQQueueConfig    `mapstructure:"queue"`
}

type RabbitMQExchangeConfig struct {
	Chat         string `mapstructure:"chat"`
	Notification string `mapstructure:"notification"`
}

type RabbitMQQueueConfig struct {
	PrivateMessages string `mapstructure:"private_messages"`
	GroupMessages   string `mapstructure:"group_messages"`
	OfflineMessages string `mapstructure:"offline_messages"`
	PushTasks       string `mapstructure:"push_tasks"`
}

type RedisConfig struct {
	Host       string `mapstructure:"host"`
	Port       int    `mapstructure:"port"`
	Password   string `mapstructure:"password"`
	DB         int    `mapstructure:"db"`
	PoolSize   int    `mapstructure:"pool_size"`
	MaxRetries int    `mapstructure:"max_retries"`
}

type PushConfig struct {
	Enabled bool            `mapstructure:"enabled"`
	APNS    APNSConfig      `mapstructure:"apns"`
	FCM     FCMConfig       `mapstructure:"fcm"`
	Email   PushEmailConfig `mapstructure:"email"`
}

type APNSConfig struct {
	Enabled    bool   `mapstructure:"enabled"`
	KeyID      string `mapstructure:"key_id"`
	TeamID     string `mapstructure:"team_id"`
	BundleID   string `mapstructure:"bundle_id"`
	KeyPath    string `mapstructure:"key_path"`
	Production bool   `mapstructure:"production"`
}

type FCMConfig struct {
	Enabled   bool   `mapstructure:"enabled"`
	ServerKey string `mapstructure:"server_key"`
}

type PushEmailConfig struct {
	Enabled bool `mapstructure:"enabled"`
}

var GlobalConfig Config

func Init() error {
	viper.SetConfigName("config")
	viper.SetConfigType("yaml")
	viper.AddConfigPath("./config")

	if err := viper.ReadInConfig(); err != nil {
		return err
	}

	if err := viper.Unmarshal(&GlobalConfig); err != nil {
		return err
	}

	return nil
}
