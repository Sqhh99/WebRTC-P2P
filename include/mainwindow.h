#ifndef EXAMPLES_PEERCONNECTION_CLIENT_MAINWINDOW_H_
#define EXAMPLES_PEERCONNECTION_CLIENT_MAINWINDOW_H_

#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "peer_connection_client.h"

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

class VideoRenderer;

class MainWndCallback {
 public:
  virtual void StartLogin(const std::string& server, int port) = 0;
  virtual void DisconnectFromServer() = 0;
  virtual void ConnectToPeer(int peer_id) = 0;
  virtual void DisconnectFromCurrentPeer() = 0;
  virtual void UIThreadCallback(int msg_id, void* data) = 0;
  virtual void Close() = 0;

 protected:
  virtual ~MainWndCallback() {}
};

// Pure virtual interface for the main window.
class MainWindow {
 public:
  virtual ~MainWindow() {}

  enum UI {
    CONNECT_TO_SERVER,
    LIST_PEERS,
    STREAMING,
  };

  virtual void RegisterObserver(MainWndCallback* callback) = 0;

  virtual bool IsWindow() = 0;
  virtual void MessageBox(const char* caption,
                          const char* text,
                          bool is_error) = 0;

  virtual UI current_ui() = 0;

  virtual void SwitchToConnectUI() = 0;
  virtual void SwitchToPeerList(const Peers& peers) = 0;
  virtual void SwitchToStreamingUI() = 0;

  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video) = 0;
  virtual void StopLocalRenderer() = 0;
  virtual void StartRemoteRenderer(
      webrtc::VideoTrackInterface* remote_video) = 0;
  virtual void StopRemoteRenderer() = 0;

  virtual void QueueUIThreadCallback(int msg_id, void* data) = 0;
};

class MainWnd : public QMainWindow, public MainWindow {
  Q_OBJECT

 public:
  MainWnd(const char* server, int port, bool auto_connect, bool auto_call,
          QWidget* parent = nullptr);
  ~MainWnd() override;

  bool Create();
  bool Destroy();

  // MainWindow interface implementation
  void RegisterObserver(MainWndCallback* callback) override;
  bool IsWindow() override;
  void SwitchToConnectUI() override;
  void SwitchToPeerList(const Peers& peers) override;
  void SwitchToStreamingUI() override;
  void MessageBox(const char* caption, const char* text, bool is_error) override;
  UI current_ui() override { return ui_; }

  void StartLocalRenderer(webrtc::VideoTrackInterface* local_video) override;
  void StopLocalRenderer() override;
  void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video) override;
  void StopRemoteRenderer() override;

  void QueueUIThreadCallback(int msg_id, void* data) override;

 signals:
  void UIThreadCallbackSignal(int msg_id, void* data);

 private slots:
  void OnConnectClicked();
  void OnPeerDoubleClicked(QListWidgetItem* item);
  void HandleUIThreadCallback(int msg_id, void* data);

 protected:
  void closeEvent(QCloseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void CreateConnectUI();
  void CreatePeerListUI();
  void CreateStreamingUI();

  UI ui_;
  MainWndCallback* callback_;
  
  // Connect UI widgets
  QWidget* connect_widget_;
  QLineEdit* server_edit_;
  QLineEdit* port_edit_;
  QPushButton* connect_button_;
  QLabel* server_label_;
  QLabel* port_label_;
  
  // Peer list UI widgets
  QWidget* peer_list_widget_;
  QListWidget* peer_listbox_;
  
  // Streaming UI widgets
  QWidget* streaming_widget_;
  VideoRenderer* local_renderer_;
  VideoRenderer* remote_renderer_;
  
  QStackedWidget* stacked_widget_;
  
  std::string server_;
  std::string port_;
  bool auto_connect_;
  bool auto_call_;
  bool destroyed_;
};

#endif  // EXAMPLES_PEERCONNECTION_CLIENT_MAINWINDOW_H_
