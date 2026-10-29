// Host-only ITransport over POSIX sockets and the core RFC 6455 codec (ws:// only).
// Used by the host tests and the headless client; inside Unreal the OneMindBus plugin
// uses the engine's WebSockets module instead (FOneMindUeWebSocketTransport).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/NatsClient.h"
#include "OneMindCore/WebSocketFraming.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace omhost {

class PosixWsTransport : public om::ITransport {
 public:
  PosixWsTransport() = default;
  ~PosixWsTransport() override;
  void SetListener(Listener* l) override;
  bool Open(const std::string& url) override;
  void Send(const std::string& bytes) override;
  void Close() override;

  /// Test hook: drop the TCP connection abruptly (simulates a network cut).
  void KillSocketForTest();

 private:
  void Run(om::WsUrl url, uint64_t gen);
  void SendFrame(om::WsOpcode op, const std::string& payload);

  std::mutex mu_;        // guards fd_ writes and listener_
  std::mutex life_mu_;   // serialises Open/Close
  Listener* listener_ = nullptr;
  int fd_ = -1;
  std::thread thread_;
  std::atomic<uint64_t> gen_{0};
  std::atomic<bool> open_{false};
};

}  // namespace omhost
