/*
 *  Conductor - 连接层，连接WebRTC引擎和UI层
 *  职责：协调WebRTC引擎、信令客户端和呼叫管理器，不直接处理WebRTC细节
 */

#include "conductor.h"
#include "mainwindow.h"
#include "rtc_base/logging.h"
#include <QMetaObject>
#include <QJsonDocument>

// ============================================================================
// 构造和析构
// ============================================================================

Conductor::Conductor(const webrtc::Environment& env, MainWnd* main_wnd)
    : is_caller_(false), env_(env), main_wnd_(main_wnd) {
  webrtc_engine_ = std::make_unique<WebRTCEngine>(env);
  webrtc_engine_->SetObserver(this);
}

Conductor::~Conductor() {
  Shutdown();
}

// ============================================================================
// 公有方法
// ============================================================================

bool Conductor::Initialize() {
  RTC_LOG(LS_INFO) << "Initializing Conductor...";
  return webrtc_engine_->Initialize();
}

void Conductor::Shutdown() {
  if (webrtc_engine_) {
    webrtc_engine_->Shutdown();
  }
  current_peer_id_.clear();
}

// ============================================================================
// WebRTCEngineObserver 实现 - 处理WebRTC引擎的回调
// ============================================================================

void Conductor::OnLocalVideoTrackAdded(webrtc::VideoTrackInterface* track) {
  RTC_LOG(LS_INFO) << "Local video track added";
  QMetaObject::invokeMethod(main_wnd_, [this, track]() {
    main_wnd_->StartLocalRenderer(track);
  }, Qt::QueuedConnection);
}

void Conductor::OnRemoteVideoTrackAdded(webrtc::VideoTrackInterface* track) {
  RTC_LOG(LS_INFO) << "Remote video track added";
  QMetaObject::invokeMethod(main_wnd_, [this, track]() {
    main_wnd_->StartRemoteRenderer(track);
  }, Qt::QueuedConnection);
}

void Conductor::OnRemoteVideoTrackRemoved() {
  RTC_LOG(LS_INFO) << "Remote video track removed";
  QMetaObject::invokeMethod(main_wnd_, [this]() {
    main_wnd_->StopRemoteRenderer();
  }, Qt::QueuedConnection);
}

void Conductor::OnIceConnectionStateChanged(webrtc::PeerConnectionInterface::IceConnectionState state) {
  RTC_LOG(LS_INFO) << "ICE connection state changed: " << state;
  
  if (state == webrtc::PeerConnectionInterface::kIceConnectionConnected ||
      state == webrtc::PeerConnectionInterface::kIceConnectionCompleted) {
    if (main_wnd_->GetCallManager()) {
      main_wnd_->GetCallManager()->NotifyPeerConnectionEstablished();
    }
  } else if (state == webrtc::PeerConnectionInterface::kIceConnectionFailed ||
             state == webrtc::PeerConnectionInterface::kIceConnectionDisconnected ||
             state == webrtc::PeerConnectionInterface::kIceConnectionClosed) {
    QMetaObject::invokeMethod(main_wnd_, [this]() {
      main_wnd_->AppendLog("ICE连接已断开", "warning");
    }, Qt::QueuedConnection);
  }
}

void Conductor::OnOfferCreated(const std::string& sdp) {
  RTC_LOG(LS_INFO) << "Offer created, sending to peer...";
  
  QJsonObject json_sdp;
  json_sdp["type"] = "offer";
  json_sdp["sdp"] = QString::fromStdString(sdp);
  
  QMetaObject::invokeMethod(main_wnd_, [this, json_sdp]() {
    if (main_wnd_->GetSignalClient()) {
      main_wnd_->GetSignalClient()->SendOffer(
          QString::fromStdString(current_peer_id_), json_sdp);
    }
  }, Qt::QueuedConnection);
}

void Conductor::OnAnswerCreated(const std::string& sdp) {
  RTC_LOG(LS_INFO) << "Answer created, sending to peer...";
  
  QJsonObject json_sdp;
  json_sdp["type"] = "answer";
  json_sdp["sdp"] = QString::fromStdString(sdp);
  
  QMetaObject::invokeMethod(main_wnd_, [this, json_sdp]() {
    if (main_wnd_->GetSignalClient()) {
      main_wnd_->GetSignalClient()->SendAnswer(
          QString::fromStdString(current_peer_id_), json_sdp);
    }
  }, Qt::QueuedConnection);
}

void Conductor::OnIceCandidateGenerated(const std::string& sdp_mid, 
                                         int sdp_mline_index, 
                                         const std::string& candidate) {
  RTC_LOG(LS_INFO) << "ICE candidate generated: " << sdp_mline_index;
  
  QJsonObject json_candidate;
  json_candidate["sdpMid"] = QString::fromStdString(sdp_mid);
  json_candidate["sdpMLineIndex"] = sdp_mline_index;
  json_candidate["candidate"] = QString::fromStdString(candidate);
  
  QMetaObject::invokeMethod(main_wnd_, [this, json_candidate]() {
    if (main_wnd_->GetSignalClient()) {
      main_wnd_->GetSignalClient()->SendIceCandidate(
          QString::fromStdString(current_peer_id_), json_candidate);
    }
  }, Qt::QueuedConnection);
}

void Conductor::OnError(const std::string& error) {
  RTC_LOG(LS_ERROR) << "WebRTC Engine error: " << error;
  QMetaObject::invokeMethod(main_wnd_, [this, error]() {
    main_wnd_->ShowError("WebRTC错误", QString::fromStdString(error));
  }, Qt::QueuedConnection);
}

// ============================================================================
// SignalClientObserver 实现 - 处理信令消息
// ============================================================================

void Conductor::OnConnected(const std::string& client_id) {
  RTC_LOG(LS_INFO) << "Connected to signaling server: " << client_id;
  QMetaObject::invokeMethod(main_wnd_, [this, client_id]() {
    main_wnd_->AppendLog("已连接到服务器，客户端ID: " + QString::fromStdString(client_id), "success");
  }, Qt::QueuedConnection);
}

void Conductor::OnDisconnected() {
  RTC_LOG(LS_INFO) << "Disconnected from signaling server";
  QMetaObject::invokeMethod(main_wnd_, [this]() {
    main_wnd_->AppendLog("已断开与服务器的连接", "warning");
  }, Qt::QueuedConnection);
}

void Conductor::OnConnectionError(const std::string& error) {
  RTC_LOG(LS_ERROR) << "Signaling connection error: " << error;
  QMetaObject::invokeMethod(main_wnd_, [this, error]() {
    main_wnd_->AppendLog("连接错误: " + QString::fromStdString(error), "error");
  }, Qt::QueuedConnection);
}

void Conductor::OnClientListUpdate(const QJsonArray& clients) {
  RTC_LOG(LS_INFO) << "Client list updated: " << clients.size() << " clients";
  main_wnd_->UpdateClientList(clients);
}

void Conductor::OnUserOffline(const std::string& client_id) {
  RTC_LOG(LS_INFO) << "User offline: " << client_id;
  if (client_id == current_peer_id_ && main_wnd_->GetCallManager()) {
    main_wnd_->GetCallManager()->EndCall();
  }
}

void Conductor::OnCallRequest(const std::string& from, const QJsonObject& payload) {
  RTC_LOG(LS_INFO) << "Call request from: " << from;
  if (main_wnd_->GetCallManager()) {
    QMetaObject::invokeMethod(main_wnd_, [this, from]() {
      main_wnd_->GetCallManager()->HandleCallRequest(QString::fromStdString(from));
    }, Qt::QueuedConnection);
  }
}

void Conductor::OnCallResponse(const std::string& from, bool accepted, const std::string& reason) {
  RTC_LOG(LS_INFO) << "Call response from: " << from << " accepted: " << accepted;
  if (main_wnd_->GetCallManager()) {
    QMetaObject::invokeMethod(main_wnd_, [this, from, accepted, reason]() {
      main_wnd_->GetCallManager()->HandleCallResponse(
          QString::fromStdString(from), 
          accepted, 
          QString::fromStdString(reason));
    }, Qt::QueuedConnection);
  }
}

void Conductor::OnCallCancel(const std::string& from, const std::string& reason) {
  RTC_LOG(LS_INFO) << "Call cancelled by: " << from;
  if (main_wnd_->GetCallManager()) {
    QMetaObject::invokeMethod(main_wnd_, [this, from, reason]() {
      main_wnd_->GetCallManager()->HandleCallCancel(
          QString::fromStdString(from), 
          QString::fromStdString(reason));
    }, Qt::QueuedConnection);
  }
}

void Conductor::OnCallEnd(const std::string& from, const std::string& reason) {
  RTC_LOG(LS_INFO) << "Call ended by: " << from;
  if (main_wnd_->GetCallManager()) {
    QMetaObject::invokeMethod(main_wnd_, [this, from, reason]() {
      main_wnd_->GetCallManager()->HandleCallEnd(
          QString::fromStdString(from), 
          QString::fromStdString(reason));
    }, Qt::QueuedConnection);
  }
}

void Conductor::OnOffer(const std::string& from, const QJsonObject& sdp) {
  RTC_LOG(LS_INFO) << "Received offer from: " << from;
  QMetaObject::invokeMethod(main_wnd_, [this, from, sdp]() {
    ProcessOffer(from, sdp);
  }, Qt::QueuedConnection);
}

void Conductor::OnAnswer(const std::string& from, const QJsonObject& sdp) {
  RTC_LOG(LS_INFO) << "Received answer from: " << from;
  QMetaObject::invokeMethod(main_wnd_, [this, from, sdp]() {
    ProcessAnswer(from, sdp);
  }, Qt::QueuedConnection);
}

void Conductor::OnIceCandidate(const std::string& from, const QJsonObject& candidate) {
  RTC_LOG(LS_INFO) << "Received ICE candidate from: " << from;
  QMetaObject::invokeMethod(main_wnd_, [this, from, candidate]() {
    ProcessIceCandidate(from, candidate);
  }, Qt::QueuedConnection);
}

// ============================================================================
// CallManagerObserver 实现 - 处理呼叫流程
// ============================================================================

void Conductor::OnCallStateChanged(CallState state, const std::string& peer_id) {
  RTC_LOG(LS_INFO) << "Call state changed: " << static_cast<int>(state);
}

void Conductor::OnIncomingCall(const std::string& caller_id) {
  RTC_LOG(LS_INFO) << "Incoming call from: " << caller_id;
}

void Conductor::OnCallAccepted(const std::string& peer_id) {
  RTC_LOG(LS_INFO) << "Call accepted by: " << peer_id;
}

void Conductor::OnCallRejected(const std::string& peer_id, const std::string& reason) {
  RTC_LOG(LS_INFO) << "Call rejected by: " << peer_id << " reason: " << reason;
  
  // 停止视频渲染器
  QMetaObject::invokeMethod(main_wnd_, [this]() {
    main_wnd_->StopLocalRenderer();
    main_wnd_->StopRemoteRenderer();
  }, Qt::QueuedConnection);
  
  if (webrtc_engine_) {
    webrtc_engine_->ClosePeerConnection();
  }
}

void Conductor::OnCallCancelled(const std::string& peer_id, const std::string& reason) {
  RTC_LOG(LS_INFO) << "Call cancelled by: " << peer_id << " reason: " << reason;
  
  // 停止视频渲染器
  QMetaObject::invokeMethod(main_wnd_, [this]() {
    main_wnd_->StopLocalRenderer();
    main_wnd_->StopRemoteRenderer();
  }, Qt::QueuedConnection);
  
  if (webrtc_engine_) {
    webrtc_engine_->ClosePeerConnection();
  }
}

void Conductor::OnCallEnded(const std::string& peer_id, const std::string& reason) {
  RTC_LOG(LS_INFO) << "Call ended with: " << peer_id << " reason: " << reason;
  
  // 停止视频渲染器
  QMetaObject::invokeMethod(main_wnd_, [this]() {
    main_wnd_->StopLocalRenderer();
    main_wnd_->StopRemoteRenderer();
  }, Qt::QueuedConnection);
  
  if (webrtc_engine_) {
    webrtc_engine_->ClosePeerConnection();
  }
}

void Conductor::OnCallTimeout() {
  RTC_LOG(LS_INFO) << "Call timeout";
  
  // 停止视频渲染器
  QMetaObject::invokeMethod(main_wnd_, [this]() {
    main_wnd_->StopLocalRenderer();
    main_wnd_->StopRemoteRenderer();
  }, Qt::QueuedConnection);
  
  if (webrtc_engine_) {
    webrtc_engine_->ClosePeerConnection();
  }
}

void Conductor::OnNeedCreatePeerConnection(const std::string& peer_id, bool is_caller) {
  RTC_LOG(LS_INFO) << "Need create peer connection with: " << peer_id << " is_caller: " << is_caller;
  
  current_peer_id_ = peer_id;
  is_caller_ = is_caller;
  
  if (!webrtc_engine_->HasPeerConnection()) {
    if (webrtc_engine_->CreatePeerConnection()) {
      webrtc_engine_->AddTracks();
      
      if (is_caller) {
        webrtc_engine_->CreateOffer();
      }
    } else {
      RTC_LOG(LS_ERROR) << "Failed to create peer connection";
      QMetaObject::invokeMethod(main_wnd_, [this]() {
        main_wnd_->ShowError("错误", "创建连接失败");
      }, Qt::QueuedConnection);
    }
  }
}

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

// ============================================================================
// 私有方法 - 处理信令消息的详细逻辑
// ============================================================================

void Conductor::ProcessOffer(const std::string& from, const QJsonObject& sdp) {
  RTC_LOG(LS_INFO) << "Processing offer from: " << from;
  
  if (!webrtc_engine_->HasPeerConnection()) {
    RTC_LOG(LS_ERROR) << "No peer connection exists when processing offer!";
    return;
  }
  
  current_peer_id_ = from;
  is_caller_ = false;
  
  std::string sdp_str = sdp["sdp"].toString().toStdString();
  webrtc_engine_->SetRemoteOffer(sdp_str);
  webrtc_engine_->CreateAnswer();
}

void Conductor::ProcessAnswer(const std::string& from, const QJsonObject& sdp) {
  RTC_LOG(LS_INFO) << "Processing answer from: " << from;
  
  std::string sdp_str = sdp["sdp"].toString().toStdString();
  webrtc_engine_->SetRemoteAnswer(sdp_str);
}

void Conductor::ProcessIceCandidate(const std::string& from, const QJsonObject& candidate) {
  RTC_LOG(LS_INFO) << "Processing ICE candidate from: " << from;
  
  std::string sdp_mid = candidate["sdpMid"].toString().toStdString();
  int sdp_mline_index = candidate["sdpMLineIndex"].toInt();
  std::string sdp = candidate["candidate"].toString().toStdString();
  
  webrtc_engine_->AddIceCandidate(sdp_mid, sdp_mline_index, sdp);
}
