// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/JetStream.h"

#include "OneMindCore/Codec.h"
#include "OneMindCore/Json.h"

#include <cstdlib>

namespace om {

namespace js_detail {
uint64_t ToU64(const std::string& s) { return static_cast<uint64_t>(std::strtoull(s.c_str(), nullptr, 10)); }

bool FromDirectReply(const NatsMsg& m, StreamMsg& out, std::string& err) {
  if (m.headers.status == 404) {
    out.found = false;
    return true;
  }
  if (m.headers.status != 0) {
    err = "status " + std::to_string(m.headers.status) + " " + m.headers.description;
    return false;
  }
  out.found = true;
  out.stream = m.headers.GetOr("Nats-Stream");
  out.subject = m.headers.GetOr("Nats-Subject");
  out.seq = ToU64(m.headers.GetOr("Nats-Sequence", "0"));
  out.headers = m.headers;
  out.data = m.data;
  return true;
}

bool FromMsgGetJson(const std::string& body, StreamMsg& out, std::string& err) {
  Json j;
  if (!Json::Parse(body, j)) {
    err = "bad JSON reply";
    return false;
  }
  if (j["error"].is_object()) {
    if (j["error"].i64("code") == 404) {
      out.found = false;
      return true;
    }
    err = j["error"].str("description", "jetstream error");
    return false;
  }
  const Json& msg = j["message"];
  out.found = true;
  out.subject = msg.str("subject");
  out.seq = static_cast<uint64_t>(msg.i64("seq"));
  std::string data, hdrs;
  Base64Decode(msg.str("data"), data);
  out.data = data;
  if (Base64Decode(msg.str("hdrs"), hdrs) && !hdrs.empty()) NatsHeaders::Parse(hdrs, out.headers);
  return true;
}

bool IsKvDelete(const NatsHeaders& h) {
  std::string op = h.GetOr("KV-Operation");
  return op == "DEL" || op == "PURGE";
}
}  // namespace js_detail

bool JsAckMeta::Parse(const std::string& reply, JsAckMeta& out) {
  auto t = nats_proto::SplitTokens(reply);
  if (t.size() < 9 || t[0] != "$JS" || t[1] != "ACK") return false;
  size_t base;
  if (t.size() == 9) {
    base = 2;  // $JS.ACK.<stream>.<consumer>.<delivered>.<sseq>.<cseq>.<ts>.<pending>
  } else if (t.size() >= 11) {
    base = 4;  // $JS.ACK.<domain>.<acchash>.<stream>.<consumer>.<delivered>.<sseq>.<cseq>.<ts>.<pending>[.<token>]
  } else {
    return false;
  }
  out.stream = t[base];
  out.consumer = t[base + 1];
  out.delivered = js_detail::ToU64(t[base + 2]);
  out.stream_seq = js_detail::ToU64(t[base + 3]);
  out.consumer_seq = js_detail::ToU64(t[base + 4]);
  out.timestamp_ns = static_cast<int64_t>(js_detail::ToU64(t[base + 5]));
  out.pending = js_detail::ToU64(t[base + 6]);
  return true;
}

void JetStream::Fallback(const std::string& stream, const std::string& body, MsgCallback cb, int timeout_ms) {
  nc_.Request(
      api_ + ".STREAM.MSG.GET." + stream, body,
      [cb, stream](const NatsMsg* m, const std::string& error) {
        StreamMsg out;
        out.stream = stream;
        if (!m) {
          cb(out, error);
          return true;
        }
        std::string err;
        if (!js_detail::FromMsgGetJson(m->data, out, err)) {
          cb(out, err);
          return true;
        }
        out.stream = stream;
        if (out.found && js_detail::IsKvDelete(out.headers)) out.found = false;
        cb(out, std::string());
        return true;
      },
      timeout_ms);
}

void JetStream::KvGet(const std::string& bucket, const std::string& key, MsgCallback cb, int timeout_ms) {
  GetLastBySubject("KV_" + bucket, "$KV." + bucket + "." + key, std::move(cb), timeout_ms);
}

void JetStream::GetLastBySubject(const std::string& stream, const std::string& subject, MsgCallback cb,
                                 int timeout_ms) {
  std::string direct = api_ + ".DIRECT.GET." + stream + "." + subject;
  JetStream* self = this;
  Json body = Json::MakeObject();
  body.set("last_by_subj", subject);
  std::string fallback_body = body.Dump();
  nc_.Request(
      direct, std::string(),
      [self, cb, stream, fallback_body, timeout_ms](const NatsMsg* m, const std::string& error) {
        if (!m) {
          if (error == "no_responders") {
            self->Fallback(stream, fallback_body, cb, timeout_ms);  // stream without allow_direct
          } else {
            cb(StreamMsg{}, error);
          }
          return true;
        }
        StreamMsg out;
        std::string err;
        if (!js_detail::FromDirectReply(*m, out, err)) {
          cb(out, err);
          return true;
        }
        if (out.found && js_detail::IsKvDelete(out.headers)) out.found = false;
        cb(out, std::string());
        return true;
      },
      timeout_ms);
}

void JetStream::GetBySeq(const std::string& stream, uint64_t seq, MsgCallback cb, int timeout_ms) {
  Json body = Json::MakeObject();
  body.set("seq", static_cast<int64_t>(seq));
  std::string b = body.Dump();
  JetStream* self = this;
  nc_.Request(
      api_ + ".DIRECT.GET." + stream, b,
      [self, cb, stream, b, timeout_ms](const NatsMsg* m, const std::string& error) {
        if (!m) {
          if (error == "no_responders") self->Fallback(stream, b, cb, timeout_ms);
          else cb(StreamMsg{}, error);
          return true;
        }
        StreamMsg out;
        std::string err;
        if (!js_detail::FromDirectReply(*m, out, err)) {
          cb(out, err);
          return true;
        }
        if (out.stream.empty()) out.stream = stream;
        cb(out, std::string());
        return true;
      },
      timeout_ms);
}

// ---------------------------------------------------------------------------------------------
// OrderedConsumer

OrderedConsumer::OrderedConsumer(JetStream& js, Config cfg, OnMsg on_msg, OnCaughtUp on_caught_up,
                                 OnError on_error)
    : js_(js),
      cfg_(std::move(cfg)),
      on_msg_(std::move(on_msg)),
      on_caught_up_(std::move(on_caught_up)),
      on_error_(std::move(on_error)),
      alive_(std::make_shared<bool>(true)) {}

OrderedConsumer::~OrderedConsumer() {
  Stop();
  *alive_ = false;
}

void OrderedConsumer::Start(TimeMs now) {
  stopped_ = false;
  now_ = now;
  seen_reconnects_ = js_.client().stats().reconnects;
  Create(now);
}

void OrderedConsumer::Stop() {
  if (stopped_) return;
  stopped_ = true;
  generation_++;
  NatsClient& nc = js_.client();
  if (sid_) nc.Unsubscribe(sid_);
  sid_ = 0;
  if (!consumer_name_.empty() && nc.connected()) {
    // Best effort; an ephemeral consumer also goes away after inactive_threshold.
    nc.Request(js_.api() + ".CONSUMER.DELETE." + cfg_.stream + "." + consumer_name_, std::string(),
               [](const NatsMsg*, const std::string&) { return true; }, 2000);
  }
  consumer_name_.clear();
  creating_ = false;
}

void OrderedConsumer::Create(TimeMs now) {
  NatsClient& nc = js_.client();
  if (stopped_) return;
  if (!nc.connected()) {
    retry_at_ = now + 500;
    return;
  }
  generation_++;
  uint64_t gen = generation_;
  std::weak_ptr<bool> alive = alive_;
  deliver_ = nc.NewInbox();
  expected_cseq_ = 1;
  sid_ = nc.Subscribe(deliver_, [this, alive, gen](const NatsMsg& m) {
    auto a = alive.lock();
    if (!a || !*a || gen != generation_) return;
    OnDelivery(m);
  });

  Json c = Json::MakeObject();
  c.set("deliver_subject", deliver_);
  if (last_sseq_ > 0) {
    c.set("deliver_policy", "by_start_sequence");
    c.set("opt_start_seq", static_cast<int64_t>(last_sseq_ + 1));
  } else if (cfg_.deliver_policy == "by_start_sequence") {
    c.set("deliver_policy", "by_start_sequence");
    c.set("opt_start_seq", static_cast<int64_t>(cfg_.start_seq > 0 ? cfg_.start_seq : 1));
  } else {
    c.set("deliver_policy", cfg_.deliver_policy);
  }
  if (!cfg_.filter_subject.empty()) c.set("filter_subject", cfg_.filter_subject);
  c.set("ack_policy", "none");
  c.set("replay_policy", "instant");
  c.set("flow_control", true);
  c.set("idle_heartbeat", static_cast<int64_t>(cfg_.idle_heartbeat_ms) * 1000000);
  c.set("inactive_threshold", static_cast<int64_t>(cfg_.inactive_threshold_ms) * 1000000);
  c.set("mem_storage", true);
  c.set("num_replicas", 1);
  if (cfg_.headers_only) c.set("headers_only", true);
  Json body = Json::MakeObject();
  body.set("stream_name", cfg_.stream);
  body.set("config", c);

  creating_ = true;
  last_activity_ = now;
  nc.Request(
      js_.api() + ".CONSUMER.CREATE." + cfg_.stream, body.Dump(),
      [this, alive, gen](const NatsMsg* m, const std::string& error) {
        auto a = alive.lock();
        if (!a || !*a || gen != generation_) return true;
        creating_ = false;
        std::string err = error;
        Json j;
        if (m && !Json::Parse(m->data, j)) err = "bad consumer create reply";
        if (err.empty() && j["error"].is_object()) err = j["error"].str("description", "consumer create failed");
        if (!err.empty()) {
          if (on_error_) on_error_("consumer create: " + err);
          js_.client().Unsubscribe(sid_);
          sid_ = 0;
          retry_at_ = now_ + 2000;
          return true;
        }
        consumer_name_ = j.str("name");
        last_activity_ = now_;
        if (!caught_up_ && j.i64("num_pending", -1) == 0 && expected_cseq_ == 1) {
          caught_up_ = true;
          if (on_caught_up_) on_caught_up_();
        }
        return true;
      },
      5000);
}

void OrderedConsumer::Reset(const std::string& why, TimeMs now) {
  resets_++;
  if (on_error_) on_error_("reset: " + why);
  NatsClient& nc = js_.client();
  if (sid_) nc.Unsubscribe(sid_);
  sid_ = 0;
  if (!consumer_name_.empty() && nc.connected()) {
    nc.Request(js_.api() + ".CONSUMER.DELETE." + cfg_.stream + "." + consumer_name_, std::string(),
               [](const NatsMsg*, const std::string&) { return true; }, 2000);
  }
  consumer_name_.clear();
  creating_ = false;
  generation_++;
  retry_at_ = now;  // recreate on the next Tick
}

void OrderedConsumer::OnDelivery(const NatsMsg& m) {
  last_activity_ = now_;
  NatsClient& nc = js_.client();
  if (m.headers.status == 100) {
    if (!m.reply.empty()) nc.Publish(m.reply, std::string());  // flow control request
    const std::string* stalled = m.headers.Get("Nats-Consumer-Stalled");
    if (stalled && !stalled->empty()) nc.Publish(*stalled, std::string());
    const std::string* last_c = m.headers.Get("Nats-Last-Consumer");
    if (last_c && js_detail::ToU64(*last_c) + 1 != expected_cseq_ && expected_cseq_ > 0) {
      Reset("heartbeat shows missed messages", now_);
    }
    return;
  }
  if (m.headers.status != 0) {
    // 408/409 and other statuses: the consumer is gone or broken; start again.
    Reset("status " + std::to_string(m.headers.status) + " " + m.headers.description, now_);
    return;
  }
  JsAckMeta meta;
  if (!JsAckMeta::Parse(m.reply, meta)) return;
  if (meta.consumer_seq != expected_cseq_) {
    Reset("consumer sequence gap", now_);
    return;
  }
  expected_cseq_++;
  last_sseq_ = meta.stream_seq;
  Delivery d;
  d.msg = &m;
  d.meta = meta;
  if (const std::string* r = m.headers.Get("Nats-Marker-Reason")) d.marker_reason = *r;
  else if (const std::string* op = m.headers.Get("KV-Operation")) {
    if (*op == "DEL" || *op == "PURGE") d.marker_reason = *op;
  }
  uint64_t gen = generation_;
  if (on_msg_) on_msg_(d);
  if (gen != generation_) return;  // the callback stopped or reset us
  if (!caught_up_ && meta.pending == 0) {
    caught_up_ = true;
    if (on_caught_up_) on_caught_up_();
  }
}

void OrderedConsumer::Tick(TimeMs now) {
  now_ = now;
  if (stopped_) return;
  NatsClient& nc = js_.client();
  if (sid_ == 0 && !creating_) {
    if (now >= retry_at_) Create(now);
    return;
  }
  if (!nc.connected()) return;
  if (nc.stats().reconnects != seen_reconnects_) {
    seen_reconnects_ = nc.stats().reconnects;
    Reset("reconnected", now);
    return;
  }
  if (!creating_ && now - last_activity_ > 3 * static_cast<TimeMs>(cfg_.idle_heartbeat_ms)) {
    Reset("no heartbeat", now);
  }
}

}  // namespace om
