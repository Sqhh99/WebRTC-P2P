/*
 * Implementation of missing test functions
 */

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "absl/memory/memory.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/frame_generator_interface.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "json/value.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "test/frame_generator.h"
#include "test/frame_generator_capturer.h"
#include "test/platform_video_capturer.h"
#include "test/test_video_capturer.h"
#include "test/vcm_capturer.h"

namespace webrtc {
namespace test {

// CreateVideoCapturer is already implemented in platform_video_capturer.cc
// We just need to provide a wrapper if needed, but it's already there.

// Implementation of CreateSquareFrameGenerator using SquareGenerator
std::unique_ptr<FrameGeneratorInterface> CreateSquareFrameGenerator(
    int width,
    int height,
    std::optional<FrameGeneratorInterface::OutputType> type,
    std::optional<int> num_squares) {
  return std::make_unique<SquareGenerator>(
      width, height,
      type.value_or(FrameGeneratorInterface::OutputType::kI420),
      num_squares.value_or(10));
}

}  // namespace test
}  // namespace webrtc

// JSON utility functions implementation
namespace webrtc {

bool GetStringFromJsonObject(const Json::Value& in,
                              std::string_view k,
                              std::string* out) {
  if (!in.isObject()) {
    return false;
  }
  
  std::string key(k);
  if (!in.isMember(key)) {
    return false;
  }
  
  const Json::Value& value = in[key];
  if (!value.isString()) {
    return false;
  }
  
  *out = value.asString();
  return true;
}

bool GetIntFromJsonObject(const Json::Value& in,
                          std::string_view k,
                          int* out) {
  if (!in.isObject()) {
    return false;
  }
  
  std::string key(k);
  if (!in.isMember(key)) {
    return false;
  }
  
  const Json::Value& value = in[key];
  if (!value.isInt()) {
    return false;
  }
  
  *out = value.asInt();
  return true;
}

}  // namespace webrtc
