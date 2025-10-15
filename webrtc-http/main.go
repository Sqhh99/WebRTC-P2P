package main

import (
	"crypto/rand"
	"crypto/rsa"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"log"
	"math/big"
	"net"
	"net/http"
	"os"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true // 允许所有来源，生产环境中应该更严格
	},
}

type SignalingServer struct {
	clients map[*websocket.Conn]string
	mutex   sync.RWMutex
}

type Message struct {
	Type    string      `json:"type"`
	From    string      `json:"from,omitempty"`
	To      string      `json:"to,omitempty"`
	Payload interface{} `json:"payload,omitempty"`
}

// ICE 服务器配置
type IceServer struct {
	URLs       []string `json:"urls"`
	Username   string   `json:"username,omitempty"`
	Credential string   `json:"credential,omitempty"`
}

func NewSignalingServer() *SignalingServer {
	return &SignalingServer{
		clients: make(map[*websocket.Conn]string),
	}
}

func (s *SignalingServer) handleWebSocket(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("升级到WebSocket连接失败: %v", err)
		return
	}
	defer conn.Close()

	log.Println("新的WebSocket连接建立")

	for {
		var msg Message
		err := conn.ReadJSON(&msg)
		if err != nil {
			log.Printf("读取消息失败: %v", err)
			s.removeClient(conn)
			break
		}

		log.Printf("收到消息: %+v", msg)

		switch msg.Type {
		case "register":
			s.registerClient(conn, msg.From)
		case "offer", "answer", "ice-candidate", "conflict-resolution", "call-request", "call-response", "call-cancel", "call-end":
			s.relayMessage(conn, msg)
		case "list-clients":
			s.sendClientList(conn)
		}
	}
}

func (s *SignalingServer) registerClient(conn *websocket.Conn, clientID string) {
	s.mutex.Lock()
	s.clients[conn] = clientID
	s.mutex.Unlock()

	log.Printf("客户端注册: %s", clientID)

	// 发送注册确认，包含 ICE 服务器配置
	response := Message{
		Type: "registered",
		From: clientID,
		Payload: map[string]interface{}{
			"iceServers": s.getIceServers(),
		},
	}
	conn.WriteJSON(response)

	// 广播客户端列表更新
	s.broadcastClientList()
}

// 获取 ICE 服务器配置（包括 STUN 和 TURN）
func (s *SignalingServer) getIceServers() []IceServer {
	return []IceServer{
		// Google 公共 STUN 服务器
		{
			URLs: []string{"stun:stun.l.google.com:19302"},
		},
		// Google 备用 STUN 服务器
		{
			URLs: []string{"stun:stun1.l.google.com:19302"},
		},
		// 你自己的 TURN 服务器 (UDP)
		{
			URLs:       []string{"turn:113.46.159.182:3478"},
			Username:   "myuser",
			Credential: "mypassword",
		},
		// 你自己的 TURN 服务器 (TCP)
		{
			URLs:       []string{"turn:113.46.159.182:3478?transport=tcp"},
			Username:   "myuser",
			Credential: "mypassword",
		},
		// 公共 TURN 服务器作为备份 (metered.ca - 免费测试用)
		{
			URLs:       []string{"turn:openrelay.metered.ca:80"},
			Username:   "openrelayproject",
			Credential: "openrelayproject",
		},
		{
			URLs:       []string{"turn:openrelay.metered.ca:443"},
			Username:   "openrelayproject",
			Credential: "openrelayproject",
		},
		{
			URLs:       []string{"turn:openrelay.metered.ca:443?transport=tcp"},
			Username:   "openrelayproject",
			Credential: "openrelayproject",
		},
	}
}

func (s *SignalingServer) removeClient(conn *websocket.Conn) {
	s.mutex.Lock()
	clientID := s.clients[conn]
	delete(s.clients, conn)
	s.mutex.Unlock()

	if clientID != "" {
		log.Printf("客户端断开连接: %s", clientID)

		// 通知其他客户端该用户已下线
		s.notifyUserOffline(clientID)

		s.broadcastClientList()
	}
}

func (s *SignalingServer) notifyUserOffline(clientID string) {
	s.mutex.RLock()
	defer s.mutex.RUnlock()

	// 向所有其他客户端发送用户下线通知
	for conn, id := range s.clients {
		if id != clientID {
			offlineMsg := Message{
				Type: "user-offline",
				From: "server",
				To:   id,
				Payload: map[string]interface{}{
					"clientId": clientID,
				},
			}

			err := conn.WriteJSON(offlineMsg)
			if err != nil {
				log.Printf("发送下线通知到客户端 %s 失败: %v", id, err)
			}
		}
	}
}

func (s *SignalingServer) relayMessage(senderConn *websocket.Conn, msg Message) {
	s.mutex.RLock()
	defer s.mutex.RUnlock()

	// 找到目标客户端
	for conn, clientID := range s.clients {
		if clientID == msg.To {
			err := conn.WriteJSON(msg)
			if err != nil {
				log.Printf("发送消息到客户端 %s 失败: %v", clientID, err)
			}
			return
		}
	}

	log.Printf("目标客户端 %s 未找到", msg.To)
}

func (s *SignalingServer) sendClientList(conn *websocket.Conn) {
	s.mutex.RLock()
	defer s.mutex.RUnlock()

	var clients []map[string]string
	for _, clientID := range s.clients {
		clients = append(clients, map[string]string{"id": clientID})
	}

	response := Message{
		Type: "client-list",
		Payload: map[string]interface{}{
			"clients": clients,
		},
	}
	conn.WriteJSON(response)
}

func (s *SignalingServer) broadcastClientList() {
	s.mutex.RLock()
	defer s.mutex.RUnlock()

	var clients []map[string]string
	for _, clientID := range s.clients {
		clients = append(clients, map[string]string{"id": clientID})
	}

	message := Message{
		Type: "client-list",
		Payload: map[string]interface{}{
			"clients": clients,
		},
	}

	for conn := range s.clients {
		err := conn.WriteJSON(message)
		if err != nil {
			log.Printf("广播客户端列表失败: %v", err)
		}
	}
}

// 生成自签名证书
func generateSelfSignedCert() error {
	// 检查证书文件是否已存在
	if _, err := os.Stat("server.crt"); err == nil {
		if _, err := os.Stat("server.key"); err == nil {
			log.Println("证书文件已存在，跳过生成")
			return nil
		}
	}

	log.Println("生成自签名证书...")

	// 生成私钥
	privateKey, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		return fmt.Errorf("生成私钥失败: %v", err)
	}

	// 创建证书模板
	template := x509.Certificate{
		SerialNumber: big.NewInt(1),
		Subject: pkix.Name{
			Organization:  []string{"WebRTC Demo"},
			Country:       []string{"CN"},
			Province:      []string{""},
			Locality:      []string{""},
			StreetAddress: []string{""},
			PostalCode:    []string{""},
		},
		NotBefore:   time.Now(),
		NotAfter:    time.Now().Add(365 * 24 * time.Hour),
		KeyUsage:    x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		ExtKeyUsage: []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
		IPAddresses: []net.IP{
			net.IPv4(127, 0, 0, 1),
			net.IPv6loopback,
		},
		DNSNames: []string{
			"localhost",
			"*.localhost",
		},
	}

	// 生成证书
	certDER, err := x509.CreateCertificate(rand.Reader, &template, &template, &privateKey.PublicKey, privateKey)
	if err != nil {
		return fmt.Errorf("生成证书失败: %v", err)
	}

	// 保存证书
	certOut, err := os.Create("server.crt")
	if err != nil {
		return fmt.Errorf("创建证书文件失败: %v", err)
	}
	defer certOut.Close()

	if err := pem.Encode(certOut, &pem.Block{Type: "CERTIFICATE", Bytes: certDER}); err != nil {
		return fmt.Errorf("编码证书失败: %v", err)
	}

	// 保存私钥
	keyOut, err := os.Create("server.key")
	if err != nil {
		return fmt.Errorf("创建私钥文件失败: %v", err)
	}
	defer keyOut.Close()

	privateKeyDER, err := x509.MarshalPKCS8PrivateKey(privateKey)
	if err != nil {
		return fmt.Errorf("序列化私钥失败: %v", err)
	}

	if err := pem.Encode(keyOut, &pem.Block{Type: "PRIVATE KEY", Bytes: privateKeyDER}); err != nil {
		return fmt.Errorf("编码私钥失败: %v", err)
	}

	log.Println("自签名证书生成成功")
	return nil
}

func main() {
	server := NewSignalingServer()

	// 设置静态文件服务
	http.Handle("/", http.FileServer(http.Dir("./static/")))

	// WebSocket信令服务器端点
	http.HandleFunc("/ws", server.handleWebSocket)

	fmt.Println("WebRTC信令服务器启动在端口 8080 (HTTP)")
	fmt.Println("HTTP访问: http://localhost:8080")
	fmt.Println("注意: 本地测试使用HTTP，生产环境建议使用HTTPS")

	// 如果是服务器环境，显示外部访问地址
	fmt.Println("\n如果在服务器上部署，请使用以下地址访问：")
	fmt.Println("HTTP: http://your-server-ip:8080")

	// 启动HTTP服务器
	log.Println("启动HTTP服务器在端口8080...")
	log.Fatal(http.ListenAndServe(":8080", nil))
}
