#ifndef EXAMPLES_PEERCONNECTION_CLIENT_CALLMANAGER_H_
#define EXAMPLES_PEERCONNECTION_CLIENT_CALLMANAGER_H_

#include <QObject>
#include <QTimer>
#include <memory>
#include <string>

#include "signalclient.h"

// 呼叫状态
enum class CallState {
  Idle,           // 空闲
  Calling,        // 呼叫中（主叫方）
  Receiving,      // 接听中（被叫方）
  Connecting,     // 连接建立中
  Connected,      // 已连接
  Ending          // 结束中
};

// 呼叫管理器观察者接口
class CallManagerObserver {
 public:
  virtual ~CallManagerObserver() = default;
  
  // 状态变化通知
  virtual void OnCallStateChanged(CallState state, const std::string& peer_id) = 0;
  
  // 收到呼叫请求
  virtual void OnIncomingCall(const std::string& caller_id) = 0;
  
  // 呼叫被接受/拒绝
  virtual void OnCallAccepted(const std::string& peer_id) = 0;
  virtual void OnCallRejected(const std::string& peer_id, const std::string& reason) = 0;
  
  // 呼叫被取消/结束
  virtual void OnCallCancelled(const std::string& peer_id, const std::string& reason) = 0;
  virtual void OnCallEnded(const std::string& peer_id, const std::string& reason) = 0;
  
  // 呼叫超时
  virtual void OnCallTimeout() = 0;
  
  // 需要创建/关闭对等连接
  virtual void OnNeedCreatePeerConnection(const std::string& peer_id, bool is_caller) = 0;
  virtual void OnNeedClosePeerConnection() = 0;
};

// 呼叫管理器：管理完整的呼叫流程
class CallManager : public QObject {
  Q_OBJECT

 public:
  explicit CallManager(QObject* parent = nullptr);
  ~CallManager() override;

  // 设置信令客户端（必须在使用前设置）
  void SetSignalClient(SignalClient* signal_client);
  
  // 注册观察者
  void RegisterObserver(CallManagerObserver* observer);
  
  // 发起呼叫
  bool InitiateCall(const QString& target_client_id);
  
  // 取消呼叫（主叫方）
  void CancelCall();
  
  // 接听呼叫（被叫方）
  void AcceptCall();
  
  // 拒绝呼叫（被叫方）
  void RejectCall(const QString& reason = QString());
  
  // 结束通话
  void EndCall();
  
  // 获取当前状态
  CallState GetCallState() const { return call_state_; }
  QString GetCurrentPeer() const { return current_peer_; }
  bool IsInCall() const { return call_state_ != CallState::Idle; }
  
  // 通知对等连接已建立（由Conductor调用）
  void NotifyPeerConnectionEstablished();
  
  // 处理来自信令服务器的消息（由Conductor调用）
  void HandleCallRequest(const QString& from);
  void HandleCallResponse(const QString& from, bool accepted, const QString& reason);
  void HandleCallCancel(const QString& from, const QString& reason);
  void HandleCallEnd(const QString& from, const QString& reason);

 signals:
  void CallStateChanged(CallState state, QString peer_id);
  void IncomingCall(QString caller_id);

 private slots:
  void OnCallRequestTimeout();

 private:
  void SetCallState(CallState state);
  void CleanupCall();
  void StartCallRequestTimer();
  void StopCallRequestTimer();
  
  SignalClient* signal_client_;
  CallManagerObserver* observer_;
  
  CallState call_state_;
  QString current_peer_;
  bool is_caller_;  // 是否是主叫方
  
  std::unique_ptr<QTimer> call_request_timer_;
  
  static constexpr int kCallRequestTimeoutMs = 30000;  // 30秒超时
};

#endif  // EXAMPLES_PEERCONNECTION_CLIENT_CALLMANAGER_H_
