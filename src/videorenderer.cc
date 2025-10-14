// Fix Qt emit macro conflict with WebRTC sigslot
#ifdef emit
#undef emit
#define QT_NO_EMIT_DEFINED
#endif

#include "videorenderer.h"

#ifdef QT_NO_EMIT_DEFINED
#define emit
#undef QT_NO_EMIT_DEFINED
#endif

#include <QPainter>
#include <QPaintEvent>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"

VideoRenderer::VideoRenderer(QWidget* parent)
    : QWidget(parent),
      width_(0),
      height_(0) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setMinimumSize(160, 120);
  
  connect(this, &VideoRenderer::FrameReceived, this, &VideoRenderer::OnFrameReceived,
          Qt::QueuedConnection);
}

VideoRenderer::~VideoRenderer() {
  Stop();
}

void VideoRenderer::SetVideoTrack(webrtc::VideoTrackInterface* track_to_render) {
  QMutexLocker lock(&mutex_);
  
  if (rendered_track_) {
    rendered_track_->RemoveSink(this);
  }
  
  rendered_track_ = track_to_render;
  
  if (rendered_track_) {
    rendered_track_->AddOrUpdateSink(this, webrtc::VideoSinkWants());
  }
}

void VideoRenderer::Stop() {
  SetVideoTrack(nullptr);
}

void VideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame) {
  QMutexLocker lock(&mutex_);
  
  webrtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
      video_frame.video_frame_buffer()->ToI420());
  
  if (video_frame.rotation() != webrtc::kVideoRotation_0) {
    buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
  }

  int width = buffer->width();
  int height = buffer->height();

  if (width != width_ || height != height_) {
    SetSize(width, height);
  }

  if (image_.isNull()) {
    return;
  }

  // Convert I420 to ARGB
  libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),
                     buffer->DataU(), buffer->StrideU(),
                     buffer->DataV(), buffer->StrideV(),
                     image_.bits(), image_.bytesPerLine(),
                     width, height);

  // Emit signal to update UI in main thread
  emit FrameReceived();
}

void VideoRenderer::OnFrameReceived() {
  update();
}

void VideoRenderer::SetSize(int width, int height) {
  if (width == width_ && height == height_) {
    return;
  }

  width_ = width;
  height_ = height;

  image_ = QImage(width_, height_, QImage::Format_ARGB32);
}

void VideoRenderer::paintEvent(QPaintEvent* event) {
  QMutexLocker lock(&mutex_);
  
  QPainter painter(this);
  painter.fillRect(rect(), Qt::black);

  if (!image_.isNull()) {
    QRect target = rect();
    
    // Calculate aspect ratio to maintain video proportions
    double widget_aspect = static_cast<double>(target.width()) / target.height();
    double video_aspect = static_cast<double>(image_.width()) / image_.height();
    
    QRect draw_rect;
    if (widget_aspect > video_aspect) {
      // Widget is wider than video
      int draw_width = static_cast<int>(target.height() * video_aspect);
      int x_offset = (target.width() - draw_width) / 2;
      draw_rect = QRect(x_offset, 0, draw_width, target.height());
    } else {
      // Widget is taller than video
      int draw_height = static_cast<int>(target.width() / video_aspect);
      int y_offset = (target.height() - draw_height) / 2;
      draw_rect = QRect(0, y_offset, target.width(), draw_height);
    }
    
    painter.drawImage(draw_rect, image_);
  }
}
