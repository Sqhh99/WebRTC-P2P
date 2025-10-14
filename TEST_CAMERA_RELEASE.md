# 摄像头资源释放测试指南

## 测试目的
验证挂断通话和退出程序时,摄像头资源是否被正确释放。

## 关键修复点

### 1. CapturerTrackSource 析构函数
```cpp
~CapturerTrackSource() override {
    Stop();  // 确保析构时停止采集
}
```

### 2. ClosePeerConnection 清理顺序
```
① capturer_source->Stop()           // 立即停止摄像头采集
② RemoveTrackOrError(senders)       // 移除senders(释放对track的引用)
③ peer_connection_->Close()         // 关闭P2P连接
④ local_video_track_ = nullptr      // 释放track(释放对source的引用)
⑤ video_source_ = nullptr           // 释放source(触发析构→再次Stop)
```

### 3. 为什么要这个顺序?

**引用链条**:
```
peer_connection_ → senders → local_video_track_ → video_source_ → CapturerTrackSource → capturer_ → VcmCapturer → 摄像头硬件
```

**必须从前往后释放**:
- 如果先释放 source,但 track 还持有引用,source 不会析构
- 如果先释放 track,但 senders 还持有引用,track 不会析构
- 所以必须: senders → track → source

**双重保险**:
- 显式调用 `Stop()`: 立即停止采集,即使对象还未析构
- 析构函数调用 `Stop()`: 确保对象销毁时必然停止采集

## 测试步骤

### 测试1: 挂断通话
1. 启动两个客户端(A和B)
2. A呼叫B,建立通话
3. **观察**: 两端摄像头指示灯都应该亮起
4. A点击"挂断"
5. **预期**: A的摄像头指示灯**立即熄灭**
6. **验证**: 打开其他应用(如相机应用),应该能立即使用摄像头

### 测试2: 被叫方挂断
1. 启动两个客户端(A和B)
2. A呼叫B,建立通话
3. B点击"挂断"
4. **预期**: B的摄像头指示灯**立即熄灭**
5. **验证**: B端能立即使用摄像头

### 测试3: 通话中退出程序
1. 启动两个客户端(A和B)
2. A呼叫B,建立通话
3. 直接关闭A的窗口(点击X)
4. **预期**: 
   - A的窗口关闭
   - 摄像头指示灯熄灭
   - 进程完全退出(任务管理器中无残留)
5. **验证**: 
   - 打开任务管理器,确认 peerconnection_client.exe 已不存在
   - 打开相机应用,确认摄像头可用

### 测试4: 未通话时退出
1. 启动客户端
2. **不进行任何通话**,直接关闭窗口
3. **预期**: 程序正常退出,无错误
4. **验证**: 任务管理器中进程已退出

### 测试5: 连续呼叫测试
1. A呼叫B → 建立通话 → 挂断
2. 立即重复上述流程3次
3. **预期**: 每次挂断后摄像头都能立即释放
4. **验证**: 第4次呼叫应该能正常使用摄像头

## 如何判断成功?

### ✅ 成功标志
- 挂断后摄像头指示灯在**1秒内**熄灭
- 退出程序后任务管理器中**无残留进程**
- 退出程序后其他应用能**立即**使用摄像头
- 连续多次通话无问题

### ❌ 失败标志
- 挂断后摄像头指示灯持续亮着
- 退出程序后进程仍在运行
- 退出程序后相机应用提示"摄像头被占用"
- 第二次呼叫时提示"无法打开摄像头"

## 调试技巧

如果问题仍存在,检查:
1. 是否有多个 peerconnection_client.exe 进程在运行
2. 使用 Process Explorer 查看进程是否持有摄像头设备句柄
3. 在代码中添加日志:
   ```cpp
   RTC_LOG(LS_INFO) << "CapturerTrackSource::Stop() called";
   RTC_LOG(LS_INFO) << "CapturerTrackSource destructor called";
   ```

## 技术说明

### 为什么旧版本不需要显式 Stop()?
旧版本的 `video_track_` 是**局部变量**,在 `AddTracks()` 函数返回后:
- 局部变量 `video_track_` 销毁
- 但 PeerConnection 持有的引用继续存在
- 当调用 `DeletePeerConnection()` 时,`peer_connection_ = nullptr` 触发:
  - PeerConnection 析构
  - 释放所有 senders
  - senders 释放 tracks
  - tracks 释放 source
  - source (CapturerTrackSource) 析构... 但析构函数没调用 Stop()!

**旧版本能工作是因为 VcmCapturer 的析构函数调用了 Destroy() → StopCapture()**

### 新版本的改进
- 保存 `video_source_` 成员变量,以便显式控制
- 在 ClosePeerConnection 中**显式调用 Stop()**
- 在析构函数中**再次调用 Stop()** (双重保险)
- **严格控制释放顺序** (senders → track → source)

这样确保了在任何情况下(正常挂断、异常退出、析构),摄像头都能被正确释放。
