# TURN 服务器测试指南

## 快速测试步骤

### 1. 测试 TURN 服务器连通性

#### Linux/Mac
```bash
# 测试 UDP 端口
nc -uzv 113.46.159.182 3478

# 测试 TCP 端口
nc -zv 113.46.159.182 3478
```

#### Windows (PowerShell)
```powershell
# 测试 TCP 端口
Test-NetConnection -ComputerName 113.46.159.182 -Port 3478

# 或使用 telnet
telnet 113.46.159.182 3478
```

### 2. 在线 TURN 测试工具

#### 方法 A: Trickle ICE 测试
1. 打开浏览器访问: https://webrtc.github.io/samples/src/content/peerconnection/trickle-ice/

2. 点击 "Add Server" 按钮

3. 输入你的 TURN 服务器信息:
   ```
   TURN or STUN URI: turn:113.46.159.182:3478
   Username: myuser
   Password: mypassword
   ```

4. 点击 "Gather candidates" 按钮

5. 等待收集完成，查找输出中包含 `relay` 的行，例如:
   ```
   relay 113.46.159.182 52341 udp ...
   ```

#### 方法 B: IceTest 测试
访问: https://icetest.info/

输入你的 TURN 配置并测试

### 3. 使用 WebRTC 客户端测试

#### 步骤 1: 启动信令服务器
```bash
# 进入项目目录
cd d:\workspace\go-workspace\webrtc\client\webrtc-http

# 启动服务器
go run main.go
```

输出应该显示:
```
WebRTC信令服务器启动在端口 8080 (HTTP)
HTTP访问: http://localhost:8080
```

#### 步骤 2: 启动第一个客户端
```bash
cd d:\workspace\go-workspace\webrtc\client\build\Release
peerconnection_client.exe
```

1. 输入服务器地址: `ws://localhost:8080/ws`
2. 输入客户端ID: `client1`
3. 点击"连接"按钮

查看日志，应该显示:
```
已连接到服务器，客户端ID: client1
接收到 7 个 ICE 服务器配置:
  - stun:stun.l.google.com:19302
  - stun:stun1.l.google.com:19302
  - turn:113.46.159.182:3478 (认证)
  - turn:113.46.159.182:3478?transport=tcp (认证)
  - turn:openrelay.metered.ca:80 (认证)
  - turn:openrelay.metered.ca:443 (认证)
  - turn:openrelay.metered.ca:443?transport=tcp (认证)
```

#### 步骤 3: 启动第二个客户端
再打开一个客户端实例：
```bash
cd d:\workspace\go-workspace\webrtc\client\build\Release
peerconnection_client.exe
```

1. 输入服务器地址: `ws://localhost:8080/ws`
2. 输入客户端ID: `client2`
3. 点击"连接"按钮

#### 步骤 4: 建立通话
1. 在 client1 中选择 client2
2. 点击"呼叫"按钮
3. 在 client2 中接听

#### 步骤 5: 验证 TURN 使用情况

**方法 1: 查看客户端日志**
日志中应该显示 ICE 连接状态变化

**方法 2: 使用 Chrome 开发者工具**
1. 打开 Chrome 浏览器
2. 访问 `chrome://webrtc-internals/`
3. 如果使用 Chrome 版本的客户端，可以看到详细的 ICE 候选信息

**方法 3: 查看 TURN 服务器日志**
在服务器上执行:
```bash
sudo docker compose logs -f coturn
```

成功的连接应该显示类似:
```
INFO: session 001000000000000001: allocation created
INFO: session 001000000000000001: new permission 
INFO: session 001000000000000001: channel bound
```

### 4. 强制使用 TURN 测试

为了验证 TURN 真的在工作，可以强制只使用 TURN：

#### 临时修改 (仅用于测试)
编辑 `src/webrtcengine.cc`，在 `CreatePeerConnection()` 方法中添加：

```cpp
// 临时测试：强制使用 TURN
config.type = webrtc::PeerConnectionInterface::kRelay;
```

重新编译并测试，此时只会使用 TURN 服务器建立连接。

**⚠️ 记得测试后改回:**
```cpp
// 允许使用所有候选（正常配置）
config.type = webrtc::PeerConnectionInterface::kAll;
```

## 测试结果判断

### ✅ 成功指标

1. **服务器连通性**
   - TCP/UDP 3478 端口可访问
   - Trickle ICE 工具能收集到 relay 候选

2. **客户端配置**
   - 日志显示接收到 7 个 ICE 服务器配置
   - 包含你的 TURN 服务器地址

3. **通话建立**
   - 能够成功建立音视频通话
   - ICE 连接状态变为 "Connected"

4. **TURN 使用**
   - TURN 服务器日志显示会话创建
   - WebRTC internals 显示 relay 候选
   - 在严格 NAT 环境下仍能连接

### ❌ 失败情况及解决方案

#### 问题 1: 端口无法访问
**症状**: 
```
Connection timed out
Connection refused
```

**解决方案**:
1. 检查云服务商安全组是否开放端口
2. 检查服务器防火墙规则
3. 确认 TURN 服务正在运行

#### 问题 2: 认证失败
**症状**:
```
401 Unauthorized
```

**解决方案**:
1. 检查用户名密码是否正确
2. 查看 TURN 服务器日志确认凭据配置
3. 确认 `lt-cred-mech` 已启用

#### 问题 3: 无法收集到 relay 候选
**症状**:
- Trickle ICE 只显示 host 和 srflx
- 没有 relay 类型的候选

**解决方案**:
1. 检查 TURN 服务器状态
2. 验证防火墙配置（尤其是 UDP 49152-65535）
3. 确认外部 IP 配置正确
4. 测试 TURN 服务器的网络连通性

#### 问题 4: 客户端未接收到配置
**症状**:
日志中没有显示 TURN 配置

**解决方案**:
1. 确认信令服务器已更新配置
2. 重启信令服务器
3. 清除客户端缓存并重新连接

## 性能测试

### 带宽测试
1. 同时建立多个通话
2. 观察 TURN 服务器带宽使用
```bash
# 实时查看网络流量
sudo iftop -i eth0
```

### 延迟测试
使用 ping 测试到 TURN 服务器的延迟：
```bash
ping -c 10 113.46.159.182
```

理想延迟应该在 50ms 以内

### 并发测试
逐步增加并发用户数，观察：
- 服务器 CPU 使用率
- 内存使用情况
- 网络带宽消耗
- 连接成功率

## 监控命令

### 实时监控
```bash
# 查看活动连接数
watch -n 2 'sudo docker exec coturn netstat -an | grep 3478 | wc -l'

# 监控资源使用
sudo docker stats coturn

# 查看最新日志
sudo docker compose logs --tail=50 -f coturn
```

### 统计信息
```bash
# 统计今天的会话数
sudo docker exec coturn grep "allocation created" /var/tmp/turnserver.log | wc -l

# 查看最近的错误
sudo docker exec coturn grep "ERROR" /var/tmp/turnserver.log | tail -20
```

## 故障排查清单

- [ ] 服务器正在运行 (`docker ps`)
- [ ] 端口监听正常 (`netstat -tulpn | grep 3478`)
- [ ] 防火墙规则正确
- [ ] 安全组配置正确
- [ ] 用户名密码正确
- [ ] 外部 IP 配置正确
- [ ] 客户端能接收到配置
- [ ] 日志中没有错误信息

## 优化建议

### 降低延迟
1. 选择地理位置接近用户的服务器
2. 使用 SSD 存储
3. 优化网络配置

### 降低成本
1. 优先使用 P2P 连接
2. 使用较低的视频码率
3. 按需启动 TURN 服务
4. 定期清理日志文件

### 提高可靠性
1. 部署多台 TURN 服务器
2. 配置健康检查
3. 设置自动重启
4. 备份配置文件

## 下一步

测试通过后，建议：
1. ✅ 修改为强密码
2. ✅ 启用 TLS 加密
3. ✅ 配置监控告警
4. ✅ 编写运维文档
5. ✅ 制定应急预案

---

**祝测试顺利！** 🚀
