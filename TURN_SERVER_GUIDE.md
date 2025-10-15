# TURN 服务器配置指南

## 概述

TURN (Traversal Using Relays around NAT) 服务器用于在 P2P 连接失败时中继媒体流量。当 STUN 无法穿透某些严格的 NAT 或防火墙时，TURN 服务器作为最后的回退选项。

## 当前配置

项目已配置使用公共测试 TURN 服务器 (openrelay.metered.ca)：
- 用户名：openrelayproject
- 密码：openrelayproject
- 端口：80, 443 (TCP/UDP)

**注意：** 公共 TURN 服务器仅用于测试，生产环境必须使用自己的 TURN 服务器。

## 为什么需要 TURN？

### NAT 穿透成功率

| 场景 | 成功率 | 说明 |
|------|--------|------|
| 直接连接 | ~15% | 双方都有公网 IP |
| STUN 穿透 | ~70% | 至少一方可以接收入站连接 |
| TURN 中继 | ~95% | 通过服务器中继，几乎总是成功 |
| 完整方案 (STUN+TURN) | ~99% | 推荐的生产配置 |

### 需要 TURN 的场景

1. **对称型 NAT**：两端都是对称型 NAT，无法直接穿透
2. **企业防火墙**：严格的出站规则限制
3. **移动网络**：某些移动运营商的 NAT 配置
4. **IPv4/IPv6 混合**：网络协议不匹配

## 部署自己的 TURN 服务器

### 选项 1: Coturn (推荐)

Coturn 是最流行的开源 TURN/STUN 服务器实现。

#### 在 Ubuntu/Debian 上安装

```bash
# 安装 Coturn
sudo apt-get update
sudo apt-get install coturn

# 启用 Coturn 服务
sudo systemctl enable coturn
```

#### 配置 Coturn

编辑 `/etc/turnserver.conf`:

```conf
# 监听端口
listening-port=3478
tls-listening-port=5349

# 外部 IP 地址（你的服务器公网 IP）
external-ip=YOUR_SERVER_PUBLIC_IP

# 中继 IP 范围
relay-ip=YOUR_SERVER_PUBLIC_IP

# 认证
# 使用长期凭据机制
lt-cred-mech

# 用户认证（格式：username:password）
user=myuser:mypassword

# 或使用共享密钥（更安全）
use-auth-secret
static-auth-secret=YOUR_LONG_RANDOM_SECRET_KEY

# Realm（域）
realm=yourdomain.com

# 日志
log-file=/var/log/turnserver.log
verbose

# 性能优化
max-bps=1000000
bps-capacity=0

# 安全设置
no-multicast-peers
no-cli
no-tlsv1
no-tlsv1_1

# 限制连接来源（可选）
# denied-peer-ip=0.0.0.0-0.255.255.255
# denied-peer-ip=127.0.0.0-127.255.255.255
```

#### 生成认证凭据

```bash
# 生成共享密钥
openssl rand -hex 32

# 生成用户密码
turnadmin -a -u myuser -p mypassword -r yourdomain.com
```

#### 启动服务

```bash
sudo systemctl start coturn
sudo systemctl status coturn

# 查看日志
sudo tail -f /var/log/turnserver.log
```

#### 防火墙配置

```bash
# UFW
sudo ufw allow 3478/tcp
sudo ufw allow 3478/udp
sudo ufw allow 5349/tcp
sudo ufw allow 49152:65535/udp  # 中继端口范围

# iptables
sudo iptables -A INPUT -p tcp --dport 3478 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 3478 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 5349 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 49152:65535 -j ACCEPT
```

### 选项 2: Docker 部署

```bash
# 拉取 Coturn Docker 镜像
docker pull coturn/coturn

# 创建配置文件
cat > turnserver.conf << EOF
listening-port=3478
external-ip=YOUR_SERVER_PUBLIC_IP
relay-ip=YOUR_SERVER_PUBLIC_IP
lt-cred-mech
user=myuser:mypassword
realm=yourdomain.com
log-file=/var/log/turnserver.log
verbose
EOF

# 运行容器
docker run -d --name coturn \
  --network=host \
  -v $(pwd)/turnserver.conf:/etc/coturn/turnserver.conf \
  coturn/coturn
```

### 选项 3: 云服务托管 TURN

推荐的商业 TURN 服务提供商：

1. **Twilio STUN/TURN**
   - 网址：https://www.twilio.com/stun-turn
   - 价格：按使用量计费
   - 优点：全球分布，高可用

2. **Xirsys**
   - 网址：https://xirsys.com/
   - 价格：免费套餐 + 付费计划
   - 优点：专为 WebRTC 优化

3. **Metered**
   - 网址：https://www.metered.ca/
   - 价格：按使用量计费
   - 优点：简单配置

## 在项目中配置 TURN 服务器

### 修改信令服务器 (main.go)

在 `getIceServers()` 函数中替换为你的 TURN 服务器：

```go
func (s *SignalingServer) getIceServers() []IceServer {
	return []IceServer{
		// STUN 服务器
		{
			URLs: []string{"stun:stun.l.google.com:19302"},
		},
		// 你的 TURN 服务器（UDP）
		{
			URLs:       []string{"turn:your-turn-server.com:3478"},
			Username:   "myuser",
			Credential: "mypassword",
		},
		// 你的 TURN 服务器（TCP）
		{
			URLs:       []string{"turn:your-turn-server.com:3478?transport=tcp"},
			Username:   "myuser",
			Credential: "mypassword",
		},
		// TURN over TLS（推荐生产环境）
		{
			URLs:       []string{"turns:your-turn-server.com:5349"},
			Username:   "myuser",
			Credential: "mypassword",
		},
	}
}
```

### 使用时间限制的凭据（推荐）

为了安全，建议使用时间限制的 TURN 凭据：

```go
import (
	"crypto/hmac"
	"crypto/sha1"
	"encoding/base64"
	"fmt"
	"time"
)

func generateTurnCredentials(username, secret string, ttl time.Duration) (string, string) {
	timestamp := time.Now().Add(ttl).Unix()
	turnUsername := fmt.Sprintf("%d:%s", timestamp, username)
	
	mac := hmac.New(sha1.New, []byte(secret))
	mac.Write([]byte(turnUsername))
	turnPassword := base64.StdEncoding.EncodeToString(mac.Sum(nil))
	
	return turnUsername, turnPassword
}

// 在注册时生成凭据
func (s *SignalingServer) getIceServers() []IceServer {
	turnUsername, turnPassword := generateTurnCredentials(
		"webrtc-user",
		"YOUR_SHARED_SECRET",
		24*time.Hour, // 24小时有效期
	)
	
	return []IceServer{
		{URLs: []string{"stun:stun.l.google.com:19302"}},
		{
			URLs:       []string{"turn:your-turn-server.com:3478"},
			Username:   turnUsername,
			Credential: turnPassword,
		},
	}
}
```

## 测试 TURN 服务器

### 命令行测试

```bash
# 使用 turnutils_uclient 测试
turnutils_uclient -v -u myuser -w mypassword your-turn-server.com
```

### 在线测试工具

访问 https://webrtc.github.io/samples/src/content/peerconnection/trickle-ice/

输入你的 TURN 服务器配置：
```
TURN URI: turn:your-turn-server.com:3478
Username: myuser
Password: mypassword
```

点击 "Gather candidates"，应该看到 `relay` 类型的候选者。

### 浏览器控制台测试

```javascript
const pc = new RTCPeerConnection({
  iceServers: [
    { urls: 'stun:stun.l.google.com:19302' },
    { 
      urls: 'turn:your-turn-server.com:3478',
      username: 'myuser',
      credential: 'mypassword'
    }
  ]
});

pc.onicecandidate = (event) => {
  if (event.candidate) {
    console.log('ICE Candidate:', event.candidate.type, event.candidate.candidate);
  }
};

pc.createOffer().then(offer => pc.setLocalDescription(offer));
```

查找输出中包含 `typ relay` 的候选者，这表示 TURN 正常工作。

## 监控和维护

### 性能监控

```bash
# 查看连接数
sudo netstat -anp | grep turnserver | wc -l

# 查看带宽使用
sudo iftop -i eth0

# Coturn 统计
sudo systemctl status coturn
```

### 日志分析

```bash
# 实时查看日志
sudo tail -f /var/log/turnserver.log

# 统计 TURN 使用情况
sudo grep "session" /var/log/turnserver.log | tail -100
```

### 成本估算

TURN 服务器成本主要来自带宽：

| 场景 | 带宽需求 | 月成本估算 |
|------|---------|-----------|
| 100 并发用户（音频） | ~5 Mbps | $20-50 |
| 100 并发用户（视频 720p） | ~50 Mbps | $200-500 |
| 1000 并发用户（视频 720p） | ~500 Mbps | $2000-5000 |

**优化建议：**
- 仅在 STUN 失败时使用 TURN
- 使用更低的视频码率
- 部署多个区域服务器
- 使用 CDN 分发

## 安全最佳实践

1. **使用 TLS/DTLS**
   - 配置 TURNS（TURN over TLS）
   - 使用有效的 SSL 证书

2. **限制访问**
   - 使用时间限制的凭据
   - IP 白名单
   - 速率限制

3. **监控异常**
   - 设置带宽告警
   - 监控异常连接
   - 定期审计日志

4. **防止滥用**
   - 限制单个用户的连接数
   - 限制中继会话时长
   - 实施配额管理

```conf
# Coturn 配置示例
max-allocate-lifetime=3600
max-allocate-timeout=60
user-quota=10
total-quota=100
```

## 故障排查

### TURN 连接失败

1. **检查防火墙**
   ```bash
   sudo ufw status
   sudo iptables -L -n
   ```

2. **检查服务状态**
   ```bash
   sudo systemctl status coturn
   sudo journalctl -u coturn -f
   ```

3. **验证网络连接**
   ```bash
   nc -zv your-turn-server.com 3478
   telnet your-turn-server.com 3478
   ```

4. **检查凭据**
   - 确认用户名/密码正确
   - 检查时间限制凭据是否过期

### 客户端日志

在客户端，检查 WebRTC 日志中的 ICE 候选者：

```
chrome://webrtc-internals/  # Chrome
about:webrtc                 # Firefox
```

查找 `relay` 类型的候选者，确认 TURN 正常工作。

## 总结

✅ **完成的改进：**
- 支持从信令服务器动态配置 ICE 服务器
- 集成 STUN 和 TURN 服务器
- 提供公共测试服务器
- 支持认证的 TURN 连接

✅ **下一步：**
1. 部署自己的 TURN 服务器（生产环境必须）
2. 配置时间限制的凭据
3. 监控 TURN 使用情况和成本
4. 优化连接策略（优先 P2P，回退到 TURN）

有了 TURN 支持，你的 WebRTC 应用现在可以在几乎所有网络环境下建立连接！
