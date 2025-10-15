#ifndef CONDUCTOR_H_GUARD
#define CONDUCTOR_H_GUARD

#include <memory>
#include <string>
#include "api/environment/environment.h"
#include "api/peer_connection_interface.h"
#include "signalclient.h"
#include "callmanager.h"
#include "webrtcengine.h"
#include <QJsonObject>
#include <QJsonArray>

class MainWnd;

// Conductor - 连接层，连接WebRTC引擎和UI层
class Conductor : public WebRTCEngineObserver,
                  public SignalClientObserver,
                  public CallManagerObserver {
 public:
  Conductor(const webrtc::Environment& env, MainWnd* main_wnd);
  ~Conductor();
  
  // 初始化
  bool Initialize();
  
  // 生命周期
  void Shutdown();

 private:
  // WebRTCEngineObserver 实现
  void OnLocalVideoTrackAdded(webrtc::VideoTrackInterface* track) override;
  void OnRemoteVideoTrackAdded(webrtc::VideoTrackInterface* track) override;
  void OnRemoteVideoTrackRemoved() override;
  void OnIceConnectionStateChanged(webrtc::PeerConnectionInterface::IceConnectionState state) override;
  void OnOfferCreated(const std::string& sdp) override;
  void OnAnswerCreated(const std::string& sdp) override;
  void OnIceCandidateGenerated(const std::string& sdp_mid, int sdp_mline_index, const std::string& candidate) override;
  void OnError(const std::string& error) override;
  
  // SignalClientObserver 实现
  void OnConnected(const std::string& client_id) override;
  void OnDisconnected() override;
  void OnConnectionError(const std::string& error) override;
  void OnIceServersReceived(const std::vector<IceServerConfig>& ice_servers) override;
  void OnClientListUpdate(const QJsonArray& clients) override;
  void OnUserOffline(const std::string& client_id) override;
  void OnCallRequest(const std::string& from, const QJsonObject& payload) override;
  void OnCallResponse(const std::string& from, bool accepted, const std::string& reason) override;
  void OnCallCancel(const std::string& from, const std::string& reason) override;
  void OnCallEnd(const std::string& from, const std::string& reason) override;
  void OnOffer(const std::string& from, const QJsonObject& sdp) override;
  void OnAnswer(const std::string& from, const QJsonObject& sdp) override;
  void OnIceCandidate(const std::string& from, const QJsonObject& candidate) override;

  // CallManagerObserver 实现
  void OnCallStateChanged(CallState state, const std::string& peer_id) override;
  void OnIncomingCall(const std::string& caller_id) override;
  void OnCallAccepted(const std::string& peer_id) override;
  void OnCallRejected(const std::string& peer_id, const std::string& reason) override;
  void OnCallCancelled(const std::string& peer_id, const std::string& reason) override;
  void OnCallEnded(const std::string& peer_id, const std::string& reason) override;
  void OnCallTimeout() override;
  void OnNeedCreatePeerConnection(const std::string& peer_id, bool is_caller) override;
  void OnNeedClosePeerConnection() override;

 private:
  void ProcessOffer(const std::string& from, const QJsonObject& sdp);
  void ProcessAnswer(const std::string& from, const QJsonObject& sdp);
  void ProcessIceCandidate(const std::string& from, const QJsonObject& candidate);
  
  std::string current_peer_id_;
  bool is_caller_;
  const webrtc::Environment env_;
  MainWnd* main_wnd_;
  std::unique_ptr<WebRTCEngine> webrtc_engine_;
  std::vector<IceServerConfig> ice_servers_;  // ICE 服务器配置
};

#endif