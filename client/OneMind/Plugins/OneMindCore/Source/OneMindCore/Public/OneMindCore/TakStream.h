// The OneMind client as a TAK contact itself (feature C3).
//
// TakStreamClient speaks the TAK Server streaming protocol on a byte transport (TLS on
// 8089 with the client certificate in production; plain TCP in local tests), from atak-civ
// takproto/README.txt "Streaming Connection Protocol Negotiation" (tak-protocols.md S1):
//   XML first (events split on </event>, each sent with an XML declaration);
//   on the server's t-x-takp-v offer of version 1, send t-x-takp-q and stop sending XML;
//   on t-x-takp-r status="true" switch both directions to v1 streaming frames
//   (0xbf, varint length, TakMessage); on "false" stay on XML; with no answer within the
//   timeout disconnect and stay on XML in later sessions (README step 6 "SHALL disconnect").
// Keep-alive follows Common Commo's constants (tak-protocols.md S5): ping (t-x-c-t, uid
// "<uid>-ping") after 15 s without receiving anything, every 4.5 s, reset at 25 s; reconnect
// with exponential backoff. The client's own SA (its contact card) is sent on every connect
// and every `sa_interval_ms`.
//
// TakDirect sends one CoT as a TAK "directed" message (connect, send, close) to a contact's
// host:port:tcp endpoint, which is how the omos mesh gateway (PR #551, DIRECT_TCP_PORT)
// and ATAK receive point-to-point CoT.
//
// This is the one place the client speaks TAK itself; everything it receives here is shown
// locally (contacts, chat) and is never republished to the bus (the bus is the authority;
// tak-client and the entity service own TAK-derived entities).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/CoT.h"
#include "OneMindCore/NatsClient.h"  // om::ITransport

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace om {

struct OM_CORE_API TakSelf {
  std::string uid;  // stable per installation, e.g. "ONEMIND-" + device id
  std::string callsign = "ONEMIND";
  std::string cot_type = "a-f-G-U-C";  // the type ATAK uses for its own position
  std::string endpoint = "*:-1:stcp";  // "*:-1:stcp" on a server stream; "ip:port:tcp" on the mesh
  std::string group_name = "Cyan";     // TAK team colour
  std::string group_role = "Team Member";
  std::string device = "Mac";
  std::string platform = "OneMind";
  std::string os = "macOS";
  std::string version = "0.4.0";
  GeoPoint point{0, 0, 9999999.0};
  double course = -1, speed = -1;
  int battery = -1;
  int stale_s = 120;
  bool simulated = true;  // run rule: everything carries opex="s"
};

/// The client's own SA (PLI), the CoT that puts it in other users' contact lists.
OM_CORE_API CotEvent BuildSelfSa(const TakSelf& self, TimeMs now);

struct OM_CORE_API TakStreamOptions {
  std::string url;          // "tls://host:8089" (production) or "tcp://host:port" (tests)
  std::string protocol = "auto";  // "auto": accept a v1 offer; "xml": never negotiate
  int ping_idle_ms = 15000;
  int ping_repeat_ms = 4500;
  int dead_ms = 25000;
  int negotiation_timeout_ms = 60000;
  int reconnect_min_ms = 1000;
  int reconnect_max_ms = 30000;
  int sa_interval_ms = 30000;   // 0 = only on connect
  size_t max_event_bytes = 1u << 20;
  size_t max_queue = 1024;      // outgoing events held while disconnected or negotiating
  int max_queue_age_ms = 30000; // older queued events are dropped, never sent late
};

class OM_CORE_API TakStreamClient : public ITransport::Listener {
 public:
  enum class Mode { Xml, Negotiating, Proto };
  enum class State { Idle, Connecting, Connected, Disconnected, Stopped };
  using EventHandler = std::function<void(const std::string& xml, const CotInfo& info)>;
  using StateHandler = std::function<void(State, const std::string& reason)>;

  TakStreamClient(std::shared_ptr<ITransport> transport, TakStreamOptions opts, TakSelf self);
  ~TakStreamClient() override;
  TakStreamClient(const TakStreamClient&) = delete;
  TakStreamClient& operator=(const TakStreamClient&) = delete;

  void Start(TimeMs now);
  void Stop();
  /// Processes transport input, negotiation, pings, SA and reconnects. One thread.
  void Poll(TimeMs now);

  /// Sends one CoT (validated with ValidateOutgoingCot). Queued while disconnected or
  /// negotiating. Returns "" or the reason it was refused.
  std::string Send(const CotEvent& ev);
  void UpdateSelf(const TakSelf& self, bool send_now);
  const TakSelf& self() const { return self_; }

  void OnEvent(EventHandler h) { on_event_ = std::move(h); }
  void OnState(StateHandler h) { on_state_ = std::move(h); }

  State state() const { return state_; }
  Mode mode() const { return mode_; }
  bool connected() const { return state_ == State::Connected; }
  const std::string& server_version() const { return server_version_; }
  const std::string& last_error() const { return last_error_; }

  struct Stats {
    uint64_t events_in = 0, events_out = 0, pings_out = 0, connects = 0, disconnects = 0;
    uint64_t negotiations = 0, negotiated_v1 = 0, declined = 0, protocol_errors = 0;
    uint64_t queue_dropped = 0, control_in = 0;
  };
  const Stats& stats() const { return stats_; }

  // ITransport::Listener (any thread; enqueue only)
  void OnTransportOpen() override;
  void OnTransportData(const char* data, size_t len) override;
  void OnTransportClosed(const std::string& reason) override;

 private:
  struct Ev {
    enum class Kind { Open, Data, Closed } kind;
    std::string bytes;
  };
  struct Queued {
    std::string xml;
    TimeMs at;
  };
  void HandleOpen(TimeMs now);
  void HandleClosed(const std::string& reason, TimeMs now);
  void Parse(TimeMs now);
  void HandleEventXml(const std::string& xml, TimeMs now);
  void Write(const std::string& xml);
  void Flush(TimeMs now);
  void SendSelf(TimeMs now);
  void Fail(const std::string& why, TimeMs now);
  void SetState(State s, const std::string& reason);
  std::string ControlXml(const std::string& type, const std::string& inner, TimeMs now) const;

  std::shared_ptr<ITransport> transport_;
  TakStreamOptions opts_;
  TakSelf self_;
  State state_ = State::Idle;
  Mode mode_ = Mode::Xml;
  bool xml_only_ = false;   // a negotiation timed out: later sessions stay on XML
  bool declined_ = false;   // the server said false in this session
  std::string rx_;
  std::mutex mu_;
  std::deque<Ev> events_;
  std::deque<Queued> queue_;
  TimeMs now_ = 0, last_rx_ = 0, last_ping_ = 0, last_sa_ = 0, neg_deadline_ = 0, next_attempt_ = 0;
  int attempt_ = 0;
  std::string server_version_, last_error_;
  EventHandler on_event_;
  StateHandler on_state_;
  Stats stats_;
};

/// One-shot directed sends (connect, send one message, close).
class OM_CORE_API TakDirect {
 public:
  using TransportFactory = std::function<std::shared_ptr<ITransport>()>;
  using Result = std::function<void(const std::string& error)>;  // "" = sent
  explicit TakDirect(TransportFactory factory) : factory_(std::move(factory)) {}
  ~TakDirect();
  /// `endpoint` is a TAK contact endpoint "host:port:tcp" (or "host:port"); `v1` sends a
  /// TAK Protocol v1 mesh frame instead of XML.
  void Send(const std::string& endpoint, const CotEvent& ev, bool v1, Result cb, TimeMs now, int timeout_ms = 5000);
  void Poll(TimeMs now);
  size_t pending() const { return jobs_.size(); }

 private:
  struct Job;
  TransportFactory factory_;
  std::vector<std::shared_ptr<Job>> jobs_;
};

/// The bytes a directed message carries (XML with declaration, or a v1 mesh frame).
OM_CORE_API bool EncodeDirected(const std::string& xml, bool v1, std::string& out, std::string* err = nullptr);
/// "host:port:tcp" or "host:port" -> "tcp://host:port". "" if not a TCP endpoint.
OM_CORE_API std::string DirectedUrl(const std::string& endpoint);

}  // namespace om
