# 离线消息同步 API 文档

## 概述

NetherLink 离线消息同步系统提供了完整的离线消息处理机制，包括消息持久化、推送通知、自动同步等功能。

## 功能特性

- ✅ 自动检测用户在线状态
- ✅ 离线消息持久化存储
- ✅ WebSocket 登录后自动同步离线消息
- ✅ 支持分页获取离线消息
- ✅ 批量标记消息已同步
- ✅ 按会话统计未读消息数
- ✅ 可选的推送通知（APNs/FCM/邮件）

## API 接口

### 1. 获取离线消息列表

**接口**: `GET /api/messages/offline`

**认证**: 需要 JWT Token

**请求参数**:
```
page: int (可选, 默认: 1) - 页码
page_size: int (可选, 默认: 50, 最大: 100) - 每页数量
conversation_id: string (可选) - 会话ID，用于过滤特定会话的消息
```

**响应示例**:
```json
{
  "success": true,
  "message": "获取成功",
  "data": {
    "messages": [
      {
        "message_id": 1760163332012743905,
        "conversation_id": "user1_user2",
        "sender_id": "user1",
        "content": "你好",
        "type": "text",
        "extra": "{}",
        "timestamp": "2025-10-11T14:15:32+08:00",
        "created_at": "2025-10-11T14:15:32+08:00"
      }
    ],
    "total": 1,
    "page": 1,
    "page_size": 50
  }
}
```

### 2. 标记消息已同步

**接口**: `POST /api/messages/mark_synced`

**认证**: 需要 JWT Token

**请求体**:
```json
{
  "message_ids": [1760163332012743905, 1760163332012743906]
}
```

**响应示例**:
```json
{
  "success": true,
  "message": "标记成功",
  "data": {
    "updated_count": 2
  }
}
```

### 3. 获取未读消息数量

**接口**: `GET /api/messages/unread_count`

**认证**: 需要 JWT Token

**响应示例**:
```json
{
  "success": true,
  "message": "获取成功",
  "data": {
    "total_unread": 5,
    "unread_by_conversation": [
      {
        "conversation_id": "user1_user2",
        "count": 3
      },
      {
        "conversation_id": "user1_user3",
        "count": 2
      }
    ]
  }
}
```

### 4. 清理已同步的离线消息索引

**接口**: `DELETE /api/messages/offline/clear`

**认证**: 需要 JWT Token

**请求参数**:
```
days: int (可选, 默认: 7) - 保留天数，删除超过该天数的已同步消息索引
```

**响应示例**:
```json
{
  "success": true,
  "message": "清理成功",
  "data": {
    "deleted_count": 10
  }
}
```

## WebSocket 消息

### 离线消息自动同步

当客户端通过 WebSocket 登录成功后，服务器会自动推送离线消息。

**消息类型**: `offline_messages`

**推送时机**: WebSocket 登录成功后自动触发

**消息格式**:
```json
{
  "type": "offline_messages",
  "payload": {
    "messages": [
      {
        "message_id": 1760163332012743905,
        "conversation_id": "user1_user2",
        "sender_id": "user1",
        "content": "你好",
        "type": "text",
        "extra": "{}",
        "timestamp": "2025-10-11T14:15:32+08:00"
      }
    ],
    "count": 1
  }
}
```

**客户端处理流程**:
1. 接收到 `offline_messages` 消息
2. 解析并显示离线消息
3. 调用 `/api/messages/mark_synced` 标记消息已同步

## 消息流程

### 用户在线时

```
发送者 -> WebSocket -> RabbitMQ -> MessageDispatch -> 接收者 WebSocket
                           |
                           v
                    MessagePersistence
                           |
                           v
                       数据库 (message)
```

### 用户离线时

```
发送者 -> WebSocket -> RabbitMQ -> MessageDispatch (检测离线)
                           |                    |
                           v                    v
                    MessagePersistence   OfflineMessageIndex
                           |                    |
                           v                    v
                       数据库 (message)    推送服务 (可选)
                                               |
                                               v
                                      APNs/FCM/邮件通知
```

### 用户上线后

```
客户端 WebSocket 登录
       |
       v
自动同步离线消息 (最多100条)
       |
       v
客户端接收并显示
       |
       v
调用 mark_synced API
       |
       v
更新 offline_message_index.synced = true
```

## 配置

### 邮件推送配置

在 `config.yaml` 中配置：

```yaml
push:
  enabled: true  # 总开关
  apns:
    enabled: false  # iOS推送
  fcm:
    enabled: false  # Android推送
  email:
    enabled: false  # 邮件推送 (默认关闭)
```

### 邮件配置

```yaml
email:
  smtp_host: smtp.qq.com
  smtp_port: 465
  sender: "your@email.com"
  display_name: "NetherLink"
  password: "your_password"
  use_ssl: true
```

## 数据库表

### offline_message_index - 离线消息索引表

```sql
CREATE TABLE offline_message_index (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id VARCHAR(50) NOT NULL,           -- 接收者ID
    message_id BIGINT NOT NULL,             -- 消息ID
    conversation_id VARCHAR(100) NOT NULL,  -- 会话ID
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    synced BOOLEAN DEFAULT FALSE,           -- 是否已同步
    synced_at TIMESTAMP NULL,               -- 同步时间
    INDEX idx_user_synced (user_id, synced, created_at)
);
```

### push_log - 推送日志表

```sql
CREATE TABLE push_log (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id VARCHAR(50) NOT NULL,
    message_id BIGINT NOT NULL,
    push_type VARCHAR(20),                  -- apns/fcm/email
    push_status VARCHAR(20),                -- success/failed
    push_time TIMESTAMP NULL,
    error_msg TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

## 客户端集成示例

### JavaScript/TypeScript

```javascript
// WebSocket 连接
const ws = new WebSocket('ws://localhost:8081/ws');

ws.onopen = () => {
  // 发送登录消息
  ws.send(JSON.stringify({
    type: 'login',
    payload: {
      uid: 'user_id',
      token: 'jwt_token'
    }
  }));
};

ws.onmessage = (event) => {
  const message = JSON.parse(event.data);
  
  // 处理离线消息同步
  if (message.type === 'offline_messages') {
    const { messages, count } = JSON.parse(message.payload);
    console.log(`收到 ${count} 条离线消息`);
    
    // 显示消息
    messages.forEach(msg => {
      displayMessage(msg);
    });
    
    // 标记已同步
    const messageIds = messages.map(m => m.message_id);
    fetch('/api/messages/mark_synced', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${token}`
      },
      body: JSON.stringify({ message_ids: messageIds })
    });
  }
};
```

### 获取离线消息

```javascript
async function getOfflineMessages(page = 1, pageSize = 50) {
  const response = await fetch(
    `/api/messages/offline?page=${page}&page_size=${pageSize}`,
    {
      headers: {
        'Authorization': `Bearer ${token}`
      }
    }
  );
  
  const data = await response.json();
  return data.data.messages;
}
```

### 获取未读数量

```javascript
async function getUnreadCount() {
  const response = await fetch('/api/messages/unread_count', {
    headers: {
      'Authorization': `Bearer ${token}`
    }
  });
  
  const data = await response.json();
  return data.data;
}
```

## 最佳实践

1. **自动同步**: WebSocket 登录后自动接收离线消息，无需手动调用
2. **分页加载**: 使用 HTTP API 分页加载历史离线消息
3. **及时标记**: 消息显示给用户后立即调用 `mark_synced` API
4. **定期清理**: 建议定期调用清理 API 删除已同步的旧索引
5. **错误处理**: 网络异常时应重试同步操作
6. **推送通知**: 生产环境建议启用 APNs/FCM，邮件推送仅作为备选

## 注意事项

- WebSocket 自动同步最多推送 100 条离线消息
- 如果离线消息超过 100 条，需要使用 HTTP API 分页获取
- 已标记同步的消息不会再次推送
- 消息持久化在 `message` 表中，离线消息索引在 `offline_message_index` 表中
- 推送服务为可选功能，可通过配置文件开关控制
