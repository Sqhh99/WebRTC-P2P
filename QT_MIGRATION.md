# WebRTC客户端 Win32到Qt迁移总结

## 项目概述
成功将WebRTC PeerConnection客户端的UI从Win32 API迁移到Qt框架,保持了原有的点对点通话功能。

## 主要修改

### 1. 新增文件

#### Qt界面文件
- `include/mainwindow.h` - Qt版本的主窗口头文件
- `src/mainwindow.cc` - Qt版本的主窗口实现
- `include/videorenderer.h` - Qt视频渲染组件头文件  
- `src/videorenderer.cc` - Qt视频渲染组件实现

### 2. 修改文件

#### CMakeLists.txt
- 添加Qt6支持(Core, Gui, Widgets模块)
- 启用CMAKE_AUTOMOC和CMAKE_AUTORCC
- 更新源文件列表,使用新的Qt文件
- 添加Qt库链接

#### src/main.cc
- 从wWinMain改为标准main入口点
- 集成QApplication
- 使用QTimer处理WebRTC消息循环
- 移除Win32消息循环代码

#### include/conductor.h 和 src/conductor.cc
- 更新include路径,使用新的mainwindow.h

### 3. 关键技术要点

#### Qt与WebRTC兼容性
WebRTC的sigslot库与Qt的emit宏存在冲突,解决方案:
```cpp
// Fix Qt emit macro conflict with WebRTC sigslot
#ifdef emit
#undef emit
#define QT_NO_EMIT_DEFINED
#endif

#include "mainwindow.h"

#ifdef QT_NO_EMIT_DEFINED
#define emit
#undef QT_NO_EMIT_DEFINED
#endif
```

#### 线程模型
- Qt使用事件循环(QApplication::exec())
- WebRTC使用Windows消息队列
- 通过QTimer定时器桥接两个事件系统

#### 视频渲染
- 使用QWidget替代Win32 GDI绘图
- 利用QMutex保护视频帧数据
- 通过Qt信号槽机制在主线程更新UI
- 使用libyuv库进行I420到ARGB格式转换

## UI功能对应

| 功能 | Win32实现 | Qt实现 |
|------|----------|--------|
| 连接界面 | 原生Windows控件 | QLineEdit, QPushButton, QLabel |
| 对等列表 | LISTBOX控件 | QListWidget |
| 视频渲染 | GDI BitBlt | QPainter绘制QImage |
| 界面切换 | ShowWindow/HideWindow | QStackedWidget |
| 消息框 | MessageBox API | QMessageBox |
| 键盘事件 | WM_CHAR消息 | keyPressEvent重载 |

## 编译说明

### 前提条件
- Qt 6.6.3 (MSVC 2019 64位)
- WebRTC库(已编译)
- CMake 3.15+
- Visual Studio 2019+

### 构建命令
```bash
cmake --build build --config Release
```

### 输出
- `build/Release/peerconnection_client.exe`

## 功能验证

所有原有功能均已保留:
- ✅ 连接到信令服务器
- ✅ 显示在线对等列表
- ✅ 建立点对点连接
- ✅ 本地视频采集和显示
- ✅ 远程视频接收和显示
- ✅ 自动连接和自动呼叫
- ✅ ESC键断开连接

## 注意事项

1. **命名空间**: WebRTC组件统一使用`webrtc::`命名空间,而非`rtc::`
2. **文件命名**: 新文件名不包含下划线(mainwindow而非main_wnd)
3. **API版本**: 基于最新WebRTC API,使用Environment、scoped_refptr等
4. **线程安全**: VideoRenderer使用QMutex保护共享数据
5. **信号槽**: 使用Qt::QueuedConnection确保跨线程信号传递安全

## 待优化项

- 本地视频窗口位置固定,可改为浮动覆盖在右下角
- 可添加更多UI控件(音量调节、设置等)
- 可以使用Qt Designer设计更复杂的界面
- 考虑添加Qt样式表美化界面

## 兼容性

- Windows 10/11
- Qt 6.x
- WebRTC M120+
- C++20标准
