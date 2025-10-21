# WebRTC 客户端重连问题修复

## 问题描述

### 现象
客户端断开连接后重新连接，无法正常接收消息和发起通话。

### 日志分析

```
20:58:11 WebRTC客户端断开: 11
20:58:19 WebRTC信令连接建立: userID=11  ← 重新连接
20:58:23 收到WebRTC消息: type=call-request, from=22, to=11  ← 但客户端11收不到
```

关键问题：**20:58:19 之后没有任何注册消息或客户端列表发送的日志**

## 根本原因

### 原因1：竞态条件（Race Condition）

当客户端快速断开并重新连接时，存在竞态条件：

1. **旧连接断开流程**：
   - `readPump` 检测到连接关闭
   - 调用 `removeClient(client)`
   - 在 `removeClient` 中删除 `clients[uid]` 并 `close(client.send)`

2. **新连接建立流程**：
   - `HandleConnection` 被调用
   - 检查 `clients[uid]` 是否存在
   - 创建新客户端并添加到 `clients[uid]`
   - 启动 `writePump` 和 `readPump`

3. **竞态问题**：
   - 如果新连接在旧连接完全清理之前建立
   - 可能导致：
     - 新客户端的 `send` channel 被旧连接的 `removeClient` 关闭
     - 新连接覆盖旧连接后，旧连接的 `removeClient` 删除了新连接
     - `writePump` 还未启动就尝试发送消息

### 原因2：时序问题

在 `HandleConnection` 中：
```go
go client.writePump()    // 异步启动
go client.readPump()     // 异步启动
s.sendRegisteredMessage(client)  // 立即发送！
```

`sendRegisteredMessage` 可能在 `writePump` 启动之前就执行了，导致消息无法发送。

## 解决方案

### 修复1：防止删除错误的客户端

```go
func (s *WebRTCSignalingServer) removeClient(client *WebRTCClient) {
    s.mutex.Lock()
    // ✅ 只有当 map 中的客户端就是当前这个客户端时才删除
    if existingClient, ok := s.clients[client.uid]; ok && existingClient == client {
        delete(s.clients, client.uid)
        // ... 通知下线和广播
    } else {
        log.Printf("WebRTC客户端断开（但已有新连接）: %s", client.uid)
    }
    s.mutex.Unlock()
    
    // ✅ 安全地关闭 send channel
    defer func() {
        if r := recover(); r != nil {
            log.Printf("关闭 send channel 时发生 panic: %v", r)
        }
    }()
    close(client.send)
}
```

### 修复2：改进重复登录处理

```go
func (s *WebRTCSignalingServer) HandleConnection(conn *websocket.Conn, uid string) {
    // ...
    
    s.mutex.Lock()
    if existingClient, ok := s.clients[uid]; ok {
        // ✅ 先从 map 中删除
        delete(s.clients, uid)
        s.mutex.Unlock()
        
        // ✅ 在锁外关闭旧连接，避免死锁
        go func() {
            existingClient.conn.Close()
            defer func() {
                if r := recover(); r != nil {
                    log.Printf("关闭旧连接 send channel 时发生 panic: %v", r)
                }
            }()
            close(existingClient.send)
        }()
        
        s.mutex.Lock()
    }
    s.clients[uid] = client
    s.mutex.Unlock()
    
    // ...
}
```

### 修复3：延迟发送注册消息

```go
func (s *WebRTCSignalingServer) HandleConnection(conn *websocket.Conn, uid string) {
    // ...
    
    // 启动读写协程
    go client.writePump()
    go client.readPump()

    // ✅ 延迟发送注册消息，确保 writePump 已经启动
    go func() {
        time.Sleep(50 * time.Millisecond)
        s.sendRegisteredMessage(client)
    }()
}
```

### 修复4：添加详细日志

```go
func (s *WebRTCSignalingServer) sendRegisteredMessage(client *WebRTCClient) {
    log.Printf("开始发送注册消息给客户端: %s", client.uid)
    
    // ...
    
    select {
    case client.send <- data:
        log.Printf("成功发送注册消息给客户端: %s", client.uid)
    default:
        log.Printf("客户端发送缓冲区已满: %s", client.uid)
        return
    }
    
    // ...
}
```

## 修复的关键点

1. ✅ **防止竞态条件**：通过指针比较确保只删除正确的客户端
2. ✅ **避免死锁**：在锁外关闭旧连接
3. ✅ **确保时序**：延迟发送注册消息，等待 writePump 启动
4. ✅ **安全关闭 channel**：使用 defer + recover 防止 panic
5. ✅ **详细日志**：方便调试和追踪问题

## 测试步骤

1. 启动服务器
2. 客户端A连接（ID: 11）
3. 客户端B连接（ID: 22）
4. 验证双方能看到对方
5. 客户端A断开连接
6. **立即**重新连接客户端A（ID: 11）
7. 检查日志是否包含：
   ```
   检测到重复登录，关闭旧连接: 11
   WebRTC客户端连接: 11
   开始发送注册消息给客户端: 11
   成功发送注册消息给客户端: 11
   发送客户端列表给 11，在线用户数: 2
   ```
8. 验证客户端B能呼叫客户端A

## 预期结果

- ✅ 客户端重新连接后能立即看到在线用户列表
- ✅ 其他客户端能正常呼叫重连的客户端
- ✅ 不会出现消息丢失或连接异常
- ✅ 日志完整显示整个连接过程

## 相关文件

- `internal/server/webrtc_signaling.go`
  - `HandleConnection()`
  - `removeClient()`
  - `sendRegisteredMessage()`

## 技术要点

### 竞态条件的防范

使用指针比较而不是简单的存在性检查：
```go
// ❌ 错误：可能删除新连接
if _, ok := s.clients[uid]; ok {
    delete(s.clients, uid)
}

// ✅ 正确：只删除当前连接
if existingClient, ok := s.clients[uid]; ok && existingClient == client {
    delete(s.clients, uid)
}
```

### Channel 关闭的安全性

```go
// ✅ 使用 defer + recover 防止重复关闭 panic
defer func() {
    if r := recover(); r != nil {
        log.Printf("关闭 send channel 时发生 panic: %v", r)
    }
}()
close(client.send)
```

### 锁的正确使用

```go
// ✅ 在锁外执行耗时操作
s.mutex.Lock()
delete(s.clients, uid)
s.mutex.Unlock()

// 在锁外关闭连接
go func() {
    existingClient.conn.Close()
}()
```

## 版本信息

- 修复日期：2025年10月21日
- 问题类型：竞态条件 + 时序问题
- 严重程度：高（影响重连功能）
