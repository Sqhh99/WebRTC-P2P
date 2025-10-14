#ifndef EXAMPLES_PEERCONNECTION_CLIENT_MAINWINDOW_H_
#define EXAMPLES_PEERCONNECTION_CLIENT_MAINWINDOW_H_

#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "signalclient.h"
#include "callmanager.h"

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

class VideoRenderer;
class Conductor;

// 主窗口类：WebRTC视频通话客户端
class MainWnd : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWnd(QWidget* parent = nullptr);
  ~MainWnd() override;

  // 设置Conductor（必须在初始化后设置）
  void SetConductor(Conductor* conductor);
  
  // 视频渲染
  void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
  void StopLocalRenderer();
  void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
  void StopRemoteRenderer();
  
  // 日志输出
  void AppendLog(const QString& message, const QString& level = "info");
  
  // 显示消息框
  void ShowError(const QString& title, const QString& message);
  void ShowInfo(const QString& title, const QString& message);
  
  // 获取信令客户端和呼叫管理器
  SignalClient* GetSignalClient() { return signal_client_.get(); }
  CallManager* GetCallManager() { return call_manager_.get(); }
  
  // 更新客户端列表（公有方法，供Conductor调用）
  void UpdateClientList(const QJsonArray& clients);

 signals:
  void SignalServerUrlChanged(QString url);
  void ClientIdChanged(QString id);

 private slots:
  // 连接相关
  void OnConnectClicked();
  void OnDisconnectClicked();
  void OnSignalConnected(QString client_id);
  void OnSignalDisconnected();
  void OnSignalError(QString error);
  
  // 用户列表
  void OnClientListUpdate(const QJsonArray& clients);
  void OnUserOffline(const std::string& client_id);
  void OnUserItemDoubleClicked(QListWidgetItem* item);
  
  // 呼叫控制
  void OnCallButtonClicked();
  void OnHangupButtonClicked();
  void OnIncomingCall(QString caller_id);
  void OnCallStateChanged(CallState state, QString peer_id);
  
  // 定时更新
  void OnUpdateStatsTimer();

 protected:
  void closeEvent(QCloseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  void CreateUI();
  void CreateConnectionPanel();
  void CreateUserListPanel();
  void CreateLogPanel();
  void CreateVideoPanel();
  void CreateControlPanel();
  
  void UpdateUIState();
  void UpdateCallButtonState();
  void LayoutLocalVideo();
  
  QString GetCallStateString(CallState state) const;
  
  Conductor* conductor_;
  std::unique_ptr<SignalClient> signal_client_;
  std::unique_ptr<CallManager> call_manager_;
  
  // 连接面板
  QWidget* connection_panel_;
  QLineEdit* server_url_edit_;
  QLineEdit* client_id_edit_;
  QPushButton* connect_button_;
  QPushButton* disconnect_button_;
  QLabel* connection_status_label_;
  
  // 用户列表面板
  QGroupBox* user_list_group_;
  QListWidget* user_list_;
  QLabel* online_users_label_;
  
  // 日志面板
  QGroupBox* log_group_;
  QTextEdit* log_text_;
  
  // 视频面板
  QWidget* video_panel_;
  VideoRenderer* local_video_renderer_;
  VideoRenderer* remote_video_renderer_;
  QLabel* no_video_label_;
  
  // 控制面板
  QWidget* control_panel_;
  QPushButton* call_button_;
  QPushButton* hangup_button_;
  QLabel* call_state_label_;
  QLabel* stats_label_;
  
  // 主布局
  QSplitter* main_splitter_;
  QSplitter* right_splitter_;
  
  // 定时器
  std::unique_ptr<QTimer> stats_timer_;
  
  // 当前状态
  QString current_peer_;
  bool is_connected_;
};

#endif  // EXAMPLES_PEERCONNECTION_CLIENT_MAINWINDOW_H_
