// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/NatsClient.h"

#include "OneMindCore/Codec.h"

#include <algorithm>
#include <cctype>

namespace om {

NatsClient::NatsClient(std::shared_ptr<ITransport> transport, NatsOptions opts)
    : transport_(std::move(transport)), opts_(std::move(opts)) {
  if (opts_.urls.empty()) opts_.urls.push_back("ws://127.0.0.1:9222");
  transport_->SetListener(this);
}

NatsClient::~NatsClient() {
  // No callbacks from the destructor: handlers may capture objects that are already gone
  // (found by AddressSanitizer in nats_client_reconnect_resubscribes_and_flushes_buffer).
  conn_handlers_.clear();
  pending_.clear();
  req_ids_.clear();
  Stop();
  transport_->SetListener(nullptr);
}

void NatsClient::OnTransportOpen() {
  std::lock_guard<std::mutex> lock(ev_mu_);
  events_.push_back(Event{Event::Kind::Open, std::string()});
}

void NatsClient::OnTransportData(const char* data, size_t len) {
  std::lock_guard<std::mutex> lock(ev_mu_);
  // Past the cap, stop queueing: Poll closes this connection as a slow consumer. Data
  // after the dropped chunk is useless (the byte stream has a hole), so drop it all.
  if (events_overflow_) return;
  if (events_bytes_ + len > opts_.max_pending_bytes) {
    events_overflow_ = true;
    return;
  }
  events_bytes_ += len;
  events_.push_back(Event{Event::Kind::Data, std::string(data, len)});
}

void NatsClient::ClearEventsLocked() {
  events_.clear();
  events_bytes_ = 0;
  events_overflow_ = false;
}

void NatsClient::ClearOps() {
  ops_.clear();
  ops_bytes_ = 0;
}

size_t NatsClient::OpBytes(const NatsOp& op) {
  size_t n = op.text.size() + op.msg.data.size() + op.msg.subject.size() + op.msg.reply.size();
  for (const auto& kv : op.msg.headers.kv) n += kv.first.size() + kv.second.size();
  return n;
}

void NatsClient::SlowConsumerClose(TimeMs now) {
  stats_.slow_consumer_closes++;
  OnClosed("slow consumer: more than " + std::to_string(opts_.max_pending_bytes) +
               " bytes received and not yet processed",
           now);
}

void NatsClient::OnTransportClosed(const std::string& reason) {
  std::lock_guard<std::mutex> lock(ev_mu_);
  events_.push_back(Event{Event::Kind::Closed, reason});
}

void NatsClient::Start(TimeMs now) {
  now_ = now;
  if (state_ == ConnState::Connecting || state_ == ConnState::Connected) return;
  state_ = ConnState::Disconnected;
  next_attempt_ = now;
  TryConnect(now);
}

void NatsClient::Stop() {
  if (state_ == ConnState::Closed || state_ == ConnState::Idle) return;
  transport_->Close();
  {
    std::lock_guard<std::mutex> lock(ev_mu_);
    ClearEventsLocked();
  }
  bool was = state_ == ConnState::Connected;
  state_ = ConnState::Closed;
  FailAllRequests("closed");
  if (was) Emit(ConnEvent::Disconnected);
}

void NatsClient::TryConnect(TimeMs now) {
  const std::string& url = opts_.urls[url_index_ % opts_.urls.size()];
  url_index_++;
  attempt_++;
  got_info_ = false;
  parser_.Reset();
  ClearOps();
  state_ = ConnState::Connecting;
  connect_deadline_ = now + opts_.connect_timeout_ms;
  if (!transport_->Open(url)) {
    last_error_ = "bad url: " + url;
    state_ = ConnState::Disconnected;
    ScheduleReconnect(now);
  }
}

void NatsClient::ScheduleReconnect(TimeMs now) {
  // Exponential backoff with a cap and +/-25% jitter derived from the token PRNG.
  int64_t wait = opts_.reconnect_wait_min_ms;
  for (int i = 1; i < attempt_ && wait < opts_.reconnect_wait_max_ms; ++i) wait *= 2;
  wait = (std::min<int64_t>)(wait, opts_.reconnect_wait_max_ms);
  std::string t = NewToken();
  int64_t jitter = (static_cast<unsigned char>(t[0]) % 51) - 25;  // -25..25 %
  wait += wait * jitter / 100;
  next_attempt_ = now + (std::max<int64_t>)(wait, 50);
}

void NatsClient::Send(const std::string& bytes) {
  stats_.bytes_out += bytes.size();
  transport_->Send(bytes);
}

void NatsClient::OnOpen(TimeMs now) {
  // Wait for INFO; the CONNECT goes out when it arrives (HandleOp).
  connect_deadline_ = now + opts_.connect_timeout_ms;
}

void NatsClient::OnClosed(const std::string& reason, TimeMs now) {
  bool was_connected = state_ == ConnState::Connected;
  if (state_ == ConnState::Closed) return;
  last_error_ = reason.empty() ? last_error_ : reason;
  transport_->Close();
  {
    // Events still queued belong to the connection that just ended.
    std::lock_guard<std::mutex> lock(ev_mu_);
    ClearEventsLocked();
  }
  state_ = ConnState::Disconnected;
  pings_out_ = 0;
  if (was_connected) {
    stats_.disconnects++;
    attempt_ = 0;  // first retry is quick after a working connection drops
    // In-flight requests cannot be answered on a new connection's inbox state; fail them
    // so callers can retry. Subscriptions are kept and resent after reconnect.
    FailAllRequests("disconnected");
    Emit(ConnEvent::Disconnected);
  }
  ScheduleReconnect(now);
}

void NatsClient::EnsureRespMux() {
  if (resp_sid_ != 0) return;
  resp_prefix_ = "_INBOX." + NewToken() + ".";
  resp_sid_ = next_sid_++;
  subs_[resp_sid_] = Sub{resp_prefix_ + "*", std::string(), nullptr};
  if (state_ == ConnState::Connected) Send(nats_proto::Sub(resp_prefix_ + "*", std::string(), resp_sid_));
}

void NatsClient::HandleOp(NatsOp& op, TimeMs now) {
  switch (op.kind) {
    case NatsOp::Kind::Info: {
      Json info;
      if (Json::Parse(op.text, info)) server_info_ = info;
      if (state_ == ConnState::Connecting && !got_info_) {
        got_info_ = true;
        Json c = Json::MakeObject();
        c.set("verbose", false);
        c.set("pedantic", false);
        c.set("tls_required", false);
        c.set("name", opts_.name);
        c.set("lang", "cpp-onemind");
        c.set("version", "0.2.0");
        c.set("protocol", 1);
        c.set("headers", true);
        c.set("no_responders", true);
        if (!opts_.user.empty()) {
          c.set("user", opts_.user);
          c.set("pass", opts_.password);
        }
        if (!opts_.token.empty()) c.set("auth_token", opts_.token);
        Send(nats_proto::Connect(c.Dump()));
        // Resubscribe everything, then flush what was published while disconnected.
        for (const auto& kv : subs_) Send(nats_proto::Sub(kv.second.subject, kv.second.queue, kv.first));
        Send(nats_proto::Ping());
        pings_out_ = 1;
        if (!reconnect_buf_.empty()) {
          Send(reconnect_buf_);
          reconnect_buf_.clear();
        }
        state_ = ConnState::Connected;
        attempt_ = 0;
        stats_.connects++;
        next_ping_ = now + opts_.ping_interval_ms;
        if (ever_connected_) {
          stats_.reconnects++;
          Emit(ConnEvent::Reconnected);
        } else {
          ever_connected_ = true;
          Emit(ConnEvent::Connected);
        }
      }
      break;
    }
    case NatsOp::Kind::Ping:
      Send(nats_proto::Pong());
      break;
    case NatsOp::Kind::Pong:
      pings_out_ = 0;
      break;
    case NatsOp::Kind::Ok:
      break;
    case NatsOp::Kind::Err: {
      last_error_ = op.text;
      std::string lower = op.text;
      for (auto& ch : lower) ch = static_cast<char>(::tolower(static_cast<unsigned char>(ch)));
      // Permission violations keep the connection; everything else is fatal per protocol.
      if (lower.find("permissions violation") == std::string::npos) OnClosed("server error: " + op.text, now);
      break;
    }
    case NatsOp::Kind::Msg:
      stats_.msgs_in++;
      DispatchMsg(op.msg);
      break;
  }
}

void NatsClient::DispatchMsg(const NatsMsg& m) {
  if (m.sid == resp_sid_ && resp_sid_ != 0) {
    std::string token = m.subject.substr((std::min)(m.subject.size(), resp_prefix_.size()));
    auto it = pending_.find(token);
    if (it == pending_.end()) return;
    ReplyHandler h = it->second.handler;
    bool many = it->second.many;
    bool done;
    if (m.headers.status == 503 && m.data.empty()) {
      done = true;
      h(nullptr, "no_responders");
    } else {
      done = h(&m, std::string());
    }
    it = pending_.find(token);  // the handler may have cancelled or added requests
    if (it == pending_.end()) return;
    if (!many || done) {
      pending_.erase(it);
      for (auto r = req_ids_.begin(); r != req_ids_.end(); ++r) {
        if (r->second == token) {
          req_ids_.erase(r);
          break;
        }
      }
    } else {
      it->second.deadline = now_ + it->second.timeout_ms;
    }
    return;
  }
  auto it = subs_.find(m.sid);
  if (it == subs_.end() || !it->second.handler) return;
  MsgHandler h = it->second.handler;
  h(m);
}

size_t NatsClient::Poll(TimeMs now, size_t max_msgs) {
  now_ = now;
  std::deque<Event> evs;
  bool overflow = false;
  {
    std::lock_guard<std::mutex> lock(ev_mu_);
    evs.swap(events_);
    events_bytes_ = 0;
    overflow = events_overflow_;
    events_overflow_ = false;
  }
  if (overflow && (state_ == ConnState::Connecting || state_ == ConnState::Connected)) {
    // The transport queue hit the cap and dropped data: this connection's stream is
    // broken. Everything queued belongs to it.
    evs.clear();
    SlowConsumerClose(now);
    ClearOps();
  }
  for (auto& e : evs) {
    if (state_ == ConnState::Closed) break;
    switch (e.kind) {
      case Event::Kind::Open:
        OnOpen(now);
        break;
      case Event::Kind::Closed:
        OnClosed(e.bytes.empty() ? "connection closed" : e.bytes, now);
        break;
      case Event::Kind::Data: {
        if (state_ != ConnState::Connecting && state_ != ConnState::Connected) break;
        stats_.bytes_in += e.bytes.size();
        std::vector<NatsOp> parsed;
        std::string err;
        if (!parser_.Feed(e.bytes.data(), e.bytes.size(), parsed, &err)) {
          stats_.protocol_errors++;
          OnClosed("protocol error: " + err, now);
          break;
        }
        for (auto& op : parsed) {
          ops_bytes_ += OpBytes(op);
          ops_.push_back(std::move(op));
        }
        if (ops_bytes_ > opts_.max_pending_bytes) SlowConsumerClose(now);
        break;
      }
    }
    if (state_ == ConnState::Disconnected) {
      // Anything still queued belongs to the dead connection.
      ClearOps();
    }
  }
  size_t dispatched = 0;
  while (!ops_.empty() && state_ != ConnState::Closed) {
    if (ops_.front().kind == NatsOp::Kind::Msg && dispatched >= max_msgs) break;
    NatsOp op = std::move(ops_.front());
    ops_.pop_front();
    size_t b = OpBytes(op);
    ops_bytes_ = ops_bytes_ > b ? ops_bytes_ - b : 0;
    if (op.kind == NatsOp::Kind::Msg) ++dispatched;
    HandleOp(op, now);
  }

  // Timers.
  if (state_ == ConnState::Connecting && now >= connect_deadline_) {
    OnClosed("connect timeout", now);
  }
  if (state_ == ConnState::Disconnected && now >= next_attempt_) TryConnect(now);
  if (state_ == ConnState::Connected && now >= next_ping_) {
    if (pings_out_ >= opts_.max_pings_out) {
      OnClosed("stale connection (no PONG)", now);
    } else {
      pings_out_++;
      Send(nats_proto::Ping());
      next_ping_ = now + opts_.ping_interval_ms;
    }
  }
  // Request timeouts.
  std::vector<std::string> expired;
  for (const auto& kv : pending_)
    if (kv.second.deadline <= now) expired.push_back(kv.first);
  for (const auto& token : expired) {
    auto it = pending_.find(token);
    if (it == pending_.end()) continue;
    ReplyHandler h = it->second.handler;
    pending_.erase(it);
    for (auto r = req_ids_.begin(); r != req_ids_.end(); ++r) {
      if (r->second == token) {
        req_ids_.erase(r);
        break;
      }
    }
    h(nullptr, "timeout");
  }
  return dispatched;
}

int64_t NatsClient::Subscribe(const std::string& subject, MsgHandler handler, const std::string& queue) {
  if (!nats_proto::ValidSubject(subject, true)) return 0;
  int64_t sid = next_sid_++;
  subs_[sid] = Sub{subject, queue, std::move(handler)};
  if (state_ == ConnState::Connected) Send(nats_proto::Sub(subject, queue, sid));
  return sid;
}

void NatsClient::Unsubscribe(int64_t sid) {
  auto it = subs_.find(sid);
  if (it == subs_.end() || sid == resp_sid_) return;
  subs_.erase(it);
  if (state_ == ConnState::Connected) Send(nats_proto::Unsub(sid));
}

bool NatsClient::HasSubscription(const std::string& subject) const {
  for (const auto& kv : subs_)
    if (kv.second.subject == subject) return true;
  return false;
}

bool NatsClient::Publish(const std::string& subject, const std::string& data, const std::string& reply,
                         const NatsHeaders* headers) {
  if (!nats_proto::ValidSubject(subject, false)) return false;
  std::string bytes = headers && !headers->empty() ? nats_proto::HPub(subject, reply, *headers, data)
                                                   : nats_proto::Pub(subject, reply, data);
  if (bytes.empty()) return false;  // invalid headers (CR/LF or ':' in a key)
  stats_.msgs_out++;
  if (state_ == ConnState::Connected) {
    Send(bytes);
    return true;
  }
  if (state_ == ConnState::Closed || state_ == ConnState::Idle) return false;
  if (reconnect_buf_.size() + bytes.size() > opts_.reconnect_buffer_bytes) {
    stats_.dropped_while_disconnected++;
    return false;
  }
  reconnect_buf_ += bytes;
  return true;
}

uint64_t NatsClient::Request(const std::string& subject, const std::string& data, ReplyHandler handler,
                             int timeout_ms, bool many, const NatsHeaders* headers) {
  if (state_ != ConnState::Connected) {
    handler(nullptr, "disconnected");
    return 0;
  }
  EnsureRespMux();
  std::string token = NewToken();
  int t = timeout_ms > 0 ? timeout_ms : opts_.default_request_timeout_ms;
  pending_[token] = Pending{std::move(handler), many, t, now_ + t};
  uint64_t id = next_req_++;
  req_ids_[id] = token;
  if (!Publish(subject, data, resp_prefix_ + token, headers)) {
    auto it = pending_.find(token);
    ReplyHandler h = it->second.handler;
    pending_.erase(it);
    req_ids_.erase(id);
    h(nullptr, "bad_request");  // invalid subject or headers
    return 0;
  }
  return id;
}

void NatsClient::CancelRequest(uint64_t id) {
  auto r = req_ids_.find(id);
  if (r == req_ids_.end()) return;
  pending_.erase(r->second);
  req_ids_.erase(r);
}

std::string NatsClient::NewInbox() const { return "_INBOX." + NewToken(); }

void NatsClient::FailAllRequests(const std::string& why) {
  auto pend = std::move(pending_);
  pending_.clear();
  req_ids_.clear();
  for (auto& kv : pend) kv.second.handler(nullptr, why);
}

void NatsClient::Emit(ConnEvent e) {
  auto handlers = conn_handlers_;
  for (auto& h : handlers) h(e);
}

}  // namespace om
