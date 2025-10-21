# 🔧 紧急修复 - WebRTC路由404问题

## 问题描述

客户端连接 `ws://localhost:8081/ws/webrtc` 返回404错误。

## 原因

WebRTC路由被错误地添加到HTTP服务器(8080端口)，而不是WebSocket服务器(8081端口)。

## 已修复

✅ 已将WebRTC路由移至正确的WebSocket服务器

### 修改的文件

1. **`internal/server/websocket.go`**
   - 添加 `webrtcSignalServer` 字段到 `WSServer`
   - 在 `setupRoutes()` 中添加 `/ws/webrtc` 路由
   - 添加 `handleWebRTCWebSocket()` 方法

2. **`internal/server/http.go`**
   - 移除 `webrtcSignalServer` 字段
   - 移除WebRTC相关的handler方法
   - 移除WebRTC路由

## 测试步骤

### 1. 停止当前服务器

按 `Ctrl+C` 停止正在运行的服务器

### 2. 重新启动服务器

```bash
cd NetherLink-server
go run main.go
```

### 3. 检查启动日志

应该看到:
```
[GIN-debug] GET    /ws                       --> ...
[GIN-debug] GET    /ws/webrtc                --> ...  ← 新增路由
[GIN-debug] Listening and serving HTTP on :8081
```

### 4. 重新运行客户端

```bash
cd client/build/Release
./peerconnection_client.exe
```

### 5. 连接测试

- 服务器地址: `ws://localhost:8081/ws/webrtc`
- 客户端ID: 输入任意ID(如 `alice`)
- 点击"连接"

**预期结果**: 
- ✅ 客户端显示"已连接"
- ✅ 服务器日志显示: `WebRTC信令连接建立: userID=alice`
- ✅ 客户端收到ICE服务器配置

## 验证

连接成功后,服务器应该输出:

```
WebRTC客户端连接: alice
Received 7 ICE server configurations
```

客户端应该显示:

```
WebSocket connected
Client registered successfully
Received 7 ICE server configurations
```

## 如果仍然失败

1. **检查端口占用**:
   ```bash
   netstat -ano | findstr :8081
   ```

2. **查看完整错误日志**:
   - 服务器端日志
   - 客户端控制台输出

3. **清理并重新编译**:
   ```bash
   # 服务器
   go clean
   go build
   
   # 客户端
   cd client/build
   cmake --build . --config Release --clean-first
   ```

## 正确的架构

```
端口 8080 (HTTP服务器)
├── /api/*              ← REST API
└── /ws/ai              ← AI对话WebSocket

端口 8081 (WebSocket服务器)
├── /ws                 ← 聊天WebSocket
└── /ws/webrtc          ← WebRTC信令 ✅ 现在在这里
```

## 更新日志

**修复时间**: 2025年10月16日 23:20  
**问题**: WebRTC路由404  
**解决**: 移动路由到WebSocket服务器  
**影响**: 无破坏性变更,仅修复问题

---

**现在请重启服务器并测试!** 🚀
