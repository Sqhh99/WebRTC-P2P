#ifndef EXAMPLES_PEERCONNECTION_CLIENT_CAPTURER_TRACK_SOURCE_H_
#define EXAMPLES_PEERCONNECTION_CLIENT_CAPTURER_TRACK_SOURCE_H_

#include <memory>
#include <cstdint>

#include "absl/types/optional.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "media/base/adapted_video_track_source.h"
#include "rtc_base/thread.h"

namespace webrtc {
class TaskQueueFactory;
}  // namespace webrtc

class CapturerTrackSource : public webrtc::AdaptedVideoTrackSource {
 public:
  static webrtc::scoped_refptr<CapturerTrackSource> Create(
      webrtc::TaskQueueFactory* task_queue_factory);

 protected:
  explicit CapturerTrackSource();
  ~CapturerTrackSource() override;

 private:
  void GenerateFrame();

  // AdaptedVideoTrackSource overrides
  bool is_screencast() const override { return false; }
  absl::optional<bool> needs_denoising() const override { return absl::nullopt; }
  webrtc::MediaSourceInterface::SourceState state() const override {
    return webrtc::MediaSourceInterface::kLive;
  }
  bool remote() const override { return false; }

  std::unique_ptr<webrtc::Thread> thread_;
  uint32_t frame_count_ = 0;
};

#endif  // EXAMPLES_PEERCONNECTION_CLIENT_CAPTURER_TRACK_SOURCE_H_