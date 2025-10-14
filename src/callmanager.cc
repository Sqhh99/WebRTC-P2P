#include "callmanager.h"
#include <QDebug>

CallManager::CallManager(QObject* parent)
    : QObject(parent),
      signal_client_(nullptr),
      observer_(nullptr),
      call_state_(CallState::Idle),
      is_caller_(false) {
  call_request_timer_ = std::make_unique<QTimer>(this);
  call_request_timer_->setSingleShot(true);
  connect(call_request_timer_.get(), &QTimer::timeout, this, &CallManager::OnCallRequestTimeout);
}

CallManager::~CallManager() = default;

void CallManager::SetSignalClient(SignalClient* signal_client) {
  signal_client_ = signal_client;
}

void CallManager::RegisterObserver(CallManagerObserver* observer) {
  observer_ = observer;
}

bool CallManager::InitiateCall(const QString& target_client_id) {
  if (!signal_client_ || !signal_client_->IsConnected()) {
    qWarning() << "Cannot initiate call: signal client not connected";
    return false;
  }
  
  if (call_state_ != CallState::Idle) {
    qWarning() << "Cannot initiate call: already in a call";
    return false;
  }
  
  qDebug() << "Initiating call to:" << target_client_id;
  
  current_peer_ = target_client_id;
  is_caller_ = true;
  SetCallState(CallState::Calling);
  
  // 发送呼叫请求
  signal_client_->SendCallRequest(target_client_id);
  
  // 启动超时计时器
  StartCallRequestTimer();
  
  return true;
}

void CallManager::CancelCall() {
  if (call_state_ != CallState::Calling) {
    qWarning() << "Cannot cancel: not in calling state";
    return;
  }
  
  qDebug() << "Cancelling call to:" << current_peer_;
  
  // 发送取消消息
  if (signal_client_ && !current_peer_.isEmpty()) {
    signal_client_->SendCallCancel(current_peer_, "cancelled");
  }
  
  CleanupCall();
}

void CallManager::AcceptCall() {
  if (call_state_ != CallState::Receiving) {
    qWarning() << "Cannot accept: not in receiving state";
    return;
  }
  
  qDebug() << "Accepting call from:" << current_peer_;
  
  // 发送接受响应
  signal_client_->SendCallResponse(current_peer_, true);
  
  SetCallState(CallState::Connecting);
  
  // 被叫方接受呼叫时创建 PeerConnection，准备接收 Offer
  if (observer_) {
    observer_->OnNeedCreatePeerConnection(current_peer_.toStdString(), false);
  }
  
  qDebug() << "PeerConnection created, waiting for offer from:" << current_peer_;
}

void CallManager::RejectCall(const QString& reason) {
  if (call_state_ != CallState::Receiving) {
    qWarning() << "Cannot reject: not in receiving state";
    return;
  }
  
  qDebug() << "Rejecting call from:" << current_peer_;
  
  // 发送拒绝响应
  QString reject_reason = reason.isEmpty() ? "rejected" : reason;
  signal_client_->SendCallResponse(current_peer_, false, reject_reason);
  
  CleanupCall();
}

void CallManager::EndCall() {
  if (call_state_ == CallState::Idle) {
    return;
  }
  
  qDebug() << "Ending call with:" << current_peer_;
  
  // 发送结束消息
  if (signal_client_ && !current_peer_.isEmpty()) {
    signal_client_->SendCallEnd(current_peer_, "hangup");
  }
  
  // 通知需要关闭对等连接
  if (observer_) {
    observer_->OnNeedClosePeerConnection();
  }
  
  if (observer_) {
    observer_->OnCallEnded(current_peer_.toStdString(), "hangup");
  }
  
  CleanupCall();
}

void CallManager::NotifyPeerConnectionEstablished() {
  if (call_state_ == CallState::Connecting) {
    qDebug() << "Peer connection established, call connected";
    SetCallState(CallState::Connected);
    
    if (observer_) {
      observer_->OnCallAccepted(current_peer_.toStdString());
    }
  }
}

void CallManager::OnCallRequestTimeout() {
  if (call_state_ == CallState::Calling) {
    qWarning() << "Call request timeout";
    
    if (observer_) {
      observer_->OnCallTimeout();
    }
    
    CleanupCall();
  }
}

void CallManager::SetCallState(CallState state) {
  if (call_state_ == state) {
    return;
  }
  
  qDebug() << "Call state changed:" << static_cast<int>(call_state_) 
           << "->" << static_cast<int>(state);
  
  call_state_ = state;
  
  if (observer_) {
    observer_->OnCallStateChanged(state, current_peer_.toStdString());
  }
  
  emit CallStateChanged(state, current_peer_);
}

void CallManager::CleanupCall() {
  qDebug() << "Cleaning up call resources";
  
  StopCallRequestTimer();
  current_peer_.clear();
  is_caller_ = false;
  SetCallState(CallState::Idle);
}

void CallManager::StartCallRequestTimer() {
  call_request_timer_->start(kCallRequestTimeoutMs);
}

void CallManager::StopCallRequestTimer() {
  if (call_request_timer_->isActive()) {
    call_request_timer_->stop();
  }
}

void CallManager::HandleCallRequest(const QString& from) {
  qDebug() << "HandleCallRequest from:" << from << "current state:" << static_cast<int>(call_state_);
  
  // 如果已经在通话中，自动拒绝
  if (call_state_ != CallState::Idle) {
    qWarning() << "Rejecting call from" << from << "- already in a call";
    if (signal_client_) {
      signal_client_->SendCallResponse(from, false, "busy");
    }
    return;
  }
  
  current_peer_ = from;
  is_caller_ = false;
  SetCallState(CallState::Receiving);
  
  // 通知观察者有来电
  if (observer_) {
    observer_->OnIncomingCall(from.toStdString());
  }
  
  emit IncomingCall(from);
}

void CallManager::HandleCallResponse(const QString& from, bool accepted, const QString& reason) {
  qDebug() << "HandleCallResponse from:" << from << "accepted:" << accepted << "reason:" << reason;
  
  // 只有在呼叫状态才处理响应
  if (call_state_ != CallState::Calling || current_peer_ != from) {
    qWarning() << "Ignoring call response - not in calling state or wrong peer";
    return;
  }
  
  StopCallRequestTimer();
  
  if (accepted) {
    qDebug() << "Call accepted by:" << from;
    SetCallState(CallState::Connecting);
    
    // 通知观察者呼叫被接受，需要创建对等连接（主叫方）
    if (observer_) {
      observer_->OnCallAccepted(from.toStdString());
      observer_->OnNeedCreatePeerConnection(from.toStdString(), true);
    }
  } else {
    qDebug() << "Call rejected by:" << from << "reason:" << reason;
    
    if (observer_) {
      observer_->OnCallRejected(from.toStdString(), reason.toStdString());
    }
    
    CleanupCall();
  }
}

void CallManager::HandleCallCancel(const QString& from, const QString& reason) {
  qDebug() << "HandleCallCancel from:" << from << "reason:" << reason;
  
  // 只有在接听状态才处理取消
  if (call_state_ != CallState::Receiving || current_peer_ != from) {
    qWarning() << "Ignoring call cancel - not in receiving state or wrong peer";
    return;
  }
  
  if (observer_) {
    observer_->OnCallCancelled(from.toStdString(), reason.toStdString());
  }
  
  CleanupCall();
}

void CallManager::HandleCallEnd(const QString& from, const QString& reason) {
  qDebug() << "HandleCallEnd from:" << from << "reason:" << reason;
  
  // 在任何通话状态都可以处理结束
  if (current_peer_ != from) {
    qWarning() << "Ignoring call end - wrong peer";
    return;
  }
  
  if (observer_) {
    observer_->OnCallEnded(from.toStdString(), reason.toStdString());
  }
  
  // 通知需要关闭对等连接
  if (observer_) {
    observer_->OnNeedClosePeerConnection();
  }
  
  CleanupCall();
}
