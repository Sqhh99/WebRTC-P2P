# WebRTC 客户端项目文档

## 项目概述

这是一个基于 WebRTC 的音视频通话客户端，使用 C++ 和 Qt 6 开发，支持点对点的音视频通信。

### 技术栈
- **WebRTC**: Google 开源的实时音视频通信库 (版本 111)
- **Qt 6.6.3**: 跨平台 GUI 框架
- **WebSocket**: 信令通信协议
- **CMake**: 构建系统
- **C++20**: 编程语言标准

### 架构特点
- **分层架构**: UI 层、连接层、WebRTC 引擎层完全解耦
- **观察者模式**: 各层通过观察者接口通信
- **状态管理**: 完善的呼叫状态机管理
- **线程安全**: 使用 Qt 的信号槽机制确保线程安全

---

## 项目结构

```
client/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 基本说明
├── PROJECT_GUIDE.md            # 本文档
├── include/                    # 头文件目录
│   ├── webrtcengine.h         # WebRTC 引擎接口
│   ├── conductor.h            # 连接层协调器
│   ├── mainwindow.h           # 主窗口界面
│   ├── signalclient.h         # 信令客户端
│   ├── callmanager.h          # 呼叫管理器
│   ├── videorenderer.h        # 视频渲染器
│   └── ...
├── src/                        # 源文件目录
│   ├── webrtcengine.cc        # WebRTC 引擎实现
│   ├── conductor.cc           # 连接层实现
│   ├── mainwindow.cc          # 主窗口实现
│   ├── signalclient.cc        # 信令客户端实现
│   ├── callmanager.cc         # 呼叫管理器实现
│   ├── videorenderer.cc       # 视频渲染器实现
│   ├── test_impl.cc           # 测试工具实现
│   └── main.cc                # 程序入口
├── test/                       # 测试相关文件
│   ├── frame_generator.*      # 视频帧生成器
│   ├── vcm_capturer.*         # 视频捕获器
│   └── ...
└── webrtc/                     # 信令服务器 (Go)
    ├── main.go                # 服务器主程序
    └── static/                # Web 客户端
```

---

## 核心组件说明

### 1. WebRTCEngine (webrtcengine.h/.cc)
**职责**: 封装所有 WebRTC 核心功能，与 UI 完全解耦

**主要功能**:
- 管理 PeerConnection 生命周期
- 处理 SDP (offer/answer) 创建和设置
- 管理 ICE 候选
- 添加本地音视频轨道
- 接收远程音视频轨道

**关键接口**:
```cpp
class WebRTCEngine {
public:
    bool Initialize();
    bool CreatePeerConnection();
    void ClosePeerConnection();
    bool AddTracks();
    void CreateOffer();
    void CreateAnswer();
    void SetRemoteOffer(const std::string& sdp);
    void SetRemoteAnswer(const std::string& sdp);
    void AddIceCandidate(const std::string& sdp_mid, int sdp_mline_index, const std::string& candidate);
};
```

### 2. Conductor (conductor.h/.cc)
**职责**: 连接层，协调 WebRTC 引擎、信令客户端和呼叫管理器

**主要功能**:
- 实现 WebRTCEngineObserver 接口，处理 WebRTC 事件
- 实现 SignalClientObserver 接口，处理信令消息
- 实现 CallManagerObserver 接口，处理呼叫状态变化
- 协调各组件间的通信

### 3. SignalClient (signalclient.h/.cc)
**职责**: WebSocket 信令通信客户端

**主要功能**:
- 管理 WebSocket 连接
- 发送和接收信令消息
- 自动重连机制
- 消息序列化/反序列化

### 4. CallManager (callmanager.h/.cc)
**职责**: 呼叫状态管理

**主要功能**:
- 管理呼叫状态机
- 处理呼叫请求、接受、拒绝、结束
- 呼叫超时检测
- 通知观察者状态变化

**呼叫状态**:
```cpp
enum class CallState {
    Idle,        // 空闲
    Calling,     // 正在呼叫
    Receiving,   // 接收呼叫
    Connecting,  // 连接中
    Connected,   // 已连接
    Ended        // 已结束
};
```

### 5. MainWnd (mainwindow.h/.cc)
**职责**: Qt 主窗口界面

**主要功能**:
- 显示本地和远程视频
- 客户端列表管理
- 呼叫控制按钮
- 日志显示

### 6. VideoRenderer (videorenderer.h/.cc)
**职责**: 视频流渲染

**主要功能**:
- 接收 WebRTC 视频帧
- 转换为 Qt 可显示格式
- 实时渲染到界面

---

## 信令协议规范

### WebSocket 连接
- **地址**: `ws://localhost:8080/ws`
- **协议**: WebSocket
- **消息格式**: JSON

### 消息基本结构
```json
{
    "type": "message-type",
    "from": "sender-client-id",
    "to": "receiver-client-id",
    "payload": {}
}
```

---

## API 消息格式详解

### 1. 客户端注册
**类型**: `register`
**方向**: Client → Server
**说明**: 客户端连接后首先发送注册消息

```json
{
    "type": "register",
    "from": "qt_client_123456",
    "to": "",
    "payload": null
}
```

**响应**: `registered`
```json
{
    "type": "registered",
    "from": "qt_client_123456",
    "to": "",
    "payload": null
}
```

---

### 2. 获取客户端列表
**类型**: `list-clients`
**方向**: Client → Server
**说明**: 请求当前在线的所有客户端列表

```json
{
    "type": "list-clients",
    "from": "qt_client_123456",
    "to": "",
    "payload": null
}
```

**响应**: `client-list`
```json
{
    "type": "client-list",
    "from": "",
    "to": "",
    "payload": {
        "clients": [
            {"id": "qt_client_123456"},
            {"id": "qt_client_789012"}
        ]
    }
}
```

---

### 3. 呼叫请求
**类型**: `call-request`
**方向**: Client A → Server → Client B
**说明**: 发起呼叫，请求与另一个客户端建立连接

```json
{
    "type": "call-request",
    "from": "qt_client_123456",
    "to": "qt_client_789012",
    "payload": {
        "timestamp": 1760468361609
    }
}
```

---

### 4. 呼叫响应
**类型**: `call-response`
**方向**: Client B → Server → Client A
**说明**: 接收方响应呼叫请求（接受或拒绝）

**接受呼叫**:
```json
{
    "type": "call-response",
    "from": "qt_client_789012",
    "to": "qt_client_123456",
    "payload": {
        "accepted": true,
        "reason": ""
    }
}
```

**拒绝呼叫**:
```json
{
    "type": "call-response",
    "from": "qt_client_789012",
    "to": "qt_client_123456",
    "payload": {
        "accepted": false,
        "reason": "用户拒绝"
    }
}
```

---

### 5. 发送 Offer (SDP)
**类型**: `offer`
**方向**: Client A → Server → Client B
**说明**: 主叫方创建并发送 SDP Offer

```json
{
    "type": "offer",
    "from": "qt_client_123456",
    "to": "qt_client_789012",
    "payload": {
        "type": "offer",
        "sdp": "v=0\r\no=- 1234567890 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=group:BUNDLE 0 1\r\na=msid-semantic: WMS stream_id\r\nm=video 9 UDP/TLS/RTP/SAVPF 96 97\r\nc=IN IP4 0.0.0.0\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=ice-ufrag:xxxx\r\na=ice-pwd:xxxx\r\n..."
    }
}
```

---

### 6. 发送 Answer (SDP)
**类型**: `answer`
**方向**: Client B → Server → Client A
**说明**: 被叫方创建并发送 SDP Answer

```json
{
    "type": "answer",
    "from": "qt_client_789012",
    "to": "qt_client_123456",
    "payload": {
        "type": "answer",
        "sdp": "v=0\r\no=- 9876543210 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=group:BUNDLE 0 1\r\na=msid-semantic: WMS stream_id\r\nm=video 9 UDP/TLS/RTP/SAVPF 96 97\r\nc=IN IP4 0.0.0.0\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=ice-ufrag:yyyy\r\na=ice-pwd:yyyy\r\n..."
    }
}
```

---

### 7. ICE 候选交换
**类型**: `ice-candidate`
**方向**: Client ↔ Server ↔ Client
**说明**: 交换 ICE 候选以建立 P2P 连接

```json
{
    "type": "ice-candidate",
    "from": "qt_client_123456",
    "to": "qt_client_789012",
    "payload": {
        "candidate": "candidate:1 1 UDP 2130706431 192.168.1.100 54321 typ host",
        "sdpMid": "0",
        "sdpMLineIndex": 0
    }
}
```

---

### 8. 取消呼叫
**类型**: `call-cancel`
**方向**: Client A → Server → Client B
**说明**: 主叫方在对方接听前取消呼叫

```json
{
    "type": "call-cancel",
    "from": "qt_client_123456",
    "to": "qt_client_789012",
    "payload": {
        "reason": "用户取消"
    }
}
```

---

### 9. 结束呼叫
**类型**: `call-end`
**方向**: Client → Server → Client
**说明**: 任意一方结束通话

```json
{
    "type": "call-end",
    "from": "qt_client_123456",
    "to": "qt_client_789012",
    "payload": {
        "reason": "通话结束"
    }
}
```

---

### 10. 用户下线通知
**类型**: `user-offline`
**方向**: Server → All Clients
**说明**: 服务器通知某个客户端已下线

```json
{
    "type": "user-offline",
    "from": "server",
    "to": "",
    "payload": {
        "clientId": "qt_client_789012"
    }
}
```

---

## 呼叫流程

### 完整呼叫流程图

```
主叫方 (Client A)          信令服务器           被叫方 (Client B)
     |                         |                       |
     |---- call-request ------>|                       |
     |                         |---- call-request ---->|
     |                         |                       | [用户点击接听]
     |                         |<--- call-response ----|
     |<--- call-response ------|  (accepted: true)     |
     |                         |                       |
     | [创建 PeerConnection]   |   [创建 PeerConnection]|
     | [添加音视频轨道]        |     [添加音视频轨道]  |
     | [创建 Offer]            |                       |
     |                         |                       |
     |------ offer SDP ------->|                       |
     |                         |------ offer SDP ----->|
     |                         |                       | [设置远程 Offer]
     |                         |                       | [创建 Answer]
     |                         |<----- answer SDP -----|
     |<----- answer SDP -------|                       |
     | [设置远程 Answer]       |                       |
     |                         |                       |
     |<-- ICE candidates ----->|<-- ICE candidates --->|
     |                         |                       |
     |                                                 |
     |<=============== P2P 连接建立 =================>|
     |                                                 |
     |<=========== 音视频流直接传输 ================>|
     |                                                 |
```

### 详细步骤说明

#### 阶段 1: 呼叫建立
1. **Client A** 选择 **Client B** 并点击"呼叫"按钮
2. **Client A** 发送 `call-request` 到服务器
3. 服务器转发 `call-request` 到 **Client B**
4. **Client B** 弹出来电对话框

#### 阶段 2: 接受呼叫
5. **Client B** 用户点击"接听"
6. **Client B** 发送 `call-response` (accepted: true)
7. 服务器转发响应到 **Client A**
8. 双方都创建 PeerConnection 并添加音视频轨道

#### 阶段 3: SDP 交换
9. **Client A** (主叫方) 创建 Offer SDP
10. **Client A** 发送 `offer` 消息
11. **Client B** 接收 Offer 并设置为远程描述
12. **Client B** 创建 Answer SDP
13. **Client B** 发送 `answer` 消息
14. **Client A** 接收 Answer 并设置为远程描述

#### 阶段 4: ICE 协商
15. 双方生成 ICE 候选并通过信令服务器交换
16. WebRTC 尝试各种网络路径建立连接
17. 连接建立成功，开始 P2P 音视频传输

#### 阶段 5: 通话中
18. 音视频数据直接在两个客户端之间传输
19. 不经过信令服务器（P2P）

#### 阶段 6: 结束通话
20. 任意一方点击"挂断"按钮
21. 发送 `call-end` 消息
22. 双方关闭 PeerConnection
23. 清理资源，返回空闲状态

---

## 构建和运行

### 环境要求
- **操作系统**: Windows 10/11
- **编译器**: MSVC 2019 或更新版本
- **CMake**: 3.15 或更新版本
- **Qt**: 6.6.3
- **WebRTC**: 预编译库 (版本 111)
- **Go**: 1.19+ (用于信令服务器)

### 构建步骤

1. **配置 CMake**:
```bash
mkdir build
cd build
cmake ..
```

2. **编译项目**:
```bash
cmake --build . --config Release
```

3. **运行客户端**:
```bash
.\Release\peerconnection_client.exe
```

### 启动信令服务器

```bash
cd webrtc
go run main.go
```

服务器将在 `http://localhost:8080` 启动

---

## 使用说明

### 启动流程
1. 启动信令服务器
2. 启动第一个客户端实例
3. 启动第二个客户端实例
4. 客户端自动连接到信令服务器并注册

### 发起呼叫
1. 在客户端列表中看到其他在线客户端
2. 选择要呼叫的客户端
3. 点击"呼叫"按钮
4. 等待对方接听

### 接听呼叫
1. 收到来电时会弹出对话框
2. 点击"接听"接受呼叫
3. 点击"拒绝"拒绝呼叫

### 结束通话
- 点击"挂断"按钮结束通话

---

## 故障排除

### 常见问题

#### 1. 连接失败
- 检查信令服务器是否运行
- 检查 WebSocket 连接地址是否正确
- 查看防火墙设置

#### 2. 无视频显示
- 检查摄像头权限
- 确认视频轨道已添加
- 查看日志输出

#### 3. ICE 连接失败
- 检查 STUN 服务器是否可访问
- 如果在 NAT 后面，可能需要 TURN 服务器
- 查看 ICE 连接状态日志

#### 4. 客户端崩溃
- 检查 WebRTC 库版本是否匹配
- 查看崩溃日志
- 确保所有依赖库正确安装

---

## 代码规范

### 命名约定
- **类名**: PascalCase (例如: `WebRTCEngine`, `CallManager`)
- **成员变量**: snake_case 后缀下划线 (例如: `peer_connection_`)
- **函数名**: PascalCase (例如: `CreateOffer()`)
- **常量**: UPPER_SNAKE_CASE (例如: `MAX_RETRY_COUNT`)

### 日志使用
```cpp
RTC_LOG(LS_INFO) << "信息日志";
RTC_LOG(LS_WARNING) << "警告日志";
RTC_LOG(LS_ERROR) << "错误日志";
```

### 线程安全
- UI 操作必须使用 `QMetaObject::invokeMethod` 投递到主线程
- WebRTC 回调在 signaling 线程执行
- 使用 Qt 信号槽机制确保线程安全

---

## 扩展功能建议

### 可以添加的功能
1. **屏幕共享**: 添加屏幕捕获和共享功能
2. **群组通话**: 支持多人音视频会议
3. **文字聊天**: 通过 DataChannel 实现文字消息
4. **录制功能**: 录制通话音视频
5. **美颜滤镜**: 添加视频特效处理
6. **网络质量显示**: 显示带宽、延迟等信息
7. **通话统计**: 显示详细的 WebRTC 统计数据
8. **TURN 服务器**: 添加中继服务器支持穿透严格 NAT

---

## 许可证

本项目使用 BSD-3-Clause 许可证，与 WebRTC 项目保持一致。

---

## 联系方式

如有问题或建议，请通过以下方式联系：
- 提交 Issue
- Pull Request
- 邮件联系

---

## 更新日志

### v1.0.0 (2025-10-15)
- ✅ 完成核心 WebRTC 功能
- ✅ 实现分层架构重构
- ✅ 修复接收端崩溃问题
- ✅ 完善信令协议
- ✅ 添加呼叫状态管理
- ✅ 实现视频渲染
- ✅ 支持音频通话

---

**项目状态**: ✅ 生产可用

最后更新: 2025年10月15日
