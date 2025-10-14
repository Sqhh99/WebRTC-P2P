#ifndef EXAMPLES_PEERCONNECTION_CLIENT_VIDEORENDERER_H_
#define EXAMPLES_PEERCONNECTION_CLIENT_VIDEORENDERER_H_

#include <memory>
#include <mutex>

#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

#include <QWidget>
#include <QImage>
#include <QMutex>

class VideoRenderer : public QWidget,
                      public webrtc::VideoSinkInterface<webrtc::VideoFrame> {
  Q_OBJECT

 public:
  VideoRenderer(QWidget* parent = nullptr);
  ~VideoRenderer() override;

  void SetVideoTrack(webrtc::VideoTrackInterface* track_to_render);
  void Stop();

  // webrtc::VideoSinkInterface implementation
  void OnFrame(const webrtc::VideoFrame& frame) override;

 signals:
  void FrameReceived();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private slots:
  void OnFrameReceived();

 private:
  void SetSize(int width, int height);

  QMutex mutex_;
  QImage image_;
  webrtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
  int width_;
  int height_;
};

#endif  // EXAMPLES_PEERCONNECTION_CLIENT_VIDEORENDERER_H_
