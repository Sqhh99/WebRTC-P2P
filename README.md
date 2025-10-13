# WebRTC PeerConnection Client

这是一个基于 WebRTC 的点对点连接客户端示例项目。

## 项目结构

```
peerconnection_client/
├── CMakeLists.txt          # CMake 构建配置
├── include/                # 头文件目录
│   ├── conductor.h
│   ├── defaults.h
│   ├── flag_defs.h
│   ├── main_wnd.h
│   └── peer_connection_client.h
├── src/                    # 源文件目录
│   ├── main.cc
│   ├── conductor.cc
│   ├── defaults.cc
│   ├── main_wnd.cc
│   ├── peer_connection_client.cc
│   └── test_impl.cc
├── test/                   # WebRTC 测试相关代码
│   ├── frame_generator.cc
│   ├── frame_generator_capturer.cc
│   ├── platform_video_capturer.cc
│   └── ... (其他测试文件)
├── build/                  # 构建输出目录 (已忽略)
└── linux/                  # Linux 特定代码
```

## 构建要求

- CMake 3.15+
- Visual Studio 2019+ (Windows)
- WebRTC 源码和预编译库

## 构建步骤

1. 确保 WebRTC 源码路径正确配置在 CMakeLists.txt 中
2. 创建构建目录：
   ```bash
   mkdir build
   cd build
   cmake ..
   ```
3. 构建项目：
   ```bash
   cmake --build . --config Release
   ```

## 运行

构建成功后，在 build/Release/ 目录下会生成 peerconnection_client.exe 可执行文件。

## 注意事项

- 项目依赖 WebRTC 库，需要预先编译 WebRTC
- 当前配置为 Windows 平台
- 使用静态链接以简化部署