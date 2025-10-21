# WebRTC 客户端列表问题修复报告

## 问题描述

### 现象
1. 第一次连接服务器时，两个客户端之间可以正常进行通话
2. 当有一方下线后再次上线，通话列表里面没有另一个仍在线的用户
3. 无法继续发起通话和接收通话

### 用户反馈
> "我开两个客户端如果是第一次连接服务器，两个客户端之间可以正常进行通话，如果有一方下线，这一方再次上线，通话列表里面没有另一个没有上线的用户，也无法继续发起通话和接收"

## 问题分析

### 根本原因

经过代码分析，发现问题出在 **WebRTC 信令服务器的客户端列表同步机制**：

1. **时序问题**：
   - 客户端重新连接时，服务端在 `HandleConnection` 中立即调用 `sendRegisteredMessage`
   - `sendRegisteredMessage` 原本只在最后调用 `broadcastClientList()` 广播给所有客户端
   - 但广播是异步的，可能在新连接的客户端还未完全准备好接收消息时就发送了

2. **客户端列表更新流程**：
   ```
   客户端A重新连接
   -> 服务端 HandleConnection(A)
   -> 添加A到 clients map
   -> 发送 registered 消息给A
   -> 广播 client-list 给所有客户端（包括A和B）
   -> 客户端A收到 registered 消息
   -> 客户端A主动请求 list-clients
   -> 服务端响应 client-list 给A
   ```

3. **原始实现的问题**：
   - 在 `sendRegisteredMessage` 中只调用了 `broadcastClientList()`
   - 这意味着新连接的客户端需要依赖广播或主动请求才能获取客户端列表
   - 如果广播消息在客户端准备好之前到达，可能会丢失

### 代码位置

**文件**: `internal/server/webrtc_signaling.go`

**关键函数**:
- `HandleConnection`: 处理新的WebRTC连接
- `sendRegisteredMessage`: 发送注册确认消息
- `broadcastClientList`: 广播客户端列表给所有客户端
- `sendClientList`: 发送客户端列表给指定客户端

## 解决方案

### 修复内容

修改 `sendRegisteredMessage` 函数，确保新连接的客户端能立即收到完整的客户端列表：

```go
// sendRegisteredMessage 发送注册确认消息
func (s *WebRTCSignalingServer) sendRegisteredMessage(client *WebRTCClient) {
    // ... 发送 registered 消息的代码 ...

    // ✅ 新增：立即发送客户端列表给新连接的客户端
    s.sendClientList(client)
    
    // 广播客户端列表给所有其他客户端
    s.broadcastClientList()
}
```

### 修复原理

1. **立即同步**：新连接的客户端在收到 `registered` 消息后，立即收到一份完整的客户端列表
2. **双重保障**：
   - `sendClientList(client)` 确保新客户端立即获取列表
   - `broadcastClientList()` 通知所有其他客户端有新用户上线
3. **避免竞态条件**：不依赖广播的时序，新客户端直接获取列表

### 额外改进

添加了详细的日志记录，方便调试：

```go
// 在 sendClientList 中
log.Printf("发送客户端列表给 %s，在线用户数: %d", client.uid, len(clients))
for _, c := range clients {
    log.Printf("  - 在线用户: %s", c["id"])
}

// 在 broadcastClientList 中
log.Printf("广播客户端列表，当前在线用户数: %d", len(clients))
```

## 测试建议

### 测试场景

1. **场景1：正常连接**
   - 客户端A连接
   - 客户端B连接
   - 验证：A和B都能看到对方

2. **场景2：重新连接**
   - 客户端A和B都在线
   - 客户端A断开连接
   - 客户端A重新连接
   - 验证：A能看到B，B能看到A

3. **场景3：多次重连**
   - 重复场景2多次
   - 验证：列表始终正确

4. **场景4：同时重连**
   - 客户端A和B同时断开
   - 客户端A和B同时重连
   - 验证：最终双方都能看到对方

### 日志检查

启动服务器后，观察日志输出：
```
WebRTC客户端连接: user_123
发送客户端列表给 user_123，在线用户数: 2
  - 在线用户: user_123
  - 在线用户: user_456
成功发送客户端列表给 user_123
广播客户端列表，当前在线用户数: 2
广播客户端列表成功给 user_123
广播客户端列表成功给 user_456
```

## 相关文件

- `internal/server/webrtc_signaling.go` - WebRTC信令服务器实现
- `client/src/signalclient.cc` - 客户端信令客户端实现
- `client/src/video_call_window.cc` - 客户端UI实现

## 注意事项

1. **WebSocket 连接管理**：
   - 服务端正确处理了重复登录（关闭旧连接）
   - 客户端列表在 `clients map` 中实时维护

2. **消息发送机制**：
   - 使用带缓冲的 channel (`send chan []byte`)
   - 如果缓冲区满，会记录日志但不会阻塞

3. **线程安全**：
   - 使用 `sync.RWMutex` 保护 `clients map`
   - 读操作使用 `RLock`，写操作使用 `Lock`

## 预期结果

修复后，客户端重新连接时应该能够：
1. ✅ 立即看到所有在线用户
2. ✅ 正常发起和接收通话
3. ✅ 多次重连都能保持正常状态
4. ✅ 不会出现"看不到其他用户"的问题

## 版本信息

- 修复日期：2025年10月21日
- 修复文件：`internal/server/webrtc_signaling.go`
- 问题类型：时序问题 / 客户端列表同步
- 严重程度：高（影响核心功能）
