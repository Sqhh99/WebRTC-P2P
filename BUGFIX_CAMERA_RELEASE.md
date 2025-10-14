# 摄像头占用和程序后台运行问题修复

## 问题描述

在挂断电话后，程序仍然占用摄像头，退出程序后程序依然在后台运行，摄像头也被占用。

## 问题原因分析

### 1. 摄像头占用问题
- **根本原因**：挂断电话时，虽然调用了 `ClosePeerConnection()`，但视频渲染器（VideoRenderer）没有及时停止，导致对视频轨道（VideoTrack）的引用未释放
- **引用链**：VideoRenderer → VideoTrack → VideoSource → VideoCapturer (摄像头)
- **问题表现**：即使关闭了 PeerConnection，由于渲染器还持有 VideoTrack 的引用，导致摄像头采集器无法正常销毁

### 2. 程序后台运行问题
- **根本原因**：退出程序时，WebRTC 的工作线程和资源没有完全释放
- **问题表现**：关闭窗口后，进程仍在后台运行，摄像头依然被占用

## 修复方案

### 1. 挂断时停止视频渲染器 (mainwindow.cc)

**修改位置**：`MainWnd::OnHangupButtonClicked()`
```cpp
void MainWnd::OnHangupButtonClicked() {
  // 先停止本地视频渲染器
  StopLocalRenderer();
  
  call_manager_->EndCall();
  AppendLog("通话已挂断", "info");
}
```

### 2. 关闭窗口时完整清理资源 (mainwindow.cc)

**修改位置**：`MainWnd::closeEvent()`
```cpp
void MainWnd::closeEvent(QCloseEvent* event) {
  // 停止所有视频渲染器
  StopLocalRenderer();
  StopRemoteRenderer();
  
  // 如果正在通话，先挂断
  if (call_manager_->IsInCall()) {
    call_manager_->EndCall();
  }
  
  // 断开信令连接
  if (signal_client_->IsConnected()) {
    signal_client_->Disconnect();
  }
  
  // 注意: conductor_->Shutdown() 会在 main.cc 的清理代码中调用
  // 不在这里调用是为了避免头文件循环依赖
  
  event->accept();
}
```

**重要说明**：`Conductor::Shutdown()` 在 `main.cc` 中的清理代码调用，不在窗口关闭事件中调用，以避免头文件循环依赖问题。

### 3. 在所有关闭连接的地方停止视频渲染器 (conductor.cc)

**修改位置**：
- `Conductor::OnCallRejected()`
- `Conductor::OnCallCancelled()`
- `Conductor::OnCallEnded()`
- `Conductor::OnCallTimeout()`
- `Conductor::OnNeedClosePeerConnection()`

**示例代码**：
```cpp
void Conductor::OnNeedClosePeerConnection() {
  RTC_LOG(LS_INFO) << "Need close peer connection";
  
  // 先停止UI层的视频渲染器
  QMetaObject::invokeMethod(main_wnd_, [this]() {
    main_wnd_->StopLocalRenderer();
    main_wnd_->StopRemoteRenderer();
  }, Qt::QueuedConnection);
  
  // 再关闭WebRTC引擎的对等连接
  if (webrtc_engine_) {
    webrtc_engine_->ClosePeerConnection();
  }
}
```

### 4. 优化 ClosePeerConnection 的资源释放顺序 (webrtcengine.cc)

**修改位置**：`WebRTCEngine::ClosePeerConnection()`

**关键改进**：
1. 首先停止摄像头采集
2. 禁用本地媒体轨道
3. 从 PeerConnection 移除所有 senders
4. 关闭 PeerConnection
5. 释放媒体轨道引用
6. 释放 video_source 引用
7. 清理观察者和其他资源

**完整代码**：
```cpp
void WebRTCEngine::ClosePeerConnection() {
  RTC_LOG(LS_INFO) << "Closing peer connection...";
  
  // 第一步: 显式停止摄像头采集(最重要!)
  if (video_source_) {
    CapturerTrackSource* capturer_source = static_cast<CapturerTrackSource*>(video_source_.get());
    if (capturer_source) {
      capturer_source->Stop();
      RTC_LOG(LS_INFO) << "Video capturer stopped";
    }
  }
  
  // 第二步: 禁用本地媒体轨道
  if (local_video_track_) {
    local_video_track_->set_enabled(false);
    RTC_LOG(LS_INFO) << "Local video track disabled";
  }
  
  if (local_audio_track_) {
    local_audio_track_->set_enabled(false);
    RTC_LOG(LS_INFO) << "Local audio track disabled";
  }
  
  // 第三步: 移除 PeerConnection 中的所有 senders
  if (peer_connection_) {
    auto senders = peer_connection_->GetSenders();
    for (const auto& sender : senders) {
      peer_connection_->RemoveTrackOrError(sender);
    }
    RTC_LOG(LS_INFO) << "Removed " << senders.size() << " senders";
    
    // 关闭连接
    peer_connection_->Close();
    peer_connection_ = nullptr;
    RTC_LOG(LS_INFO) << "Peer connection closed";
  }
  
  // 第四步: 释放本地媒体轨道
  local_video_track_ = nullptr;
  local_audio_track_ = nullptr;
  
  // 第五步: 释放 video_source
  video_source_ = nullptr;
  RTC_LOG(LS_INFO) << "Video source released";
  
  // 第六步: 清理观察者和其他资源
  pc_observer_.reset();
  pending_ice_candidates_.clear();
  
  RTC_LOG(LS_INFO) << "Peer connection closed successfully";
}
```

## 资源释放的关键点

### 1. 正确的释放顺序
为了确保摄像头能够正确释放，必须按照以下顺序清理资源：

```
UI层停止渲染 → 停止摄像头采集 → 禁用轨道 → 移除Senders → 关闭连接 → 释放引用
```

### 2. VideoRenderer 的作用
`VideoRenderer::Stop()` 方法会调用 `RemoveSink()`，这会：
- 停止从 VideoTrack 接收视频帧
- 释放对 VideoTrack 的引用
- 允许 VideoTrack 和 VideoSource 被正确销毁

### 3. 引用计数机制
WebRTC 使用智能指针（`scoped_refptr`）管理对象生命周期：
- 必须确保所有持有引用的对象都释放了引用
- 只有引用计数归零时，对象才会被销毁
- VideoCapturer 在 CapturerTrackSource 的析构函数中被停止

## 验证方法

### 1. 测试挂断后摄像头释放
1. 启动程序并连接到信令服务器
2. 与另一个客户端建立通话
3. 点击"挂断"按钮
4. 使用摄像头测试工具（如 Windows 相机应用）确认摄像头已释放

### 2. 测试程序退出
1. 启动程序
2. 建立通话或不建立通话
3. 关闭窗口
4. 打开任务管理器，确认 `peerconnection_client.exe` 进程已完全退出
5. 确认摄像头已释放（指示灯熄灭或能够在其他应用中使用）

### 3. 测试各种退出场景
- 通话中退出程序
- 呼叫中（未接通）退出程序
- 空闲状态退出程序
- 连接信令服务器后退出程序

## 潜在问题和注意事项

### 1. Qt 信号槽的线程安全
使用 `Qt::QueuedConnection` 确保 UI 操作在主线程执行：
```cpp
QMetaObject::invokeMethod(main_wnd_, [this]() {
    main_wnd_->StopLocalRenderer();
}, Qt::QueuedConnection);
```

### 2. 析构函数调用顺序
- `MainWnd` 的析构函数中不应调用 `conductor_->Shutdown()`（已在 closeEvent 中调用）
- `Conductor` 的析构函数会自动调用 `Shutdown()`
- `main.cc` 中的清理代码确保正确的析构顺序

### 3. 日志监控
修改后的代码添加了详细的日志输出，可以通过日志确认资源释放的顺序：
```
Closing peer connection...
Video capturer stopped
Local video track disabled
Local audio track disabled
Removed 2 senders from peer connection
Peer connection closed
Video source released
Peer connection closed successfully
```

## 编译和测试

### 重新编译
```bash
cd build
cmake --build . --config Release
```

### 运行测试
```bash
.\Release\peerconnection_client.exe
```

## 总结

此次修复通过以下措施解决了摄像头占用和程序后台运行的问题：

1. **完善资源释放链路**：确保在所有退出场景下都正确停止视频渲染器
2. **优化释放顺序**：先停止采集，再释放引用，避免资源泄漏
3. **添加详细日志**：便于调试和确认资源释放过程
4. **保证线程安全**：使用 Qt 的信号槽机制确保 UI 操作的线程安全

修复后，程序在挂断电话或退出时应该能够正确释放摄像头和所有资源，不会留下后台进程。
