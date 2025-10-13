#include "capturer_track_source.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace {
constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr int kFps = 30;
}  // namespace

webrtc::scoped_refptr<CapturerTrackSource> CapturerTrackSource::Create(
    webrtc::TaskQueueFactory* task_queue_factory) {
  return webrtc::make_ref_counted<CapturerTrackSource>();
}

CapturerTrackSource::CapturerTrackSource()
    : webrtc::AdaptedVideoTrackSource() {
  thread_ = webrtc::Thread::Create();
  thread_->Start();
  
  // 每隔 1000/fps 毫秒生成一帧
  thread_->PostDelayedTask([this]() {
    GenerateFrame();
    thread_->PostDelayedTask([this]() { GenerateFrame(); }, 
                             webrtc::TimeDelta::Millis(1000 / kFps));
  }, webrtc::TimeDelta::Millis(0));
}

CapturerTrackSource::~CapturerTrackSource() {
  if (thread_) {
    thread_->Stop();
  }
}

void CapturerTrackSource::GenerateFrame() {
  // 创建一个简单的彩色渐变帧
  webrtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kWidth, kHeight);

  // 生成渐变色
  uint8_t y_value = (frame_count_ * 2) % 256;
  for (int y = 0; y < kHeight; ++y) {
    memset(buffer->MutableDataY() + y * buffer->StrideY(), 
           (y_value + y) % 256, kWidth);
  }
  
  // U 和 V 分量设为中间值（灰色）
  memset(buffer->MutableDataU(), 128, buffer->ChromaWidth() * buffer->ChromaHeight());
  memset(buffer->MutableDataV(), 128, buffer->ChromaWidth() * buffer->ChromaHeight());

  webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
                                  .set_video_frame_buffer(buffer)
                                  .set_timestamp_us(webrtc::TimeMicros())
                                  .build();

  // 调用父类的 OnFrame 方法，传递帧给所有订阅者
  webrtc::AdaptedVideoTrackSource::OnFrame(frame);
  frame_count_++;
}