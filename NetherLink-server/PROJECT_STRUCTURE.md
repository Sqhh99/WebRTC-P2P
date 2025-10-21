# NetherLink-server 项目结构总览

## 📂 目录结构

```
NetherLink-server/
├── main.go                          # 应用程序入口
├── go.mod                           # Go模块依赖
├── go.sum                           # 依赖校验文件
├── start.sh                         # 快速启动脚本 [NEW]
├── docker-compose.yml               # Docker编排文件
│
├── config/                          # 配置模块
│   ├── config.go                    # 配置结构体定义 [UPDATED]
│   └── config.yaml                  # 应用配置文件 [UPDATED]
│
├── internal/                        # 内部业务逻辑
│   ├── middleware/                  # 中间件
│   │   └── auth.go                  # JWT认证中间件
│   │
│   ├── model/                       # 数据模型
│   │   ├── user.go                  # 用户模型
│   │   ├── message.go               # 消息模型
│   │   ├── contact.go               # 联系人模型
│   │   ├── post.go                  # 动态模型
│   │   └── ai_conversation.go       # AI对话模型
│   │
│   ├── server/                      # 服务器核心
│   │   ├── http.go                  # HTTP服务器
│   │   ├── websocket.go             # WebSocket服务器 [UPDATED]
│   │   ├── ai_handler.go            # AI对话处理器
│   │   ├── user_handler.go          # 用户相关API
│   │   ├── contact_handler.go       # 联系人相关API
│   │   ├── post_handler.go          # 动态相关API
│   │   └── websocket/               # WebSocket子模块
│   │       ├── client.go            # 客户端连接
│   │       └── server.go            # 服务器逻辑
│   │
│   └── service/                     # 业务服务层 [NEW]
│       ├── message_persistence.go   # 消息持久化服务
│       ├── message_dispatch.go      # 消息分发服务
│       └── push_service.go          # 推送服务
│
├── pkg/                             # 可复用的包
│   ├── database/                    # 数据库管理
│   │   ├── db.go                    # 数据库接口
│   │   └── mysql.go                 # MySQL实现
│   │
│   ├── mq/                          # 消息队列 [NEW]
│   │   └── rabbitmq.go              # RabbitMQ客户端
│   │
│   ├── cache/                       # 缓存管理 [NEW]
│   │   └── redis.go                 # Redis客户端
│   │
│   └── utils/                       # 工具函数
│       ├── email.go                 # 邮件发送 [UPDATED]
│       └── file.go                  # 文件处理
│
├── migrations/                      # 数据库迁移 [NEW]
│   └── 001_add_message_queue_tables.sql  # 消息队列表
│
├── uploads/                         # 上传文件目录
│   ├── images/                      # 图片文件
│   └── posts/                       # 动态图片
│
├── docs/                            # 文档目录 [NEW]
│   ├── ARCHITECTURE_DESIGN.md       # 架构设计文档
│   ├── DEPLOYMENT_GUIDE.md          # 部署指南
│   ├── UPGRADE_SUMMARY.md           # 升级总结
│   └── new-design.md                # 流程图设计
│
├── README.md                        # 项目说明
├── REFACTOR_README.md               # 重构说明
├── LICENSE                          # 开源协议
└── netherlink.sql                   # 数据库初始化脚本

```

## 📋 文件说明

### 核心入口
- **main.go**: 应用程序启动入口，初始化所有服务和依赖

### 配置层 (config/)
- **config.go**: 定义配置结构体，包含服务器、数据库、RabbitMQ、Redis、推送等配置
- **config.yaml**: YAML格式的配置文件，包含所有运行参数

### 业务逻辑层 (internal/)

#### 中间件 (middleware/)
- **auth.go**: JWT Token验证中间件，保护需要认证的API

#### 数据模型 (model/)
- **user.go**: 用户、好友、群组、群成员等模型
- **message.go**: 聊天消息模型，支持多种消息类型
- **contact.go**: 联系人和会话信息模型
- **post.go**: 社交动态相关模型
- **ai_conversation.go**: AI对话记录模型

#### 服务器 (server/)
- **http.go**: HTTP服务器，提供RESTful API
- **websocket.go**: WebSocket服务器，处理实时通信
- **ai_handler.go**: AI对话处理器，集成Deepseek API
- **user_handler.go**: 用户注册、登录、搜索等API
- **contact_handler.go**: 好友、群组管理API
- **post_handler.go**: 社交动态发布、评论、点赞API

#### 业务服务 (service/) [NEW]
- **message_persistence.go**: 
  - 消费RabbitMQ消息
  - 持久化到MySQL
  - 更新会话列表
  
- **message_dispatch.go**:
  - 查询用户在线状态
  - 在线用户实时推送
  - 离线用户触发推送任务
  
- **push_service.go**:
  - 处理推送任务
  - 支持APNs/FCM/Email多渠道
  - 记录推送日志

### 基础设施层 (pkg/)

#### 数据库 (database/)
- **db.go**: 数据库接口定义
- **mysql.go**: MySQL连接池管理，使用GORM

#### 消息队列 (mq/) [NEW]
- **rabbitmq.go**: 
  - RabbitMQ连接管理
  - 队列声明和绑定
  - 消息发布和消费

#### 缓存 (cache/) [NEW]
- **redis.go**:
  - Redis连接池管理
  - 在线状态管理
  - 未读消息计数
  - 连接ID映射

#### 工具 (utils/)
- **email.go**: 邮件发送工具（SMTP）
- **file.go**: 文件上传和处理

### 数据库迁移 (migrations/) [NEW]
- **001_add_message_queue_tables.sql**: 
  - conversation表（会话列表）
  - offline_message_index表（离线消息索引）
  - push_log表（推送日志）

### 文档 (docs/) [NEW]
- **ARCHITECTURE_DESIGN.md**: 详细的架构设计文档
- **DEPLOYMENT_GUIDE.md**: 完整的部署和安装指南
- **UPGRADE_SUMMARY.md**: 本次升级的总结
- **new-design.md**: 消息流程图和技术细节

## 🔗 模块依赖关系

```
main.go
  ├─> config (配置加载)
  ├─> database (数据库初始化)
  ├─> cache (Redis初始化)
  ├─> mq (RabbitMQ初始化)
  ├─> server.HTTPServer (HTTP服务器)
  ├─> server.WSServer (WebSocket服务器)
  ├─> service.MessagePersistenceService (消息持久化)
  ├─> service.MessageDispatchService (消息分发)
  └─> service.PushService (推送服务)

server.WSServer
  ├─> mq (发布消息到队列)
  ├─> cache (更新在线状态)
  └─> model (数据模型)

service.MessagePersistenceService
  ├─> mq (消费队列消息)
  ├─> database (保存到MySQL)
  └─> model (数据模型)

service.MessageDispatchService
  ├─> mq (消费队列消息)
  ├─> cache (查询在线状态)
  ├─> server.WSServer (推送给在线用户)
  └─> database (记录离线消息)

service.PushService
  ├─> mq (消费推送任务)
  ├─> database (查询用户信息，记录日志)
  └─> utils.email (发送邮件通知)
```

## 🚀 服务启动流程

```
1. main.go 启动
   ↓
2. 加载 config/config.yaml
   ↓
3. 初始化 MySQL 连接池
   ↓
4. 初始化 Redis 连接
   ↓
5. 初始化 RabbitMQ 连接
   ├─ 声明交换机 (chat.direct, notification.direct)
   └─ 声明队列并绑定路由键
   ↓
6. 启动 HTTP 服务器 (端口 8080)
   ├─ 注册路由
   └─ 启动 Gin 引擎
   ↓
7. 启动 WebSocket 服务器 (端口 8081)
   ├─ 监听 /ws 路径
   └─ 管理客户端连接
   ↓
8. 启动消息持久化服务
   └─ 监听 private_messages_queue
   ↓
9. 启动消息分发服务
   └─ 监听 offline_messages_queue
   ↓
10. 启动推送服务
    └─ 监听 push_tasks_queue
    ↓
11. 使用 errgroup 并发运行所有服务
    └─ 任一服务失败，所有服务停止
```

## 📊 数据流向

### 消息发送流程
```
客户端 A
  ↓ (WebSocket)
WebSocket网关 (8081端口)
  ↓ (发布消息)
RabbitMQ (private_messages_queue)
  ↓ (消费)
消息持久化服务
  ↓ (保存)
MySQL (message表, conversation表)
  ↓ (发布分发任务)
RabbitMQ (offline_messages_queue)
  ↓ (消费)
消息分发服务
  ↓ (查询在线状态)
Redis
  ├─ 在线 → WebSocket网关 → 客户端 B (实时)
  └─ 离线 → RabbitMQ (push_tasks_queue) → 推送服务 → 邮件/APNs/FCM
```

### 用户上线流程
```
客户端
  ↓ (WebSocket连接)
WebSocket网关
  ↓ (JWT验证)
middleware.AuthMiddleware
  ↓ (验证通过)
WebSocket网关
  ↓ (更新状态)
Redis (SET online:user:{uid})
  ↓ (维护连接)
内存 (sync.Map connections)
```

### 离线消息同步流程
```
客户端上线
  ↓ (请求同步)
WebSocket网关
  ↓ (查询)
MySQL (offline_message_index表)
  ↓ (获取消息ID列表)
MySQL (message表)
  ↓ (返回完整消息)
WebSocket网关
  ↓ (推送)
客户端 (批量接收)
  ↓ (更新状态)
MySQL (offline_message_index.synced = TRUE)
Redis (清除未读计数)
```

## 🔍 关键技术点

### 1. 并发处理
- **sync.Map**: 存储WebSocket连接，线程安全
- **errgroup**: 并发运行多个服务，任一失败则全部停止
- **goroutine**: 每个连接独立的goroutine处理

### 2. 消息队列
- **RabbitMQ**: 解耦消息发送和处理
- **持久化消息**: 防止消息丢失
- **确认机制**: 消费成功后Ack，失败则Nack重试

### 3. 缓存策略
- **在线状态**: Redis Hash存储，支持心跳更新
- **未读计数**: Redis Hash存储，增量更新
- **TTL**: 自动过期机制，防止内存泄漏

### 4. 数据库优化
- **索引**: 会话查询、消息查询、离线消息查询
- **事务**: 保证消息和会话的一致性
- **批量操作**: 减少数据库IO

## 📝 配置说明

### 必需配置
```yaml
server:
  http.port: 8080              # HTTP服务端口
  websocket.port: 8081         # WebSocket服务端口

database:
  host: localhost              # MySQL地址
  port: 3306                   # MySQL端口
  username: root               # 数据库用户名
  password: 123456             # 数据库密码
  dbname: netherlink           # 数据库名

rabbitmq:
  host: localhost              # RabbitMQ地址
  port: 5672                   # RabbitMQ端口
  username: sqhh99             # RabbitMQ用户名
  password: sqhh99             # RabbitMQ密码

redis:
  host: localhost              # Redis地址
  port: 6379                   # Redis端口
  password: sqhh99             # Redis密码
```

### 可选配置
```yaml
push:
  enabled: true                # 是否启用推送
  email.enabled: true          # 邮件推送
  apns.enabled: false          # iOS推送（需要证书）
  fcm.enabled: false           # Android推送（需要配置）

ai:
  model: gemma-3-4b-it         # AI模型
  api_key: xxx                 # API密钥
  max_history: 10              # 最大历史记录
```

## 🛠️ 开发建议

### 添加新API
1. 在 `internal/server/` 创建handler文件
2. 在 `http.go` 的 `setupRoutes` 添加路由
3. 如需数据库，在 `internal/model/` 定义模型

### 添加新的消息类型
1. 在 `internal/model/message.go` 定义类型
2. 在 `websocket.go` 添加处理逻辑
3. 更新客户端协议文档

### 添加新的推送渠道
1. 在 `config/config.go` 添加配置结构
2. 在 `service/push_service.go` 实现推送方法
3. 更新配置文件示例

## 📚 相关文档

- [README.md](README.md) - 项目介绍和快速开始
- [ARCHITECTURE_DESIGN.md](ARCHITECTURE_DESIGN.md) - 详细架构设计
- [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md) - 部署和运维指南
- [UPGRADE_SUMMARY.md](UPGRADE_SUMMARY.md) - 升级总结
- [new-design.md](new-design.md) - 消息流程图

---

**最后更新**: 2025年10月11日  
**版本**: v2.0.0
