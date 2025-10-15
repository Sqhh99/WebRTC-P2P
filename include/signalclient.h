#ifndef EXAMPLES_PEERCONNECTION_CLIENT_SIGNALCLIENT_H_
#define EXAMPLES_PEERCONNECTION_CLIENT_SIGNALCLIENT_H_

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <memory>
#include <string>
#include <vector>

// ICE 服务器配置结构
struct IceServerConfig {
  std::vector<std::string> urls;
  std::string username;
  std::string credential;
};

// 信令消息类型
enum class SignalMessageType {
    Register,           // 注册客户端
    Registered,         // 注册确认
    ClientList,         // 客户端列表
    UserOffline,        // 用户下线
    CallRequest,        // 呼叫请求
    CallResponse,       // 呼叫响应
    CallCancel,         // 取消呼叫
    CallEnd,            // 结束通话
    Offer,              // SDP Offer
    Answer,             // SDP Answer
    IceCandidate,       // ICE候选
    Unknown
};

// 信令客户端观察者接口
class SignalClientObserver {
 public:
  virtual ~SignalClientObserver() = default;
  
  // 连接状态变化
  virtual void OnConnected(const std::string& client_id) = 0;
  virtual void OnDisconnected() = 0;
  virtual void OnConnectionError(const std::string& error) = 0;
  
  // ICE 服务器配置更新
  virtual void OnIceServersReceived(const std::vector<IceServerConfig>& ice_servers) = 0;
  
  // 客户端列表更新
  virtual void OnClientListUpdate(const QJsonArray& clients) = 0;
  virtual void OnUserOffline(const std::string& client_id) = 0;
  
  // 呼叫相关
  virtual void OnCallRequest(const std::string& from, const QJsonObject& payload) = 0;
  virtual void OnCallResponse(const std::string& from, bool accepted, const std::string& reason) = 0;
  virtual void OnCallCancel(const std::string& from, const std::string& reason) = 0;
  virtual void OnCallEnd(const std::string& from, const std::string& reason) = 0;
  
  // WebRTC信令
  virtual void OnOffer(const std::string& from, const QJsonObject& sdp) = 0;
  virtual void OnAnswer(const std::string& from, const QJsonObject& sdp) = 0;
  virtual void OnIceCandidate(const std::string& from, const QJsonObject& candidate) = 0;
};

// WebSocket信令客户端
class SignalClient : public QObject {
  Q_OBJECT

 public:
  explicit SignalClient(QObject* parent = nullptr);
  ~SignalClient() override;

  // 连接到信令服务器
  void Connect(const QString& server_url, const QString& client_id = QString());
  void Disconnect();
  bool IsConnected() const;
  
  // 获取客户端ID
  QString GetClientId() const { return client_id_; }
  
  // 获取 ICE 服务器配置
  const std::vector<IceServerConfig>& GetIceServers() const { return ice_servers_; }
  
  // 注册观察者
  void RegisterObserver(SignalClientObserver* observer);
  
  // 发送消息
  void SendCallRequest(const QString& to);
  void SendCallResponse(const QString& to, bool accepted, const QString& reason = QString());
  void SendCallCancel(const QString& to, const QString& reason = QString());
  void SendCallEnd(const QString& to, const QString& reason = QString());
  void SendOffer(const QString& to, const QJsonObject& sdp);
  void SendAnswer(const QString& to, const QJsonObject& sdp);
  void SendIceCandidate(const QString& to, const QJsonObject& candidate);
  void RequestClientList();

 signals:
  void SignalConnected(QString client_id);
  void SignalDisconnected();
  void SignalError(QString error);

 private slots:
  void OnWebSocketConnected();
  void OnWebSocketDisconnected();
  void OnWebSocketError(QAbstractSocket::SocketError error);
  void OnTextMessageReceived(const QString& message);
  void OnReconnectTimer();

 private:
  void SendMessage(const QJsonObject& message);
  void HandleMessage(const QJsonObject& message);
  SignalMessageType GetMessageType(const QString& type_str) const;
  void AttemptReconnect();
  void ClearReconnectTimer();

  std::unique_ptr<QWebSocket> websocket_;
  SignalClientObserver* observer_;
  QString server_url_;
  QString client_id_;
  bool is_connected_;
  bool manual_disconnect_;
  int reconnect_attempts_;
  std::unique_ptr<QTimer> reconnect_timer_;
  std::vector<IceServerConfig> ice_servers_;  // ICE 服务器配置
  
  static constexpr int kMaxReconnectAttempts = 5;
};

#endif  // EXAMPLES_PEERCONNECTION_CLIENT_SIGNALCLIENT_H_
