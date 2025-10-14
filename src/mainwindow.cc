#include "mainwindow.h"
#include "videorenderer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QInputDialog>
#include <QFrame>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

MainWnd::MainWnd(QWidget* parent)
    : QMainWindow(parent),
      conductor_(nullptr),
      is_connected_(false) {
  setWindowTitle("WebRTC Video Call Client");
  resize(1200, 800);
  
  // 创建信令客户端和呼叫管理器
  signal_client_ = std::make_unique<SignalClient>(this);
  call_manager_ = std::make_unique<CallManager>(this);
  call_manager_->SetSignalClient(signal_client_.get());
  
  // 连接信号
  connect(signal_client_.get(), &SignalClient::SignalConnected, 
          this, &MainWnd::OnSignalConnected);
  connect(signal_client_.get(), &SignalClient::SignalDisconnected,
          this, &MainWnd::OnSignalDisconnected);
  connect(signal_client_.get(), &SignalClient::SignalError,
          this, &MainWnd::OnSignalError);
  
  connect(call_manager_.get(), &CallManager::CallStateChanged,
          this, &MainWnd::OnCallStateChanged);
  connect(call_manager_.get(), &CallManager::IncomingCall,
          this, &MainWnd::OnIncomingCall);
  
  // 创建UI
  CreateUI();
  UpdateUIState();
  
  // 创建统计定时器
  stats_timer_ = std::make_unique<QTimer>(this);
  connect(stats_timer_.get(), &QTimer::timeout, this, &MainWnd::OnUpdateStatsTimer);
  stats_timer_->start(1000);  // 每秒更新一次
  
  AppendLog("应用程序已启动", "info");
}

MainWnd::~MainWnd() {
  if (signal_client_) {
    signal_client_->Disconnect();
  }
}

void MainWnd::SetConductor(Conductor* conductor) {
  conductor_ = conductor;
}

void MainWnd::CreateUI() {
  QWidget* central_widget = new QWidget(this);
  setCentralWidget(central_widget);
  
  QVBoxLayout* main_layout = new QVBoxLayout(central_widget);
  main_layout->setContentsMargins(5, 5, 5, 5);
  main_layout->setSpacing(5);
  
  // 创建连接面板（顶部）
  CreateConnectionPanel();
  main_layout->addWidget(connection_panel_);
  
  // 创建主分割器（左右布局）
  main_splitter_ = new QSplitter(Qt::Horizontal, this);
  
  // 左侧：用户列表
  CreateUserListPanel();
  main_splitter_->addWidget(user_list_group_);
  
  // 右侧：视频和日志的垂直分割
  right_splitter_ = new QSplitter(Qt::Vertical, this);
  
  // 视频面板
  CreateVideoPanel();
  right_splitter_->addWidget(video_panel_);
  
  // 日志面板
  CreateLogPanel();
  right_splitter_->addWidget(log_group_);
  
  right_splitter_->setStretchFactor(0, 3);  // 视频占3份
  right_splitter_->setStretchFactor(1, 1);  // 日志占1份
  
  main_splitter_->addWidget(right_splitter_);
  main_splitter_->setStretchFactor(0, 1);  // 用户列表占1份
  main_splitter_->setStretchFactor(1, 4);  // 右侧占4份
  
  main_layout->addWidget(main_splitter_);
  
  // 创建控制面板（底部）
  CreateControlPanel();
  main_layout->addWidget(control_panel_);
}

void MainWnd::CreateConnectionPanel() {
  connection_panel_ = new QWidget(this);
  QHBoxLayout* layout = new QHBoxLayout(connection_panel_);
  layout->setContentsMargins(5, 5, 5, 5);
  
  QLabel* server_label = new QLabel("信令服务器:");
  layout->addWidget(server_label);
  
  server_url_edit_ = new QLineEdit("ws://localhost:8080/ws");
  server_url_edit_->setMinimumWidth(250);
  layout->addWidget(server_url_edit_);
  
  QLabel* id_label = new QLabel("客户端ID:");
  layout->addWidget(id_label);
  
  client_id_edit_ = new QLineEdit();
  client_id_edit_->setPlaceholderText("自动生成");
  client_id_edit_->setMaximumWidth(150);
  layout->addWidget(client_id_edit_);
  
  connect_button_ = new QPushButton("连接");
  connect_button_->setMaximumWidth(80);
  connect(connect_button_, &QPushButton::clicked, this, &MainWnd::OnConnectClicked);
  layout->addWidget(connect_button_);
  
  disconnect_button_ = new QPushButton("断开");
  disconnect_button_->setMaximumWidth(80);
  disconnect_button_->setEnabled(false);
  connect(disconnect_button_, &QPushButton::clicked, this, &MainWnd::OnDisconnectClicked);
  layout->addWidget(disconnect_button_);
  
  connection_status_label_ = new QLabel("未连接");
  connection_status_label_->setStyleSheet("QLabel { color: red; font-weight: bold; }");
  layout->addWidget(connection_status_label_);
  
  layout->addStretch();
}

void MainWnd::CreateUserListPanel() {
  user_list_group_ = new QGroupBox("在线用户", this);
  QVBoxLayout* layout = new QVBoxLayout(user_list_group_);
  
  online_users_label_ = new QLabel("在线用户: 0");
  layout->addWidget(online_users_label_);
  
  user_list_ = new QListWidget();
  user_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  connect(user_list_, &QListWidget::itemDoubleClicked,
          this, &MainWnd::OnUserItemDoubleClicked);
  layout->addWidget(user_list_);
  
  QLabel* hint_label = new QLabel("双击用户发起通话");
  hint_label->setStyleSheet("QLabel { color: gray; font-size: 10px; }");
  layout->addWidget(hint_label);
}

void MainWnd::CreateLogPanel() {
  log_group_ = new QGroupBox("日志", this);
  QVBoxLayout* layout = new QVBoxLayout(log_group_);
  
  log_text_ = new QTextEdit();
  log_text_->setReadOnly(true);
  log_text_->setMaximumHeight(150);
  layout->addWidget(log_text_);
  
  QPushButton* clear_log_btn = new QPushButton("清空日志");
  clear_log_btn->setMaximumWidth(100);
  connect(clear_log_btn, &QPushButton::clicked, log_text_, &QTextEdit::clear);
  layout->addWidget(clear_log_btn);
}

void MainWnd::CreateVideoPanel() {
  video_panel_ = new QWidget(this);
  video_panel_->setStyleSheet("QWidget { background-color: black; }");
  video_panel_->setMinimumHeight(400);
  
  QVBoxLayout* layout = new QVBoxLayout(video_panel_);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  
  // 远程视频（主视频）
  remote_video_renderer_ = new VideoRenderer(video_panel_);
  remote_video_renderer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(remote_video_renderer_);
  
  // 本地视频（小窗口，绝对定位）
  local_video_renderer_ = new VideoRenderer(video_panel_);
  local_video_renderer_->setFixedSize(240, 180);
  local_video_renderer_->raise();  // 放在最上层
  
  // 无视频提示标签
  no_video_label_ = new QLabel("等待视频连接...", video_panel_);
  no_video_label_->setAlignment(Qt::AlignCenter);
  no_video_label_->setStyleSheet("QLabel { color: white; font-size: 18px; }");
  layout->addWidget(no_video_label_);
  remote_video_renderer_->hide();
}

void MainWnd::CreateControlPanel() {
  control_panel_ = new QWidget(this);
  QHBoxLayout* layout = new QHBoxLayout(control_panel_);
  layout->setContentsMargins(5, 5, 5, 5);
  
  call_button_ = new QPushButton("呼叫");
  call_button_->setEnabled(false);
  call_button_->setMinimumHeight(40);
  call_button_->setStyleSheet("QPushButton { font-size: 14px; font-weight: bold; }");
  connect(call_button_, &QPushButton::clicked, this, &MainWnd::OnCallButtonClicked);
  layout->addWidget(call_button_);
  
  hangup_button_ = new QPushButton("挂断");
  hangup_button_->setEnabled(false);
  hangup_button_->setMinimumHeight(40);
  hangup_button_->setStyleSheet("QPushButton { font-size: 14px; font-weight: bold; background-color: #d9534f; color: white; }");
  connect(hangup_button_, &QPushButton::clicked, this, &MainWnd::OnHangupButtonClicked);
  layout->addWidget(hangup_button_);
  
  call_state_label_ = new QLabel("空闲");
  call_state_label_->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; }");
  layout->addWidget(call_state_label_);
  
  stats_label_ = new QLabel("");
  stats_label_->setStyleSheet("QLabel { color: gray; }");
  layout->addWidget(stats_label_);
  
  layout->addStretch();
}

void MainWnd::OnConnectClicked() {
  QString server_url = server_url_edit_->text().trimmed();
  if (server_url.isEmpty()) {
    ShowError("错误", "请输入信令服务器地址");
    return;
  }
  
  QString client_id = client_id_edit_->text().trimmed();
  
  AppendLog(QString("正在连接到服务器: %1").arg(server_url), "info");
  signal_client_->Connect(server_url, client_id);
  
  connect_button_->setEnabled(false);
}

void MainWnd::OnDisconnectClicked() {
  signal_client_->Disconnect();
  AppendLog("已断开连接", "info");
}

void MainWnd::OnSignalConnected(QString client_id) {
  is_connected_ = true;
  client_id_edit_->setText(client_id);
  
  connection_status_label_->setText(QString("已连接 [%1]").arg(client_id));
  connection_status_label_->setStyleSheet("QLabel { color: green; font-weight: bold; }");
  
  connect_button_->setEnabled(false);
  disconnect_button_->setEnabled(true);
  server_url_edit_->setEnabled(false);
  client_id_edit_->setEnabled(false);
  
  AppendLog(QString("已连接到服务器，客户端ID: %1").arg(client_id), "success");
  UpdateUIState();
}

void MainWnd::OnSignalDisconnected() {
  is_connected_ = false;
  
  connection_status_label_->setText("未连接");
  connection_status_label_->setStyleSheet("QLabel { color: red; font-weight: bold; }");
  
  connect_button_->setEnabled(true);
  disconnect_button_->setEnabled(false);
  server_url_edit_->setEnabled(true);
  client_id_edit_->setEnabled(true);
  
  user_list_->clear();
  online_users_label_->setText("在线用户: 0");
  
  AppendLog("已断开与服务器的连接", "warning");
  UpdateUIState();
}

void MainWnd::OnSignalError(QString error) {
  AppendLog(QString("信令错误: %1").arg(error), "error");
  ShowError("连接错误", error);
}

void MainWnd::UpdateClientList(const QJsonArray& clients) {
  // 使用 Qt 的线程安全调用
  QMetaObject::invokeMethod(this, [this, clients]() {
    OnClientListUpdate(clients);
  }, Qt::QueuedConnection);
}

void MainWnd::OnClientListUpdate(const QJsonArray& clients) {
  qDebug() << "MainWnd::OnClientListUpdate called with" << clients.size() << "clients";
  qDebug() << "Clients array:" << clients;
  
  user_list_->clear();
  QString my_id = signal_client_->GetClientId();
  
  int online_count = 0;
  for (const QJsonValue& value : clients) {
    QJsonObject client = value.toObject();
    QString client_id = client["id"].toString();
    
    qDebug() << "Processing client:" << client_id << "my_id:" << my_id;
    
    // 不显示自己
    if (client_id == my_id) {
      qDebug() << "Skipping self";
      continue;
    }
    
    QListWidgetItem* item = new QListWidgetItem(client_id);
    item->setData(Qt::UserRole, client_id);
    user_list_->addItem(item);
    online_count++;
  }
  
  online_users_label_->setText(QString("在线用户: %1").arg(online_count));
  AppendLog(QString("用户列表已更新，在线用户: %1").arg(online_count), "info");
}

void MainWnd::OnUserOffline(const std::string& client_id) {
  QString qclient_id = QString::fromStdString(client_id);
  
  // 从列表中移除
  for (int i = 0; i < user_list_->count(); ++i) {
    QListWidgetItem* item = user_list_->item(i);
    if (item->data(Qt::UserRole).toString() == qclient_id) {
      delete user_list_->takeItem(i);
      break;
    }
  }
  
  online_users_label_->setText(QString("在线用户: %1").arg(user_list_->count()));
  AppendLog(QString("用户 %1 已下线").arg(qclient_id), "warning");
  
  // 如果正在与该用户通话，结束通话
  if (current_peer_ == qclient_id && call_manager_->IsInCall()) {
    AppendLog("对方已离线，通话已结束", "warning");
    call_manager_->EndCall();
  }
}

void MainWnd::OnUserItemDoubleClicked(QListWidgetItem* item) {
  if (!item || !is_connected_) {
    return;
  }
  
  QString target_id = item->data(Qt::UserRole).toString();
  current_peer_ = target_id;
  
  AppendLog(QString("准备呼叫用户: %1").arg(target_id), "info");
  
  // 通过呼叫管理器发起呼叫
  if (call_manager_->InitiateCall(target_id)) {
    AppendLog(QString("正在呼叫 %1...").arg(target_id), "info");
  } else {
    AppendLog("呼叫失败", "error");
  }
}

void MainWnd::OnCallButtonClicked() {
  QListWidgetItem* selected = user_list_->currentItem();
  if (!selected) {
    ShowError("提示", "请先选择要呼叫的用户");
    return;
  }
  
  OnUserItemDoubleClicked(selected);
}

void MainWnd::OnHangupButtonClicked() {
  call_manager_->EndCall();
  AppendLog("通话已挂断", "info");
}

void MainWnd::OnIncomingCall(QString caller_id) {
  current_peer_ = caller_id;
  
  AppendLog(QString("收到来自 %1 的呼叫").arg(caller_id), "info");
  
  QMessageBox msg_box(this);
  msg_box.setWindowTitle("来电");
  msg_box.setText(QString("用户 %1 正在呼叫您").arg(caller_id));
  msg_box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
  msg_box.setButtonText(QMessageBox::Yes, "接听");
  msg_box.setButtonText(QMessageBox::No, "拒绝");
  
  int result = msg_box.exec();
  
  if (result == QMessageBox::Yes) {
    call_manager_->AcceptCall();
    AppendLog(QString("已接听来自 %1 的呼叫").arg(caller_id), "success");
  } else {
    call_manager_->RejectCall("用户拒绝");
    AppendLog(QString("已拒绝来自 %1 的呼叫").arg(caller_id), "info");
  }
}

void MainWnd::OnCallStateChanged(CallState state, QString peer_id) {
  call_state_label_->setText(GetCallStateString(state));
  UpdateCallButtonState();
  
  if (state == CallState::Connected) {
    no_video_label_->hide();
    remote_video_renderer_->show();
  } else if (state == CallState::Idle) {
    no_video_label_->show();
    remote_video_renderer_->hide();
    current_peer_.clear();
  }
}

void MainWnd::OnUpdateStatsTimer() {
  // 更新统计信息
  if (call_manager_->IsInCall()) {
    CallState state = call_manager_->GetCallState();
    stats_label_->setText(QString("通话状态: %1").arg(GetCallStateString(state)));
  } else {
    stats_label_->setText("");
  }
}

void MainWnd::UpdateUIState() {
  bool can_call = is_connected_ && !call_manager_->IsInCall() && user_list_->currentItem() != nullptr;
  call_button_->setEnabled(can_call);
  
  UpdateCallButtonState();
}

void MainWnd::UpdateCallButtonState() {
  bool in_call = call_manager_->IsInCall();
  hangup_button_->setEnabled(in_call);
  
  bool can_call = is_connected_ && !in_call && user_list_->currentItem() != nullptr;
  call_button_->setEnabled(can_call);
}

QString MainWnd::GetCallStateString(CallState state) const {
  switch (state) {
    case CallState::Idle: return "空闲";
    case CallState::Calling: return "呼叫中...";
    case CallState::Receiving: return "来电中...";
    case CallState::Connecting: return "连接中...";
    case CallState::Connected: return "通话中";
    case CallState::Ending: return "结束中...";
    default: return "未知";
  }
}

void MainWnd::StartLocalRenderer(webrtc::VideoTrackInterface* local_video) {
  if (local_video_renderer_) {
    local_video_renderer_->SetVideoTrack(local_video);
    local_video_renderer_->show();
    LayoutLocalVideo();
  }
}

void MainWnd::StopLocalRenderer() {
  if (local_video_renderer_) {
    local_video_renderer_->Stop();
    local_video_renderer_->hide();
  }
}

void MainWnd::StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video) {
  if (remote_video_renderer_) {
    remote_video_renderer_->SetVideoTrack(remote_video);
    remote_video_renderer_->show();
    no_video_label_->hide();
  }
}

void MainWnd::StopRemoteRenderer() {
  if (remote_video_renderer_) {
    remote_video_renderer_->Stop();
    remote_video_renderer_->hide();
    no_video_label_->show();
  }
}

void MainWnd::AppendLog(const QString& message, const QString& level) {
  QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
  QString color;
  
  if (level == "error") {
    color = "red";
  } else if (level == "warning") {
    color = "orange";
  } else if (level == "success") {
    color = "green";
  } else {
    color = "black";
  }
  
  QString html = QString("<span style='color: gray;'>[%1]</span> "
                        "<span style='color: %2;'>%3</span>")
                 .arg(timestamp, color, message);
  
  log_text_->append(html);
  
  // 自动滚动到底部
  QTextCursor cursor = log_text_->textCursor();
  cursor.movePosition(QTextCursor::End);
  log_text_->setTextCursor(cursor);
}

void MainWnd::ShowError(const QString& title, const QString& message) {
  QMessageBox::critical(this, title, message);
}

void MainWnd::ShowInfo(const QString& title, const QString& message) {
  QMessageBox::information(this, title, message);
}

void MainWnd::closeEvent(QCloseEvent* event) {
  if (call_manager_->IsInCall()) {
    call_manager_->EndCall();
  }
  
  if (signal_client_->IsConnected()) {
    signal_client_->Disconnect();
  }
  
  event->accept();
}

void MainWnd::resizeEvent(QResizeEvent* event) {
  QMainWindow::resizeEvent(event);
  LayoutLocalVideo();
}

void MainWnd::LayoutLocalVideo() {
  if (!local_video_renderer_ || !video_panel_) {
    return;
  }
  
  // 将本地视频放在右下角
  int margin = 10;
  int x = video_panel_->width() - local_video_renderer_->width() - margin;
  int y = video_panel_->height() - local_video_renderer_->height() - margin;
  
  local_video_renderer_->move(x, y);
}
