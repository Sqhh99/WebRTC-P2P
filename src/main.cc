/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// clang-format off
// Windows headers must be included in specific order
#include <windows.h>
#include <shellapi.h>
// clang-format on

#include <memory>

// WebRTC headers
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/win32_socket_init.h"

// Application headers
#include "call_coordinator.h"
#include "video_call_window.h"

// Qt headers
#include <QApplication>
#include <QTimer>

/**
 * @brief Main entry point for the WebRTC Video Call Client
 * 
 * This application demonstrates a clean architecture using:
 * - CallCoordinator: Business logic and WebRTC coordination
 * - VideoCallWindow: Qt-based UI layer
 * - Observer pattern: Decoupling between layers
 * 
 * @return int Exit code (0 for success, -1 for failure)
 */
int main(int argc, char* argv[]) {
  // ============================================================================
  // 1. Initialize WebRTC infrastructure
  // ============================================================================
  
  // Initialize Winsock for network operations
  webrtc::WinsockInitializer winsock_init;
  
  // Create socket server for WebRTC networking
  webrtc::PhysicalSocketServer socket_server;
  webrtc::AutoSocketServerThread main_thread(&socket_server);
  
  // Create WebRTC environment
  webrtc::Environment env = webrtc::CreateEnvironment();
  
  // Initialize SSL/TLS support
  webrtc::InitializeSSL();

  // ============================================================================
  // 2. Initialize Qt Application
  // ============================================================================
  
  QApplication app(argc, argv);
  app.setApplicationName("WebRTC Video Call Client");
  app.setOrganizationName("NetherLink");

  // ============================================================================
  // 3. Create and initialize business coordinator
  // ============================================================================
  
  auto coordinator = std::make_unique<CallCoordinator>(env);
  
  if (!coordinator->Initialize()) {
    qCritical() << "Failed to initialize CallCoordinator";
    webrtc::CleanupSSL();
    return -1;
  }

  // ============================================================================
  // 4. Create and setup UI window
  // ============================================================================
  
  VideoCallWindow main_window(coordinator.get());
  coordinator->SetUIObserver(&main_window);
  main_window.show();

  // ============================================================================
  // 5. Setup Windows message processing timer
  // ============================================================================
  
  // Qt and WebRTC use different event loops, so we need to process
  // Windows messages periodically to keep WebRTC responsive
  QTimer message_timer;
  QObject::connect(&message_timer, &QTimer::timeout, []() {
    MSG msg;
    while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  });
  message_timer.start(10);  // Process every 10ms

  // ============================================================================
  // 6. Run application event loop
  // ============================================================================
  
  int result = app.exec();

  // ============================================================================
  // 7. Cleanup resources
  // ============================================================================
  
  message_timer.stop();
  
  // Shutdown coordinator and release WebRTC resources
  coordinator->Shutdown();
  coordinator.reset();

  // Cleanup WebRTC SSL
  webrtc::CleanupSSL();
  
  return result;
}
