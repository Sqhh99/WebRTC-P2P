# 自部署 TURN 服务器配置说明

## 服务器信息

### 基本信息
- **公网IP**: 113.46.159.182
- **内网IP**: 172.31.3.203
- **监听端口**: 3478 (UDP/TCP)
- **中继端口范围**: 49152-65535

### 认证信息
- **用户名**: myuser
- **密码**: mypassword
- **Realm**: turn.yourdomain.com

### 部署方式
- 使用 Docker Compose 部署 Coturn 4.7.0
- 配置文件位置: `~/software-docker/coturn-docker/turnserver.conf`

## 当前配置

### Coturn 配置文件 (turnserver.conf)
```conf
listening-port=3478

# 设置监听地址（内网IP）
listening-ip=172.31.3.203

# 外部IP地址（公网IP映射到内网IP）
external-ip=113.46.159.182/172.31.3.203

# 启用长期凭证机制
lt-cred-mech

# 用户凭证
user=myuser:mypassword

# realm设置
realm=turn.yourdomain.com

# 日志文件路径
log-file=/var/tmp/turnserver.log

# 详细日志
verbose

# 禁用TLS/DTLS（生产环境建议启用）
no-tls
no-dtls

# 指定端口范围
min-port=49152
max-port=65535
```

### 信令服务器配置 (main.go)

已配置的 ICE 服务器列表（按优先级）：
1. **STUN**: stun.l.google.com:19302
2. **STUN**: stun1.l.google.com:19302  
3. **TURN (你的服务器 UDP)**: turn:113.46.159.182:3478
4. **TURN (你的服务器 TCP)**: turn:113.46.159.182:3478?transport=tcp
5. **备用 TURN**: openrelay.metered.ca (公共测试服务器)

## 服务器状态

### 启动状态 ✅
```
INFO: Coturn Version Coturn-4.7.0 'Gorst'
INFO: Max number of open files/sockets allowed: 1048576
INFO: Max supported TURN Sessions: ~524000
INFO: IPv4. TCP listener opened on: 172.31.3.203:3478
INFO: IPv4. UDP listener opened on: 172.31.3.203:3478
```

### 支持的功能
- ✅ TLS 1.2/1.3
- ✅ DTLS 1.2
- ✅ TURN/STUN ALPN
- ✅ Redis 支持
- ✅ PostgreSQL/MySQL/MongoDB 支持

## 防火墙配置

### 需要开放的端口

#### 必需端口
```bash
# TURN 监听端口
TCP/UDP 3478

# 中继端口范围
UDP 49152-65535
```

#### 可选端口（如果启用 TLS）
```bash
# TURN over TLS
TCP 5349
```

### 防火墙规则示例

#### iptables
```bash
# 允许 TURN 端口
sudo iptables -A INPUT -p tcp --dport 3478 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 3478 -j ACCEPT

# 允许中继端口范围
sudo iptables -A INPUT -p udp --dport 49152:65535 -j ACCEPT

# 保存规则
sudo iptables-save > /etc/iptables/rules.v4
```

#### ufw
```bash
sudo ufw allow 3478/tcp
sudo ufw allow 3478/udp
sudo ufw allow 49152:65535/udp
```

#### 云服务商安全组
如果使用云服务器，需要在云服务商控制台添加安全组规则：
- 入站规则：TCP 3478
- 入站规则：UDP 3478
- 入站规则：UDP 49152-65535

## 测试方法

### 1. 命令行测试
```bash
# 使用 turnutils_uclient 测试
turnutils_uclient -v -u myuser -w mypassword 113.46.159.182
```

### 2. 在线测试工具
访问：https://webrtc.github.io/samples/src/content/peerconnection/trickle-ice/

配置：
```
TURN URI: turn:113.46.159.182:3478
Username: myuser
Password: mypassword
```

点击 "Gather candidates"，应该看到 `relay` 类型的候选者。

### 3. 客户端测试
1. 启动信令服务器：
   ```bash
   cd webrtc-http
   go run main.go
   ```

2. 启动客户端并连接

3. 查看日志，应该看到：
   ```
   接收到 7 个 ICE 服务器配置:
     - stun:stun.l.google.com:19302
     - stun:stun1.l.google.com:19302
     - turn:113.46.159.182:3478 (认证)
     - turn:113.46.159.182:3478?transport=tcp (认证)
     - turn:openrelay.metered.ca:80 (认证)
     - turn:openrelay.metered.ca:443 (认证)
     - turn:openrelay.metered.ca:443?transport=tcp (认证)
   ```

4. 建立通话，在 `chrome://webrtc-internals/` 中查看是否有 `relay` 候选者

## 监控和维护

### 查看日志
```bash
# 实时查看 Docker 容器日志
sudo docker compose logs -f

# 查看 Coturn 日志文件
sudo tail -f /var/tmp/turnserver.log
```

### 查看连接状态
```bash
# 查看当前连接数
sudo docker exec coturn netstat -anp | grep 3478 | wc -l

# 查看端口占用
sudo docker exec coturn ss -tulpn | grep 3478
```

### 重启服务
```bash
cd ~/software-docker/coturn-docker
sudo docker compose restart
```

### 查看资源使用
```bash
# 容器资源使用情况
sudo docker stats coturn

# 带宽监控
sudo iftop -i eth0
```

## 安全建议

### ⚠️ 当前配置的安全问题

1. **弱密码**: `myuser:mypassword` 太简单
2. **未启用 TLS**: 通信未加密
3. **固定凭据**: 没有使用时间限制的凭据

### 🔒 安全加固建议

#### 1. 使用强密码
```bash
# 生成强密码
openssl rand -base64 32

# 更新配置
user=myuser:STRONG_RANDOM_PASSWORD_HERE
```

#### 2. 启用 TLS（推荐）
```conf
# 删除这两行
# no-tls
# no-dtls

# 添加证书配置
cert=/path/to/cert.pem
pkey=/path/to/key.pem
```

#### 3. 使用时间限制的凭据
在信令服务器中实现动态凭据生成：

```go
import (
    "crypto/hmac"
    "crypto/sha1"
    "encoding/base64"
    "fmt"
    "time"
)

func generateTurnCredentials(secret string, ttl time.Duration) (string, string) {
    timestamp := time.Now().Add(ttl).Unix()
    username := fmt.Sprintf("%d:webrtc", timestamp)
    
    mac := hmac.New(sha1.New, []byte(secret))
    mac.Write([]byte(username))
    password := base64.StdEncoding.EncodeToString(mac.Sum(nil))
    
    return username, password
}

// 在 getIceServers 中使用
func (s *SignalingServer) getIceServers() []IceServer {
    username, password := generateTurnCredentials(
        "YOUR_SHARED_SECRET", 
        24*time.Hour,
    )
    
    return []IceServer{
        // ... STUN 配置 ...
        {
            URLs:       []string{"turn:113.46.159.182:3478"},
            Username:   username,
            Credential: password,
        },
    }
}
```

然后在 turnserver.conf 中：
```conf
# 注释掉固定用户
# user=myuser:mypassword

# 使用共享密钥
use-auth-secret
static-auth-secret=YOUR_SHARED_SECRET
```

#### 4. 限制访问
```conf
# 限制每个用户的配额
user-quota=10
total-quota=100

# 限制会话时长
max-allocate-lifetime=3600
max-allocate-timeout=60

# 拒绝特定 IP 范围
denied-peer-ip=0.0.0.0-0.255.255.255
denied-peer-ip=127.0.0.0-127.255.255.255
```

## 成本估算

### 带宽消耗
基于你的 TURN 服务器使用情况：

| 场景 | 并发用户 | 视频质量 | 估算带宽 |
|------|---------|---------|---------|
| 小规模测试 | 10 | 480p | ~5 Mbps |
| 中等使用 | 50 | 720p | ~50 Mbps |
| 大规模应用 | 200 | 720p | ~200 Mbps |

**注意**: 
- 只有在 P2P 失败时才使用 TURN
- 实际使用率通常在 10-30%
- 音频通话带宽需求低得多

### 优化建议
1. 使用更低的视频码率
2. 优先使用 P2P 连接
3. 根据网络质量动态调整质量
4. 考虑使用 CDN 分发（如果有静态内容）

## 故障排查

### 问题：连接到 TURN 服务器失败

#### 检查服务器状态
```bash
sudo docker compose ps
sudo docker compose logs coturn | tail -50
```

#### 检查端口监听
```bash
sudo netstat -tulpn | grep 3478
```

#### 检查防火墙
```bash
# 测试端口可达性
nc -zv 113.46.159.182 3478
telnet 113.46.159.182 3478
```

#### 检查云服务商安全组
确保在云服务商控制台已开放所需端口

### 问题：客户端无法获取 relay 候选

#### 1. 检查客户端日志
查看是否接收到 TURN 配置

#### 2. 检查浏览器控制台
```javascript
// 打开 chrome://webrtc-internals/
// 查找 "relay" 类型的候选者
```

#### 3. 验证凭据
确保用户名和密码正确

### 问题：高延迟或丢包

#### 优化配置
```conf
# 增加缓冲区大小
relay-threads=4

# 优化网络性能
no-tcp-relay
no-multicast-peers
```

## 下一步优化

### 短期（1-2周）
- [ ] 更改为强密码
- [ ] 启用 TLS/DTLS
- [ ] 配置日志轮转
- [ ] 设置监控告警

### 中期（1个月）
- [ ] 实现时间限制的凭据
- [ ] 部署 Redis 持久化
- [ ] 配置负载均衡
- [ ] 性能测试和调优

### 长期（3个月）
- [ ] 多区域部署
- [ ] 自动扩容
- [ ] 成本分析和优化
- [ ] 灾难恢复计划

## 备注

### 重要提醒
⚠️ **生产环境必做清单**:
1. ✅ 已部署自己的 TURN 服务器
2. ⚠️ 更改默认密码（当前：mypassword）
3. ⚠️ 启用 TLS 加密
4. ⚠️ 配置访问控制
5. ⚠️ 设置监控和告警
6. ⚠️ 配置自动备份
7. ⚠️ 制定维护计划

### 联系信息
- 服务器位置: sqhh99@hcss-ecs-dcb1
- 配置目录: ~/software-docker/coturn-docker
- 容器名称: coturn

---

**最后更新**: 2025年10月15日
**状态**: ✅ 服务器已部署并运行
**版本**: Coturn 4.7.0 'Gorst'
