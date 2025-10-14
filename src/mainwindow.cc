// Fix Qt emit macro conflict with WebRTC sigslot
#ifdef emit
#undef emit
#define QT_NO_EMIT_DEFINED
#endif

#include "mainwindow.h"
#include "videorenderer.h"

#ifdef QT_NO_EMIT_DEFINED
#define emit
#undef QT_NO_EMIT_DEFINED
#endif

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QApplication>

MainWnd::MainWnd(const char* server, int port, bool auto_connect, bool auto_call,
                 QWidget* parent)
    : QMainWindow(parent),
      ui_(CONNECT_TO_SERVER),
      callback_(nullptr),
      connect_widget_(nullptr),
      server_edit_(nullptr),
      port_edit_(nullptr),
      connect_button_(nullptr),
      server_label_(nullptr),
      port_label_(nullptr),
      peer_list_widget_(nullptr),
      peer_listbox_(nullptr),
      streaming_widget_(nullptr),
      local_renderer_(nullptr),
      remote_renderer_(nullptr),
      stacked_widget_(nullptr),
      server_(server),
      auto_connect_(auto_connect),
      auto_call_(auto_call),
      destroyed_(false) {
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%i", port);
  port_ = buffer;

  // Connect signal for UI thread callbacks
  connect(this, &MainWnd::UIThreadCallbackSignal, this,
          &MainWnd::HandleUIThreadCallback, Qt::QueuedConnection);
}

MainWnd::~MainWnd() {
  destroyed_ = true;
}

bool MainWnd::Create() {
  setWindowTitle("WebRTC PeerConnection Client");
  resize(800, 600);

  stacked_widget_ = new QStackedWidget(this);
  setCentralWidget(stacked_widget_);

  CreateConnectUI();
  CreatePeerListUI();
  CreateStreamingUI();

  SwitchToConnectUI();

  return true;
}

bool MainWnd::Destroy() {
  destroyed_ = true;
  close();
  return true;
}

void MainWnd::RegisterObserver(MainWndCallback* callback) {
  callback_ = callback;
}

bool MainWnd::IsWindow() {
  return !destroyed_ && isVisible();
}

void MainWnd::SwitchToConnectUI() {
  ui_ = CONNECT_TO_SERVER;
  stacked_widget_->setCurrentWidget(connect_widget_);
  server_edit_->setFocus();

  if (auto_connect_) {
    QTimer::singleShot(100, this, &MainWnd::OnConnectClicked);
  }
}

void MainWnd::SwitchToPeerList(const Peers& peers) {
  ui_ = LIST_PEERS;
  
  peer_listbox_->clear();
  peer_listbox_->addItem("List of currently connected peers:");
  
  for (const auto& peer : peers) {
    QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(peer.second));
    item->setData(Qt::UserRole, peer.first);
    peer_listbox_->addItem(item);
  }
  
  stacked_widget_->setCurrentWidget(peer_list_widget_);
  peer_listbox_->setFocus();

  if (auto_call_ && !peers.empty()) {
    // Auto-select and connect to the last peer
    QTimer::singleShot(100, [this]() {
      int count = peer_listbox_->count();
      if (count > 1) {  // Skip the header item
        QListWidgetItem* item = peer_listbox_->item(count - 1);
        peer_listbox_->setCurrentItem(item);
        OnPeerDoubleClicked(item);
      }
    });
  }
}

void MainWnd::SwitchToStreamingUI() {
  ui_ = STREAMING;
  stacked_widget_->setCurrentWidget(streaming_widget_);
}

void MainWnd::MessageBox(const char* caption, const char* text, bool is_error) {
  QMessageBox::Icon icon_type = is_error ? QMessageBox::Critical : QMessageBox::Information;
  QMessageBox msg_box;
  msg_box.setIcon(icon_type);
  msg_box.setWindowTitle(QString::fromUtf8(caption));
  msg_box.setText(QString::fromUtf8(text));
  msg_box.exec();
}

void MainWnd::StartLocalRenderer(webrtc::VideoTrackInterface* local_video) {
  if (local_renderer_) {
    local_renderer_->SetVideoTrack(local_video);
  }
}

void MainWnd::StopLocalRenderer() {
  if (local_renderer_) {
    local_renderer_->Stop();
  }
}

void MainWnd::StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video) {
  if (remote_renderer_) {
    remote_renderer_->SetVideoTrack(remote_video);
  }
}

void MainWnd::StopRemoteRenderer() {
  if (remote_renderer_) {
    remote_renderer_->Stop();
  }
}

void MainWnd::QueueUIThreadCallback(int msg_id, void* data) {
  emit UIThreadCallbackSignal(msg_id, data);
}

void MainWnd::OnConnectClicked() {
  if (!callback_) {
    return;
  }

  std::string server = server_edit_->text().toStdString();
  std::string port_str = port_edit_->text().toStdString();
  
  int port = atoi(port_str.c_str());
  if (port <= 0 || port > 65535) {
    MessageBox("Error", "Invalid port number", true);
    return;
  }

  callback_->StartLogin(server, port);
}

void MainWnd::OnPeerDoubleClicked(QListWidgetItem* item) {
  if (!callback_ || !item) {
    return;
  }

  // Skip the header item
  if (item->text() == "List of currently connected peers:") {
    return;
  }

  int peer_id = item->data(Qt::UserRole).toInt();
  if (peer_id != -1) {
    callback_->ConnectToPeer(peer_id);
  }
}

void MainWnd::HandleUIThreadCallback(int msg_id, void* data) {
  if (callback_) {
    callback_->UIThreadCallback(msg_id, data);
  }
}

void MainWnd::closeEvent(QCloseEvent* event) {
  if (callback_) {
    callback_->Close();
  }
  event->accept();
}

void MainWnd::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Escape) {
    if (callback_) {
      if (ui_ == STREAMING) {
        callback_->DisconnectFromCurrentPeer();
      } else {
        callback_->DisconnectFromServer();
      }
    }
  } else {
    QMainWindow::keyPressEvent(event);
  }
}

void MainWnd::CreateConnectUI() {
  connect_widget_ = new QWidget();
  
  QVBoxLayout* main_layout = new QVBoxLayout(connect_widget_);
  main_layout->setAlignment(Qt::AlignCenter);
  
  QGridLayout* form_layout = new QGridLayout();
  
  server_label_ = new QLabel("Server:");
  server_edit_ = new QLineEdit(QString::fromStdString(server_));
  server_edit_->setMinimumWidth(300);
  
  port_label_ = new QLabel("Port:");
  port_edit_ = new QLineEdit(QString::fromStdString(port_));
  port_edit_->setMaximumWidth(100);
  
  connect_button_ = new QPushButton("Connect");
  
  form_layout->addWidget(server_label_, 0, 0);
  form_layout->addWidget(server_edit_, 0, 1);
  form_layout->addWidget(port_label_, 1, 0);
  form_layout->addWidget(port_edit_, 1, 1);
  form_layout->addWidget(connect_button_, 2, 0, 1, 2);
  
  main_layout->addLayout(form_layout);
  main_layout->addStretch();
  
  connect(connect_button_, &QPushButton::clicked, this, &MainWnd::OnConnectClicked);
  connect(server_edit_, &QLineEdit::returnPressed, this, &MainWnd::OnConnectClicked);
  connect(port_edit_, &QLineEdit::returnPressed, this, &MainWnd::OnConnectClicked);
  
  stacked_widget_->addWidget(connect_widget_);
}

void MainWnd::CreatePeerListUI() {
  peer_list_widget_ = new QWidget();
  
  QVBoxLayout* layout = new QVBoxLayout(peer_list_widget_);
  
  peer_listbox_ = new QListWidget();
  peer_listbox_->setSelectionMode(QAbstractItemView::SingleSelection);
  
  layout->addWidget(peer_listbox_);
  
  connect(peer_listbox_, &QListWidget::itemDoubleClicked, this,
          &MainWnd::OnPeerDoubleClicked);
  
  stacked_widget_->addWidget(peer_list_widget_);
}

void MainWnd::CreateStreamingUI() {
  streaming_widget_ = new QWidget();
  streaming_widget_->setStyleSheet("background-color: black;");
  
  QVBoxLayout* layout = new QVBoxLayout(streaming_widget_);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  
  // Remote video takes the main area
  remote_renderer_ = new VideoRenderer();
  remote_renderer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(remote_renderer_, 1);
  
  // Local video is a small overlay
  local_renderer_ = new VideoRenderer();
  local_renderer_->setFixedSize(160, 120);
  local_renderer_->setParent(streaming_widget_);
  
  // Position local video in bottom-right corner
  // This will be adjusted in the layout
  
  stacked_widget_->addWidget(streaming_widget_);
}
