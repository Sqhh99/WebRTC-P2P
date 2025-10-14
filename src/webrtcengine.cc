/*
 *  WebRTC引擎实现 - 封装所有WebRTC核心逻辑，与UI完全解耦
 */

#include "webrtcengine.h"

#include <utility>
#include <optional>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_modular_peer_connection_factory.h"
#include "api/enable_media.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/test/create_frame_generator.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "modules/video_capture/video_capture_factory.h"
#include "pc/video_track_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "test/frame_generator.h"
#include "test/frame_generator_capturer.h"
#include "test/platform_video_capturer.h"
#include "test/test_video_capturer.h"


namespace {

using webrtc::test::TestVideoCapturer;

// 设置远程描述的观察者
class SetRemoteDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  static webrtc::scoped_refptr<SetRemoteDescriptionObserver> Create(
      std::function<void(webrtc::RTCError)> callback) {
    return webrtc::make_ref_counted<SetRemoteDescriptionObserver>(callback);
  }
  
  explicit SetRemoteDescriptionObserver(std::function<void(webrtc::RTCError)> callback)
      : callback_(callback) {}
  
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    if (callback_) {
      callback_(error);
    }
  }
  
 private:
  std::function<void(webrtc::RTCError)> callback_;
};

// 设置本地描述的观察者
class SetLocalDescriptionObserver
    : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  static webrtc::scoped_refptr<SetLocalDescriptionObserver> Create(
      std::function<void(webrtc::RTCError)> callback) {
    return webrtc::make_ref_counted<SetLocalDescriptionObserver>(callback);
  }
  
  explicit SetLocalDescriptionObserver(std::function<void(webrtc::RTCError)> callback)
      : callback_(callback) {}
  
  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
    if (callback_) {
      callback_(error);
    }
  }
  
 private:
  std::function<void(webrtc::RTCError)> callback_;
};

// 创建视频捕获器
std::unique_ptr<TestVideoCapturer> CreateCapturer(
    webrtc::TaskQueueFactory& task_queue_factory) {
  const size_t kWidth = 640;
  const size_t kHeight = 480;
  const size_t kFps = 30;
  
  std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());
  if (!info) {
    return nullptr;
  }
  
  int num_devices = info->NumberOfDevices();
  for (int i = 0; i < num_devices; ++i) {
    std::unique_ptr<TestVideoCapturer> capturer =
        webrtc::test::CreateVideoCapturer(kWidth, kHeight, kFps, i);
    if (capturer) {
      return capturer;
    }
  }
  
  auto frame_generator = webrtc::test::CreateSquareFrameGenerator(
      kWidth, kHeight, std::nullopt, std::nullopt);
  return std::make_unique<webrtc::test::FrameGeneratorCapturer>(
      webrtc::Clock::GetRealTimeClock(), std::move(frame_generator), kFps,
      task_queue_factory);
}

// 视频轨道源
class CapturerTrackSource : public webrtc::VideoTrackSource {
 public:
  static webrtc::scoped_refptr<CapturerTrackSource> Create(
      webrtc::TaskQueueFactory& task_queue_factory) {
    std::unique_ptr<TestVideoCapturer> capturer =
        CreateCapturer(task_queue_factory);
    if (capturer) {
      capturer->Start();
      return webrtc::make_ref_counted<CapturerTrackSource>(std::move(capturer));
    }
    return nullptr;
  }

 protected:
  explicit CapturerTrackSource(std::unique_ptr<TestVideoCapturer> capturer)
      : VideoTrackSource(/*remote=*/false), capturer_(std::move(capturer)) {}

 private:
  webrtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
    return capturer_.get();
  }

  std::unique_ptr<TestVideoCapturer> capturer_;
};

}  // namespace

// ============================================================================
// 内部观察者类实现
// ============================================================================

// PeerConnectionObserver的内部实现
class WebRTCEngine::PeerConnectionObserverImpl : public webrtc::PeerConnectionObserver {
 public:
  explicit PeerConnectionObserverImpl(WebRTCEngine* engine) : engine_(engine) {}
  
  void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {}
  void OnAddTrack(webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
                  const std::vector<webrtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override {
    engine_->OnPeerConnectionAddTrack(receiver.get());
  }
  void OnRemoveTrack(webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override {
    engine_->OnPeerConnectionRemoveTrack(receiver.get());
  }
  void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    engine_->OnPeerConnectionIceConnectionChange(new_state);
  }
  void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
  void OnIceCandidate(const webrtc::IceCandidate* candidate) override {
    engine_->OnPeerConnectionIceCandidate(candidate);
  }
  void OnIceConnectionReceivingChange(bool receiving) override {}
  void OnIceCandidateRemoved(const webrtc::IceCandidate* candidate) override {}
  
 private:
  WebRTCEngine* engine_;
};

// CreateSessionDescriptionObserver的内部实现
class WebRTCEngine::CreateSessionDescriptionObserverImpl : public webrtc::CreateSessionDescriptionObserver {
 public:
  static webrtc::scoped_refptr<CreateSessionDescriptionObserverImpl> Create(
      WebRTCEngine* engine, bool is_offer) {
    return webrtc::make_ref_counted<CreateSessionDescriptionObserverImpl>(engine, is_offer);
  }
  
  explicit CreateSessionDescriptionObserverImpl(WebRTCEngine* engine, bool is_offer)
      : engine_(engine), is_offer_(is_offer) {}
  
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    engine_->OnSessionDescriptionSuccess(desc, is_offer_);
  }
  
  void OnFailure(webrtc::RTCError error) override {
    engine_->OnSessionDescriptionFailure(error.message());
  }
  
 private:
  WebRTCEngine* engine_;
  bool is_offer_;
};

// ============================================================================
// WebRTCEngine 实现
// ============================================================================

WebRTCEngine::WebRTCEngine(const webrtc::Environment& env)
    : env_(env), observer_(nullptr), is_creating_offer_(false) {
}

WebRTCEngine::~WebRTCEngine() {
  ClosePeerConnection();
}

void WebRTCEngine::SetObserver(WebRTCEngineObserver* observer) {
  observer_ = observer;
}

bool WebRTCEngine::Initialize() {
  RTC_DCHECK(!peer_connection_factory_);
  RTC_LOG(LS_INFO) << "Initializing WebRTC Engine...";

  if (!signaling_thread_) {
    signaling_thread_ = webrtc::Thread::CreateWithSocketServer();
    signaling_thread_->Start();
  }

  webrtc::PeerConnectionFactoryDependencies deps;
  deps.signaling_thread = signaling_thread_.get();
  deps.env = env_;
  deps.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
  deps.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
  deps.video_encoder_factory =
      std::make_unique<webrtc::VideoEncoderFactoryTemplate<
          webrtc::LibvpxVp8EncoderTemplateAdapter,
          webrtc::LibvpxVp9EncoderTemplateAdapter,
          webrtc::OpenH264EncoderTemplateAdapter,
          webrtc::LibaomAv1EncoderTemplateAdapter>>();
  deps.video_decoder_factory =
      std::make_unique<webrtc::VideoDecoderFactoryTemplate<
          webrtc::LibvpxVp8DecoderTemplateAdapter,
          webrtc::LibvpxVp9DecoderTemplateAdapter,
          webrtc::OpenH264DecoderTemplateAdapter,
          webrtc::Dav1dDecoderTemplateAdapter>>();
  webrtc::EnableMedia(deps);

  peer_connection_factory_ =
      webrtc::CreateModularPeerConnectionFactory(std::move(deps));

  if (!peer_connection_factory_) {
    RTC_LOG(LS_ERROR) << "Failed to initialize PeerConnectionFactory";
    return false;
  }

  RTC_LOG(LS_INFO) << "WebRTC Engine initialized successfully";
  return true;
}

bool WebRTCEngine::CreatePeerConnection() {
  RTC_DCHECK(peer_connection_factory_);
  RTC_DCHECK(!peer_connection_);

  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  
  webrtc::PeerConnectionInterface::IceServer stun_server;
  stun_server.uri = "stun:stun.l.google.com:19302";
  config.servers.push_back(stun_server);

  // 创建并保存内部观察者 - 必须保持存活!
  pc_observer_ = std::make_unique<PeerConnectionObserverImpl>(this);
  webrtc::PeerConnectionDependencies pc_dependencies(pc_observer_.get());
  auto error_or_peer_connection =
      peer_connection_factory_->CreatePeerConnectionOrError(
          config, std::move(pc_dependencies));
          
  if (error_or_peer_connection.ok()) {
    peer_connection_ = std::move(error_or_peer_connection.value());
    RTC_LOG(LS_INFO) << "PeerConnection created successfully";
    return true;
  } else {
    RTC_LOG(LS_ERROR) << "CreatePeerConnection failed: "
                      << error_or_peer_connection.error().message();
    if (observer_) {
      observer_->OnError(error_or_peer_connection.error().message());
    }
    pc_observer_.reset();  // 清理观察者
    return false;
  }
}

void WebRTCEngine::ClosePeerConnection() {
  peer_connection_ = nullptr;
  pc_observer_.reset();  // 清理观察者
  pending_ice_candidates_.clear();
}

bool WebRTCEngine::AddTracks() {
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "Cannot add tracks: no peer connection";
    return false;
  }

  if (!peer_connection_->GetSenders().empty()) {
    RTC_LOG(LS_WARNING) << "Tracks already added";
    return true;
  }

  // 添加视频轨道
  auto video_device = CapturerTrackSource::Create(env_.task_queue_factory());
  if (video_device) {
    webrtc::scoped_refptr<webrtc::VideoTrackInterface> video_track = 
        peer_connection_factory_->CreateVideoTrack(video_device, "video_label");
    auto result_or_error = peer_connection_->AddTrack(video_track, {"stream_id"});
    
    if (!result_or_error.ok()) {
      RTC_LOG(LS_ERROR) << "Failed to add video track: "
                        << result_or_error.error().message();
      if (observer_) {
        observer_->OnError("Failed to add video track");
      }
      return false;
    } else {
      if (observer_) {
        observer_->OnLocalVideoTrackAdded(video_track.get());
      }
    }
  }

  // 添加音频轨道
  webrtc::AudioOptions audio_options;
  auto audio_source = peer_connection_factory_->CreateAudioSource(audio_options);
  auto audio_track = peer_connection_factory_->CreateAudioTrack("audio_label", audio_source.get());
  auto result_or_error = peer_connection_->AddTrack(audio_track, {"stream_id"});
  
  if (!result_or_error.ok()) {
    RTC_LOG(LS_ERROR) << "Failed to add audio track: "
                      << result_or_error.error().message();
    if (observer_) {
      observer_->OnError("Failed to add audio track");
    }
    return false;
  }

  return true;
}

void WebRTCEngine::CreateOffer() {
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "Cannot create offer: no peer connection";
    return;
  }

  is_creating_offer_ = true;
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = true;
  options.offer_to_receive_video = true;
  
  auto observer = CreateSessionDescriptionObserverImpl::Create(this, true);
  peer_connection_->CreateOffer(observer.get(), options);
}

void WebRTCEngine::CreateAnswer() {
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "Cannot create answer: no peer connection";
    return;
  }

  is_creating_offer_ = false;
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  
  auto observer = CreateSessionDescriptionObserverImpl::Create(this, false);
  peer_connection_->CreateAnswer(observer.get(), options);
}

void WebRTCEngine::SetRemoteOffer(const std::string& sdp) {
  SetRemoteDescription("offer", sdp);
}

void WebRTCEngine::SetRemoteAnswer(const std::string& sdp) {
  SetRemoteDescription("answer", sdp);
}

void WebRTCEngine::SetRemoteDescription(const std::string& type, const std::string& sdp) {
  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << "Cannot set remote description: no peer connection";
    return;
  }

  webrtc::SdpType sdp_type = (type == "offer") ? webrtc::SdpType::kOffer : webrtc::SdpType::kAnswer;
  webrtc::SdpParseError error;
  auto session_desc = webrtc::CreateSessionDescription(sdp_type, sdp, &error);
  
  if (!session_desc) {
    RTC_LOG(LS_ERROR) << "Failed to parse SDP: " << error.description;
    if (observer_) {
      observer_->OnError("Failed to parse SDP: " + error.description);
    }
    return;
  }

  auto observer = SetRemoteDescriptionObserver::Create([this](webrtc::RTCError error) {
    if (!error.ok()) {
      RTC_LOG(LS_ERROR) << "SetRemoteDescription failed: " << error.message();
      if (observer_) {
        observer_->OnError(std::string("SetRemoteDescription failed: ") + error.message());
      }
    } else {
      RTC_LOG(LS_INFO) << "SetRemoteDescription succeeded";
      ProcessPendingIceCandidates();
    }
  });

  peer_connection_->SetRemoteDescription(std::move(session_desc), observer);
}

void WebRTCEngine::AddIceCandidate(const std::string& sdp_mid, 
                                    int sdp_mline_index, 
                                    const std::string& candidate) {
  if (!peer_connection_) {
    RTC_LOG(LS_WARNING) << "Cannot add ICE candidate: no peer connection";
    return;
  }

  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::IceCandidate> ice_candidate(
      webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, candidate, &error));
      
  if (!ice_candidate) {
    RTC_LOG(LS_ERROR) << "Failed to parse ICE candidate: " << error.description;
    return;
  }

  if (!peer_connection_->remote_description()) {
    RTC_LOG(LS_INFO) << "Remote description not set yet, queueing ICE candidate";
    pending_ice_candidates_.push_back(ice_candidate.release());
    return;
  }

  if (!peer_connection_->AddIceCandidate(ice_candidate.get())) {
    RTC_LOG(LS_ERROR) << "Failed to add ICE candidate";
  }
}

void WebRTCEngine::ProcessPendingIceCandidates() {
  if (!peer_connection_ || !peer_connection_->remote_description()) {
    return;
  }

  for (const auto* candidate : pending_ice_candidates_) {
    if (!peer_connection_->AddIceCandidate(candidate)) {
      RTC_LOG(LS_ERROR) << "Failed to add pending ICE candidate";
    }
    delete candidate;
  }
  pending_ice_candidates_.clear();
}

bool WebRTCEngine::IsConnected() const {
  if (!peer_connection_) {
    return false;
  }
  
  auto state = peer_connection_->ice_connection_state();
  return state == webrtc::PeerConnectionInterface::kIceConnectionConnected ||
         state == webrtc::PeerConnectionInterface::kIceConnectionCompleted;
}

void WebRTCEngine::Shutdown() {
  ClosePeerConnection();
  peer_connection_factory_ = nullptr;
  signaling_thread_ = nullptr;
}

// ============================================================================
// 内部回调方法 - 从观察者类调用
// ============================================================================

void WebRTCEngine::OnPeerConnectionAddTrack(webrtc::RtpReceiverInterface* receiver) {
  RTC_LOG(LS_INFO) << "Track added: " << receiver->id();
  auto* track = receiver->track().get();
  
  if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
    auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
    if (observer_) {
      observer_->OnRemoteVideoTrackAdded(video_track);
    }
  }
}

void WebRTCEngine::OnPeerConnectionRemoveTrack(webrtc::RtpReceiverInterface* receiver) {
  RTC_LOG(LS_INFO) << "Track removed: " << receiver->id();
  auto* track = receiver->track().get();
  
  if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
    if (observer_) {
      observer_->OnRemoteVideoTrackRemoved();
    }
  }
}

void WebRTCEngine::OnPeerConnectionIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << "ICE connection state changed: " << new_state;
  
  if (observer_) {
    observer_->OnIceConnectionStateChanged(new_state);
  }
}

void WebRTCEngine::OnPeerConnectionIceCandidate(const webrtc::IceCandidate* candidate) {
  RTC_LOG(LS_INFO) << "ICE candidate generated: " << candidate->sdp_mline_index();
  
  std::string candidate_str;
  if (candidate->ToString(&candidate_str)) {
    if (observer_) {
      observer_->OnIceCandidateGenerated(
          candidate->sdp_mid(), 
          candidate->sdp_mline_index(), 
          candidate_str);
    }
  }
}

void WebRTCEngine::OnSessionDescriptionSuccess(webrtc::SessionDescriptionInterface* desc, bool is_offer) {
  std::string sdp;
  desc->ToString(&sdp);

  auto observer = SetLocalDescriptionObserver::Create([this, sdp, is_offer](webrtc::RTCError error) {
    if (!error.ok()) {
      RTC_LOG(LS_ERROR) << "SetLocalDescription failed: " << error.message();
      if (observer_) {
        observer_->OnError(std::string("SetLocalDescription failed: ") + error.message());
      }
    } else {
      RTC_LOG(LS_INFO) << "SetLocalDescription succeeded";
      
      if (observer_) {
        if (is_offer) {
          observer_->OnOfferCreated(sdp);
        } else {
          observer_->OnAnswerCreated(sdp);
        }
      }
    }
  });

  peer_connection_->SetLocalDescription(
      std::unique_ptr<webrtc::SessionDescriptionInterface>(desc), observer);
}

void WebRTCEngine::OnSessionDescriptionFailure(const std::string& error) {
  RTC_LOG(LS_ERROR) << "Create session description failed: " << error;
  
  if (observer_) {
    observer_->OnError("Create session description failed: " + error);
  }
}
