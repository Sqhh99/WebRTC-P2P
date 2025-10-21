# NetherLink 消息交互流程图

## 完整消息交互流程

```mermaid
sequenceDiagram
    participant Client as 客户端
    participant WS as WebSocket服务器
    participant HTTP as HTTP服务器
    participant MQ as RabbitMQ消息队列
    participant Persistence as 消息持久化服务
    participant Dispatch as 消息分发服务
    participant Redis as Redis缓存
    participant DB as MySQL数据库
    participant Push as 推送服务

    %% 1. 用户登录流程
    rect rgb(240, 248, 255)
        Note over Client, DB: 用户登录认证流程
        Client->>WS: WebSocket连接建立
        Client->>WS: 发送登录消息 (type: "login")
        WS->>WS: JWT令牌验证
        WS->>Redis: 设置用户在线状态
        WS->>Client: 登录成功响应
    end

    %% 2. 发送消息流程
    rect rgb(255, 248, 220)
        Note over Client, Push: 消息发送和处理流程（异步持久化）
        Client->>WS: 发送聊天消息 (type: "chat")
        WS->>WS: 消息验证和预处理
        WS->>MQ: 立即发布消息到队列
        WS->>Client: 立即返回发送成功确认
        
        Note over MQ, DB: 异步持久化流程
        MQ->>Persistence: 消息持久化服务消费消息
        Persistence->>DB: 保存消息到数据库
        Persistence->>MQ: 转发到离线消息队列
    end

    %% 3. 消息分发流程
    rect rgb(240, 255, 240)
        Note over MQ, Push: 异步消息分发流程
        MQ->>Dispatch: 消费消息
        Dispatch->>Redis: 检查接收者在线状态

        alt 用户在线
            Dispatch->>WS: 通过WebSocket推送消息
            WS->>Client: 实时消息推送
        else 用户离线
            Dispatch->>Redis: 增加未读消息计数
            Dispatch->>DB: 记录离线消息索引
            Dispatch->>MQ: 发送推送任务
            MQ->>Push: 消费推送任务
            Push->>Push: 根据配置选择推送方式
            alt APNs推送
                Push->>Push: 发送iOS推送通知
            else FCM推送
                Push->>Push: 发送Android推送通知
            else 邮件推送
                Push->>Push: 发送邮件通知
            end
        end
    end

    %% 4. 离线消息同步流程
    rect rgb(255, 240, 245)
        Note over Client, Redis: 离线消息同步流程（WebSocket确认机制）
        Client->>WS: 请求同步离线消息 (type: "sync_offline_messages")
        WS->>DB: 查询未同步的离线消息
        DB-->>WS: 返回离线消息列表
        WS->>Client: 发送离线消息数据
        
        Note over Client, Redis: 客户端处理完消息后确认
        Client->>WS: 确认消息已同步 (type: "confirm_messages_synced")
        WS->>DB: 更新离线消息索引状态
        WS->>Redis: 清除未读消息计数
        WS->>Client: 确认响应
    end

    %% 5. HTTP API查询流程
    rect rgb(220, 240, 255)
        Note over Client, DB: HTTP API查询流程
        Client->>HTTP: 获取离线消息 (GET /api/messages/offline)
        HTTP->>DB: 分页查询离线消息
        DB-->>HTTP: 返回消息数据
        HTTP->>Client: 返回离线消息列表

        Client->>HTTP: 获取未读计数 (GET /api/messages/unread-count)
        HTTP->>Redis: 查询未读消息统计
        Redis-->>HTTP: 返回未读数量
        HTTP->>Client: 返回未读消息统计
    end
```

## 核心组件职责

### 🖥️ WebSocket服务器
- 处理实时消息收发
- 用户认证和连接管理
- 离线消息同步和确认处理
- 零延迟消息发送体验

### � 消息持久化服务
- 异步消息持久化
- 消息格式验证
- 会话状态更新
- 消息去重处理

### ⚙️ 消息分发服务
- 消息路由决策（在线/离线）
- 离线消息索引记录
- 推送任务触发

### 🌐 HTTP服务器
- RESTful API接口
- 离线消息查询
- 用户信息和动态管理

### 📨 RabbitMQ消息队列
- 异步消息处理
- 解耦消息发送和处理
- 推送任务队列
- 消息暂存缓冲

### � Redis缓存
- 用户在线状态管理
- 未读消息计数缓存
- 会话状态存储

### �🗄️ MySQL数据库
- 消息持久化存储
- 离线消息索引管理
- 用户和会话数据

### 📢 推送服务
- 多渠道推送通知
- APNs (iOS) / FCM (Android) / Email
- 推送配置管理

## 消息状态流转

```mermaid
stateDiagram-v2
    [*] --> 发送中: 客户端发送消息
    发送中 --> 队列中: 消息进入MQ队列（立即返回确认）
    队列中 --> 持久化中: 持久化服务消费消息
    持久化中 --> 已保存: 消息保存到数据库
    已保存 --> 待分发: 进入分发队列
    待分发 --> 在线推送: 用户在线
    待分发 --> 离线处理: 用户离线

    在线推送 --> 已送达: WebSocket推送成功
    已送达 --> [*]

    离线处理 --> 索引记录: 记录离线消息索引
    索引记录 --> 推送通知: 发送推送任务
    推送通知 --> 待同步: 等待客户端同步

    待同步 --> 同步中: 客户端请求同步
    同步中 --> 等待确认: 消息推送给客户端
    等待确认 --> 已同步: 客户端发送确认消息
    已同步 --> [*]

    note right of 队列中
        异步处理:
        - 消息暂存在MQ
        - 客户端立即收到确认
        - 数据库不可用时仍能发送
    end note

    note right of 待同步
        未同步状态:
        - 离线消息索引存在
        - Redis未读计数>0
    end note

    note right of 已同步
        已同步状态:
        - 客户端发送confirm_messages_synced
        - 离线消息索引标记synced=true
        - Redis未读计数清零
    end note
```

## 数据流说明

### 消息发送流程
1. **客户端** → **WebSocket** → **消息队列**（立即返回确认）
2. **消息队列** → **持久化服务** → **数据库**（异步保存）
3. **持久化服务** → **消息队列** → **分发服务** → **在线判断**
4. **在线**: WebSocket推送 → **客户端**
5. **离线**: 记录索引 → 推送通知 → **客户端设备**

### 离线消息同步流程
1. **客户端** → **WebSocket** → **数据库查询**
2. **数据库** → **WebSocket** → **客户端**
3. **客户端处理完消息** → **WebSocket确认** → **数据库更新**
4. **数据库** → **Redis更新** → **客户端确认**

## 关键设计特点

### 🔄 异步处理
- 消息发送零延迟响应（立即返回确认）
- 消息持久化异步执行
- 推送通知异步执行
- 极致的用户体验

### 📱 统一WebSocket通道
- 所有实时操作通过WebSocket完成
- 消息同步和确认使用同一连接
- 降低连接开销和复杂性
- 提升通信效率

### 🛡️ 可靠性保证
- 消息队列暂存保证不丢失
- 消息持久化存储
- 离线消息索引跟踪
- 失败重试机制
- 数据库不可用时仍可发送消息

### ⚡ 性能优化
- 消息发送无需等待数据库
- Redis缓存热点数据
- 消息队列削峰填谷
- 分页查询大数据集
- 持久化和分发并行处理</content>
<parameter name="filePath">/home/sqhh99/workspace/cpp-workspace/ch-server/NetherLink-server/MESSAGE_FLOW_DIAGRAM.md