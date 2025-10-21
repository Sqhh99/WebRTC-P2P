package mq

import (
	"NetherLink-server/config"
	"fmt"
	"log"
	"sync"

	"github.com/streadway/amqp"
)

var (
	connection *amqp.Connection
	channel    *amqp.Channel
	once       sync.Once
	mqMutex    sync.RWMutex
)

// InitRabbitMQ 初始化RabbitMQ连接
func InitRabbitMQ() error {
	var err error
	once.Do(func() {
		cfg := config.GlobalConfig.RabbitMQ
		dsn := fmt.Sprintf("amqp://%s:%s@%s:%d%s",
			cfg.Username,
			cfg.Password,
			cfg.Host,
			cfg.Port,
			cfg.VHost,
		)

		connection, err = amqp.Dial(dsn)
		if err != nil {
			log.Printf("Failed to connect to RabbitMQ: %v", err)
			return
		}

		channel, err = connection.Channel()
		if err != nil {
			log.Printf("Failed to open a channel: %v", err)
			return
		}

		// 声明交换机
		err = channel.ExchangeDeclare(
			cfg.Exchange.Chat,
			"direct",
			true,  // durable
			false, // auto-deleted
			false, // internal
			false, // no-wait
			nil,   // arguments
		)
		if err != nil {
			log.Printf("Failed to declare chat exchange: %v", err)
			return
		}

		err = channel.ExchangeDeclare(
			cfg.Exchange.Notification,
			"direct",
			true,
			false,
			false,
			false,
			nil,
		)
		if err != nil {
			log.Printf("Failed to declare notification exchange: %v", err)
			return
		}

		// 声明队列
		queues := []struct {
			name       string
			routingKey string
			exchange   string
		}{
			{cfg.Queue.PrivateMessages, "private.message", cfg.Exchange.Chat},
			{cfg.Queue.GroupMessages, "group.message", cfg.Exchange.Chat},
			{cfg.Queue.OfflineMessages, "offline.message", cfg.Exchange.Chat},
			{cfg.Queue.PushTasks, "push.task", cfg.Exchange.Notification},
		}

		for _, q := range queues {
			_, err = channel.QueueDeclare(
				q.name,
				true,  // durable
				false, // delete when unused
				false, // exclusive
				false, // no-wait
				nil,   // arguments
			)
			if err != nil {
				log.Printf("Failed to declare queue %s: %v", q.name, err)
				return
			}

			err = channel.QueueBind(
				q.name,
				q.routingKey,
				q.exchange,
				false,
				nil,
			)
			if err != nil {
				log.Printf("Failed to bind queue %s: %v", q.name, err)
				return
			}
		}

		log.Println("RabbitMQ initialized successfully")
	})
	return err
}

// GetChannel 获取RabbitMQ通道
func GetChannel() (*amqp.Channel, error) {
	mqMutex.RLock()
	defer mqMutex.RUnlock()

	if channel == nil {
		return nil, fmt.Errorf("RabbitMQ channel is not initialized")
	}
	return channel, nil
}

// PublishMessage 发布消息到指定队列
func PublishMessage(exchange, routingKey string, body []byte) error {
	ch, err := GetChannel()
	if err != nil {
		return err
	}

	return ch.Publish(
		exchange,
		routingKey,
		false, // mandatory
		false, // immediate
		amqp.Publishing{
			ContentType:  "application/json",
			Body:         body,
			DeliveryMode: amqp.Persistent, // 持久化消息
		},
	)
}

// ConsumeMessages 消费指定队列的消息
func ConsumeMessages(queueName string) (<-chan amqp.Delivery, error) {
	ch, err := GetChannel()
	if err != nil {
		return nil, err
	}

	// 设置QoS，每次只处理一条消息
	err = ch.Qos(
		1,     // prefetch count
		0,     // prefetch size
		false, // global
	)
	if err != nil {
		return nil, err
	}

	return ch.Consume(
		queueName,
		"",    // consumer
		false, // auto-ack
		false, // exclusive
		false, // no-local
		false, // no-wait
		nil,   // args
	)
}

// Close 关闭RabbitMQ连接
func Close() {
	mqMutex.Lock()
	defer mqMutex.Unlock()

	if channel != nil {
		channel.Close()
	}
	if connection != nil {
		connection.Close()
	}
}
