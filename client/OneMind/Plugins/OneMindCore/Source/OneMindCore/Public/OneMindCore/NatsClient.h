// Transport-agnostic NATS client for the OneMind client (feature B3, spike S5 fallback:
// NATS over WebSocket from C++). Core subscribe, publish (with headers), request/reply,
// request-many, reconnect with resubscribe, client pings.
//
// Threading: transports may call the ITransport::Listener methods from any thread; those
// calls only enqueue. Everything else (parsing, callbacks, timers) runs inside Poll(),
// which the owner calls from one thread (the Unreal game thread, or the test main loop).
// Handlers may call back into the client.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Common.h"
#include "OneMindCore/Json.h"
#include "OneMindCore/NatsProtocol.h"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace om {

/// A byte-stream transport that carries NATS protocol bytes (WebSocket in practice).
class OM_CORE_API ITransport {
 public:
  class Listener {
   public:
    virtual ~Listener() = default;
    virtual void OnTransportOpen() = 0;
    virtual void OnTransportData(const char* data, size_t len) = 0;
    virtual void OnTransportClosed(const std::string& reason) = 0;
  };
  virtual ~ITransport() = default;
  virtual void SetListener(Listener* listener) = 0;
  /// Starts connecting; the outcome arrives through the listener. Returns false if the
  /// URL is unusable.
  virtual bool Open(const std::string& url) = 0;
  /// Sends bytes (one WebSocket binary message). Must be safe to call while open.
  virtual void Send(const std::string& bytes) = 0;
  /// Closes the connection. After Close returns, the transport makes no further
  /// listener calls for that connection.
  virtual void Close() = 0;
};

struct OM_CORE_API NatsOptions {
  std::vector<std::string> urls{"ws://127.0.0.1:9222"};
  std::string name = "onemind-client";
  /// Credentials come from the OS keychain or environment at run time, never from Git.
  std::string user;
  std::string password;
  std::string token;
  int connect_timeout_ms = 5000;
  int ping_interval_ms = 20000;
  int max_pings_out = 2;
  int reconnect_wait_min_ms = 250;
  int reconnect_wait_max_ms = 5000;
  /// Publishes made while disconnected are buffered up to this many bytes, then dropped.
  size_t reconnect_buffer_bytes = 4u * 1024u * 1024u;
  int default_request_timeout_ms = 5000;
  /// Cap on bytes received but not yet dispatched (transport queue plus parsed messages
  /// held back by Poll's max_msgs budget). Past it the client acts as a slow consumer:
  /// it closes the connection and reconnects, and layers re-snapshot (their reconnect
  /// path). Protects memory when the game thread stalls while a large layer streams.
  size_t max_pending_bytes = 64u * 1024u * 1024u;
};

enum class ConnState { Idle, Connecting, Connected, Disconnected, Closed };

class OM_CORE_API NatsClient : public ITransport::Listener {
 public:
  using MsgHandler = std::function<void(const NatsMsg&)>;
  /// Called with a reply message, or with msg == nullptr and an error
  /// ("timeout", "no_responders", "disconnected", "cancelled", "bad_request").
  /// Return true when no further replies are wanted (request-many terminator).
  using ReplyHandler = std::function<bool(const NatsMsg* msg, const std::string& error)>;
  enum class ConnEvent { Connected, Reconnected, Disconnected };
  using ConnHandler = std::function<void(ConnEvent)>;

  NatsClient(std::shared_ptr<ITransport> transport, NatsOptions opts);
  ~NatsClient() override;
  NatsClient(const NatsClient&) = delete;
  NatsClient& operator=(const NatsClient&) = delete;

  /// Begins connecting (and keeps reconnecting until Stop).
  void Start(TimeMs now);
  void Stop();

  /// Processes transport input, timers and callbacks. `max_msgs` bounds message
  /// callbacks per call (frame budget); the rest stay queued for the next call.
  size_t Poll(TimeMs now, size_t max_msgs = 100000);

  ConnState state() const { return state_; }
  bool connected() const { return state_ == ConnState::Connected; }
  const Json& server_info() const { return server_info_; }
  const std::string& last_error() const { return last_error_; }

  int64_t Subscribe(const std::string& subject, MsgHandler handler, const std::string& queue = std::string());
  void Unsubscribe(int64_t sid);
  size_t subscription_count() const { return subs_.size(); }
  bool HasSubscription(const std::string& subject) const;

  bool Publish(const std::string& subject, const std::string& data, const std::string& reply = std::string(),
               const NatsHeaders* headers = nullptr);

  /// Request with a single reply (many=false) or a stream of replies (many=true) that
  /// ends when the handler returns true. For many=true, `timeout_ms` is an idle timeout
  /// that restarts on every reply.
  uint64_t Request(const std::string& subject, const std::string& data, ReplyHandler handler,
                   int timeout_ms = 0, bool many = false, const NatsHeaders* headers = nullptr);
  void CancelRequest(uint64_t id);

  /// A fresh, unique inbox subject (`_INBOX.<token>`).
  std::string NewInbox() const;

  void AddConnHandler(ConnHandler h) { conn_handlers_.push_back(std::move(h)); }

  struct Stats {
    uint64_t msgs_in = 0, bytes_in = 0, msgs_out = 0, bytes_out = 0;
    uint64_t connects = 0, reconnects = 0, disconnects = 0, dropped_while_disconnected = 0;
    uint64_t protocol_errors = 0;
    uint64_t slow_consumer_closes = 0;  // connections closed for max_pending_bytes
  };
  const Stats& stats() const { return stats_; }

  // ITransport::Listener (any thread; enqueue only)
  void OnTransportOpen() override;
  void OnTransportData(const char* data, size_t len) override;
  void OnTransportClosed(const std::string& reason) override;

 private:
  struct Sub {
    std::string subject;
    std::string queue;
    MsgHandler handler;
  };
  struct Pending {
    ReplyHandler handler;
    bool many = false;
    int timeout_ms = 0;
    TimeMs deadline = 0;
  };
  struct Event {
    enum class Kind { Open, Data, Closed } kind;
    std::string bytes;
  };

  void Send(const std::string& bytes);
  void HandleOp(NatsOp& op, TimeMs now);
  void DispatchMsg(const NatsMsg& m);
  void OnOpen(TimeMs now);
  void OnClosed(const std::string& reason, TimeMs now);
  void ScheduleReconnect(TimeMs now);
  void TryConnect(TimeMs now);
  void EnsureRespMux();
  void FailAllRequests(const std::string& why);
  void Emit(ConnEvent e);
  void ClearOps();
  void ClearEventsLocked();
  void SlowConsumerClose(TimeMs now);
  static size_t OpBytes(const NatsOp& op);

  std::shared_ptr<ITransport> transport_;
  NatsOptions opts_;
  ConnState state_ = ConnState::Idle;
  bool got_info_ = false;
  bool ever_connected_ = false;
  Json server_info_;
  std::string last_error_;
  NatsParser parser_;
  std::deque<NatsOp> ops_;
  size_t ops_bytes_ = 0;  // payload + header bytes held in ops_

  std::mutex ev_mu_;
  std::deque<Event> events_;
  size_t events_bytes_ = 0;      // guarded by ev_mu_
  bool events_overflow_ = false;  // guarded by ev_mu_

  std::map<int64_t, Sub> subs_;
  int64_t next_sid_ = 1;
  std::string resp_prefix_;
  int64_t resp_sid_ = 0;
  std::map<std::string, Pending> pending_;
  std::map<uint64_t, std::string> req_ids_;
  uint64_t next_req_ = 1;

  std::string reconnect_buf_;
  TimeMs now_ = 0;
  TimeMs next_attempt_ = 0;
  TimeMs connect_deadline_ = 0;
  TimeMs next_ping_ = 0;
  int pings_out_ = 0;
  int attempt_ = 0;
  size_t url_index_ = 0;
  std::vector<ConnHandler> conn_handlers_;
  Stats stats_;
};

}  // namespace om
