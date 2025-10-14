# CMake 最低版本要求
cmake_minimum_required(VERSION 3.15)

# 项目名称
project(peerconnection_client)

# Qt 配置
set(CMAKE_PREFIX_PATH "d:/Qt/6.6.3/msvc2019_64") # Qt Kit Dir
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 查找 Qt6 包
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# 设置 C++ 标准为 C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 路径配置 ---
set(WEBRTC_SRC_DIR "D:/webrtc-checkout/src")
set(WEBRTC_LIB_DIR "${WEBRTC_SRC_DIR}/out/Release/obj")
set(ABSEIL_INSTALL_DIR "${WEBRTC_SRC_DIR}/third_party/abseil-cpp/build_static/install")
set(JSONCPP_INSTALL_DIR "${WEBRTC_SRC_DIR}/third_party/jsoncpp/build_static/install")

# --- 检查路径 ---
if(NOT EXISTS "${WEBRTC_SRC_DIR}")
    message(FATAL_ERROR "WebRTC source directory not found at: ${WEBRTC_SRC_DIR}")
endif()
if(NOT EXISTS "${WEBRTC_LIB_DIR}")
    message(FATAL_ERROR "WebRTC library directory not found at: ${WEBRTC_LIB_DIR}")
endif()

set(MAIN_WEBRTC_LIB_PATH "${WEBRTC_LIB_DIR}/webrtc.lib")
if(NOT EXISTS "${MAIN_WEBRTC_LIB_PATH}")
    message(FATAL_ERROR "The main library webrtc.lib was not found at: ${MAIN_WEBRTC_LIB_PATH}")
endif()

# 检查 JsonCpp 库
set(JSONCPP_LIB_PATH "${JSONCPP_INSTALL_DIR}/lib/jsoncpp.lib")
if(NOT EXISTS "${JSONCPP_LIB_PATH}")
    message(FATAL_ERROR "JsonCpp library not found at: ${JSONCPP_LIB_PATH}")
endif()

# 强制使用静态运行时库 /MT
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# 编译选项
add_compile_options(
    /Zc:__cplusplus  # 启用正确的 __cplusplus 宏
    /EHsc  # 异常处理
)

add_definitions(
    -DWIN32_LEAN_AND_MEAN 
    -DNOMINMAX
    -DWEBRTC_WIN 
    -D_CRT_SECURE_NO_WARNINGS 
    -D_SCL_SECURE_NO_WARNINGS
    -DWEBRTC_LITTLE_ENDIAN 
    -D_WIN32_WINNT=0x0A00 
    -D_HAS_ITERATOR_DEBUGGING=0
    -D_ITERATOR_DEBUG_LEVEL=0
    -DWEBRTC_LIBRARY_IMPL
    -DWEBRTC_NON_STATIC_TRACE_EVENT_HANDLERS=1
    -DUNICODE
    -D_UNICODE
)

include_directories(SYSTEM 
    ${WEBRTC_SRC_DIR}
    "${WEBRTC_SRC_DIR}/third_party/abseil-cpp"
    "${WEBRTC_SRC_DIR}/third_party/libyuv/include"
    "${JSONCPP_INSTALL_DIR}/include"
    "${WEBRTC_SRC_DIR}/api"
    "${WEBRTC_SRC_DIR}/rtc_base"
    "${WEBRTC_SRC_DIR}/media"
    "${WEBRTC_SRC_DIR}/p2p"
    "${WEBRTC_SRC_DIR}/pc"
)

link_directories(
    ${WEBRTC_LIB_DIR}
    "${ABSEIL_INSTALL_DIR}/lib"
    "${JSONCPP_INSTALL_DIR}/lib"
)

# --- 生成目标 (Windows GUI 应用) ---
add_executable(peerconnection_client WIN32
    src/main.cc
    src/conductor.cc
    src/defaults.cc
    src/mainwindow.cc
    src/videorenderer.cc
    src/peer_connection_client.cc
    src/test_impl.cc
    test/platform_video_capturer.cc
    test/vcm_capturer.cc
    test/test_video_capturer.cc
    test/frame_generator.cc
    test/frame_generator_capturer.cc
    test/frame_utils.cc
    include/mainwindow.h
    include/videorenderer.h
)
target_include_directories(peerconnection_client PRIVATE "${CMAKE_SOURCE_DIR}/include")
# WebRTC 的必需库
file(GLOB WEBRTC_EXTRA_LIBS 
    "${WEBRTC_LIB_DIR}/rtc_base/rtc_json.lib"
    "${WEBRTC_LIB_DIR}/api/field_trials.lib"
)

# Abseil 库
set(ABSEIL_LIBRARIES
    absl_flags_parse
    absl_flags_usage
    absl_flags_usage_internal
    absl_flags_config
    absl_flags_marshalling
    absl_flags_commandlineflag
    absl_flags_commandlineflag_internal
    absl_flags_private_handle_accessor
    absl_flags_reflection
    absl_flags_internal
    absl_flags_program_name
    absl_log_internal_message
    absl_log_internal_check_op
    absl_log_internal_conditions
    absl_log_internal_log_sink_set
    absl_log_internal_format
    absl_log_internal_globals
    absl_log_globals
    absl_log_entry
    absl_log_severity
    absl_log_sink
    absl_stacktrace
    absl_symbolize
    absl_debugging_internal
    absl_demangle_internal
    absl_synchronization
    absl_graphcycles_internal
    absl_kernel_timeout_internal
    absl_time
    absl_time_zone
    absl_civil_time
    absl_strings
    absl_strings_internal
    absl_str_format_internal
    absl_string_view
    absl_cord
    absl_cord_internal
    absl_cordz_functions
    absl_cordz_handle
    absl_cordz_info
    absl_base
    absl_malloc_internal
    absl_raw_logging_internal
    absl_spinlock_wait
    absl_throw_delegate
    absl_int128
    absl_city
    absl_hash
    absl_raw_hash_set
)

# Windows 系统库
set(SYSTEM_LIBRARIES
    ws2_32 
    secur32 
    winmm 
    iphlpapi 
    advapi32 
    user32 
    crypt32 
    dmoguids 
    msdmo
    strmiids
    wmcodecdspuuid
    comctl32
    gdi32
    comdlg32
    ole32
    shell32
)

# 链接所有库
target_link_libraries(peerconnection_client
    PRIVATE
    webrtc
    jsoncpp
    ${WEBRTC_EXTRA_LIBS}
    ${ABSEIL_LIBRARIES}
    ${SYSTEM_LIBRARIES}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# 设置链接选项
set_target_properties(peerconnection_client PROPERTIES
    LINK_FLAGS "/SUBSYSTEM:WINDOWS"
)