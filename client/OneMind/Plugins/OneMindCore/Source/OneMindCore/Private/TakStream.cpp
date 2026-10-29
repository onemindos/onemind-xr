// SPDX-License-Identifier: GPL-3.0-or-later
// TAK Server streaming client and directed sends (feature C3); see TakStream.h.
#include "OneMindCore/TakStream.h"

#include "CotDetail.h"
#include "OneMindCore/TakProto.h"
#include "OneMindCore/Xml.h"

#include <cstdlib>

namespace om {

namespace {
const char kDecl[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";

bool IsStreamControl(const std::string& type) {
  return type.rfind("t-x-takp-", 0) == 0 || type == "t-x-c-t" || type == "t-x-c-t-r";
}
}  // namespace

CotEvent BuildSelfSa(const TakSelf& s, TimeMs now) {
  CotBuild b;
  b.uid = s.uid;
  b.callsign = s.callsign;
  b.stale_s = s.stale_s;
  b.simulated = s.simulated;
  CotEvent ev = cot_detail::Base(b, s.cot_type, s.point, now);
  ev.SetAttribute("how", "m-g");
  // Replace the callsign-only <contact> from Base with the full contact card.
  ev.RemoveNodes("detail\tcontact");
  std::vector<std::pair<std::string, std::string>> contact;
  if (!s.endpoint.empty()) contact.push_back({"endpoint", s.endpoint});
  contact.push_back({"callsign", s.callsign.substr(0, 64)});
  ev.AddNode("detail\tcontact", contact);
  ev.AddNode("detail\t__group", {{"name", s.group_name}, {"role", s.group_role}});
  ev.AddNode("detail\ttakv", {{"device", s.device}, {"platform", s.platform}, {"os", s.os}, {"version", s.version}});
  ev.AddNode("detail\tuid", {{"Droid", s.callsign.substr(0, 64)}});
  ev.AddNode("detail\tprecisionlocation", {{"geopointsrc", "USER"}, {"altsrc", "???"}});
  if (s.battery >= 0) ev.AddNode("detail\tstatus", {{"battery", std::to_string(s.battery)}});
  if (s.course >= 0 && s.speed >= 0)
    ev.AddNode("detail\ttrack", {{"speed", cot_detail::FormatNum(s.speed)}, {"course", cot_detail::FormatNum(s.course)}});
  return ev;
}

// ---------------------------------------------------------------------------------------------

TakStreamClient::TakStreamClient(std::shared_ptr<ITransport> transport, TakStreamOptions opts, TakSelf self)
    : transport_(std::move(transport)), opts_(std::move(opts)), self_(std::move(self)) {
  transport_->SetListener(this);
}

TakStreamClient::~TakStreamClient() {
  Stop();
  transport_->SetListener(nullptr);
}

void TakStreamClient::SetState(State s, const std::string& reason) {
  if (state_ == s) return;
  state_ = s;
  if (on_state_) on_state_(s, reason);
}

void TakStreamClient::Start(TimeMs now) {
  now_ = now;
  attempt_ = 0;
  next_attempt_ = now;
  SetState(State::Disconnected, "start");
}

void TakStreamClient::Stop() {
  if (state_ == State::Stopped || state_ == State::Idle) {
    state_ = State::Stopped;
    return;
  }
  transport_->Close();
  {
    std::lock_guard<std::mutex> l(mu_);
    events_.clear();
  }
  SetState(State::Stopped, "stop");
}

void TakStreamClient::OnTransportOpen() {
  std::lock_guard<std::mutex> l(mu_);
  events_.push_back(Ev{Ev::Kind::Open, std::string()});
}
void TakStreamClient::OnTransportData(const char* data, size_t len) {
  std::lock_guard<std::mutex> l(mu_);
  events_.push_back(Ev{Ev::Kind::Data, std::string(data, len)});
}
void TakStreamClient::OnTransportClosed(const std::string& reason) {
  std::lock_guard<std::mutex> l(mu_);
  events_.push_back(Ev{Ev::Kind::Closed, reason});
}

void TakStreamClient::Poll(TimeMs now) {
  now_ = now;
  if (state_ == State::Stopped || state_ == State::Idle) return;
  std::deque<Ev> evs;
  {
    std::lock_guard<std::mutex> l(mu_);
    evs.swap(events_);
  }
  for (auto& e : evs) {
    if (state_ == State::Stopped) return;
    if (e.kind == Ev::Kind::Open) HandleOpen(now);
    else if (e.kind == Ev::Kind::Closed) HandleClosed(e.bytes, now);
    else if (state_ == State::Connected) {
      rx_ += e.bytes;
      last_rx_ = now;
      Parse(now);
    }
  }
  if (state_ == State::Disconnected && now >= next_attempt_) {
    SetState(State::Connecting, "connecting");
    if (!transport_->Open(opts_.url)) Fail("bad url " + opts_.url, now);
    return;
  }
  if (state_ != State::Connected) return;
  if (mode_ == Mode::Negotiating && now >= neg_deadline_) {
    xml_only_ = true;  // README step 6: indeterminate, disconnect; stay on XML afterwards
    Fail("protocol negotiation timed out", now);
    return;
  }
  if (now - last_rx_ >= opts_.dead_ms) {
    Fail("no data for " + std::to_string(opts_.dead_ms) + " ms", now);
    return;
  }
  if (now - last_rx_ >= opts_.ping_idle_ms && now - last_ping_ >= opts_.ping_repeat_ms) {
    if (mode_ != Mode::Negotiating) {
      Write(ControlXml("t-x-c-t", std::string(), now));
      stats_.pings_out++;
    }
    last_ping_ = now;
  }
  if (opts_.sa_interval_ms > 0 && now - last_sa_ >= opts_.sa_interval_ms) SendSelf(now);
}

void TakStreamClient::HandleOpen(TimeMs now) {
  if (state_ != State::Connecting) return;
  rx_.clear();
  mode_ = Mode::Xml;
  declined_ = false;
  last_rx_ = now;
  last_ping_ = now;
  attempt_ = 0;
  stats_.connects++;
  SetState(State::Connected, "connected");
  SendSelf(now);
  Flush(now);
}

void TakStreamClient::HandleClosed(const std::string& reason, TimeMs now) {
  if (state_ != State::Connected && state_ != State::Connecting) return;
  last_error_ = reason;
  if (state_ == State::Connected) stats_.disconnects++;
  int wait = opts_.reconnect_min_ms;
  for (int i = 0; i < attempt_ && wait < opts_.reconnect_max_ms; ++i) wait *= 2;
  if (wait > opts_.reconnect_max_ms) wait = opts_.reconnect_max_ms;
  wait = wait / 2 + std::rand() % (wait / 2 + 1);  // jitter
  ++attempt_;
  next_attempt_ = now + wait;
  mode_ = Mode::Xml;
  rx_.clear();
  SetState(State::Disconnected, reason);
}

void TakStreamClient::Fail(const std::string& why, TimeMs now) {
  last_error_ = why;
  transport_->Close();
  {
    std::lock_guard<std::mutex> l(mu_);
    events_.clear();  // Close() stops listener calls for this connection
  }
  if (state_ != State::Connected && state_ != State::Connecting) state_ = State::Connecting;
  HandleClosed(why, now);
}

void TakStreamClient::Parse(TimeMs now) {
  while (state_ == State::Connected && !rx_.empty()) {
    if (mode_ == Mode::Proto) {
      if (static_cast<unsigned char>(rx_[0]) != kTakMagic) {
        stats_.protocol_errors++;
        Fail("v1 stream: missing magic byte", now);
        return;
      }
      size_t pos = 1;
      uint64_t n = 0;
      if (!GetVarint(rx_.data(), rx_.size(), pos, n)) {
        if (rx_.size() > 11) {
          stats_.protocol_errors++;
          Fail("v1 stream: bad length", now);
        }
        return;  // wait for more bytes
      }
      if (n > opts_.max_event_bytes) {
        stats_.protocol_errors++;
        Fail("v1 stream: message over limit", now);
        return;
      }
      if (rx_.size() - pos < n) return;
      std::string payload = rx_.substr(pos, static_cast<size_t>(n));
      rx_.erase(0, pos + static_cast<size_t>(n));
      TakDecoded d;
      std::string err;
      if (!DecodeTakPayload(payload, d, &err)) {
        stats_.protocol_errors++;
        continue;  // one bad message does not end the session
      }
      if (d.has_event) HandleEventXml(d.xml, now);
      continue;
    }
    size_t end = rx_.find("</event>");
    if (end == std::string::npos) {
      if (rx_.size() > opts_.max_event_bytes) {
        stats_.protocol_errors++;
        Fail("XML stream: event over limit", now);
      }
      return;
    }
    std::string chunk = rx_.substr(0, end + 8);
    rx_.erase(0, end + 8);
    size_t start = chunk.find("<event");
    if (start == std::string::npos) continue;
    HandleEventXml(chunk.substr(start), now);
  }
}

void TakStreamClient::HandleEventXml(const std::string& xml, TimeMs now) {
  std::string err;
  std::unique_ptr<XmlNode> root = XmlParse(xml, &err);
  if (!root || root->tag != "event") {
    stats_.protocol_errors++;
    return;
  }
  std::string type = root->AttrOr("type");
  if (IsStreamControl(type)) {
    stats_.control_in++;
    XmlNode* tc = nullptr;
    if (XmlNode* d = root->Child("detail")) tc = d->Child("TakControl");
    if (type == "t-x-takp-v" && tc) {
      if (XmlNode* vi = tc->Child("TakServerVersionInfo")) server_version_ = vi->AttrOr("serverVersion");
      bool offers1 = false;
      for (XmlNode* s : tc->Children("TakProtocolSupport")) offers1 = offers1 || s->AttrOr("version") == "1";
      if (offers1 && opts_.protocol == "auto" && !xml_only_ && !declined_ && mode_ == Mode::Xml) {
        Write(ControlXml("t-x-takp-q", "<TakControl><TakRequest version=\"1\"/></TakControl>", now));
        mode_ = Mode::Negotiating;  // README step 6: no more XML from us until the answer
        neg_deadline_ = now + opts_.negotiation_timeout_ms;
        stats_.negotiations++;
      }
    } else if (type == "t-x-takp-r" && tc && mode_ == Mode::Negotiating) {
      XmlNode* r = tc->Child("TakResponse");
      if (r && r->AttrOr("status") == "true") {
        mode_ = Mode::Proto;
        stats_.negotiated_v1++;
      } else {
        mode_ = Mode::Xml;
        declined_ = true;
        stats_.declined++;
      }
      Flush(now);
    }
    return;
  }
  CotInfo info;
  if (!ParseCot(xml, info, &err)) {
    stats_.protocol_errors++;
    return;
  }
  stats_.events_in++;
  if (on_event_) on_event_(xml, info);
}

std::string TakStreamClient::ControlXml(const std::string& type, const std::string& inner, TimeMs now) const {
  std::string uid = type == "t-x-c-t" ? self_.uid + "-ping" : self_.uid;
  return "<event version=\"2.0\" uid=\"" + XmlEscape(uid) + "\" type=\"" + type + "\" time=\"" + CotTime(now) +
         "\" start=\"" + CotTime(now) + "\" stale=\"" + CotTime(now + 20000) +
         "\" how=\"m-g\"><point lat=\"0.0\" lon=\"0.0\" hae=\"0.0\" ce=\"999999\" le=\"999999\"/>" +
         (inner.empty() ? std::string("<detail/>") : "<detail>" + inner + "</detail>") + "</event>";
}

void TakStreamClient::Write(const std::string& xml) {
  if (mode_ == Mode::Proto) {
    std::string payload, err;
    if (!CotXmlToTakPayload(xml, payload, &err)) return;
    transport_->Send(FrameTakStream(payload));
  } else {
    transport_->Send(std::string(kDecl) + xml);
  }
}

void TakStreamClient::Flush(TimeMs now) {
  if (state_ != State::Connected || mode_ == Mode::Negotiating) return;
  while (!queue_.empty()) {
    Queued q = queue_.front();
    queue_.pop_front();
    if (now - q.at > opts_.max_queue_age_ms) {
      stats_.queue_dropped++;
      continue;
    }
    Write(q.xml);
    stats_.events_out++;
  }
}

void TakStreamClient::SendSelf(TimeMs now) {
  last_sa_ = now;
  if (self_.uid.empty()) return;
  std::string xml = BuildSelfSa(self_, now).ToXML();
  if (mode_ == Mode::Negotiating) return;  // the next interval sends it
  Write(xml);
  stats_.events_out++;
}

std::string TakStreamClient::Send(const CotEvent& ev) {
  std::string why = ValidateOutgoingCot(ev);
  if (!why.empty()) return why;
  if (IsStreamControl(ev.GetAttribute("type"))) return "stream control types are sent by the client itself";
  std::string xml = ev.ToXML();
  if (state_ == State::Connected && mode_ != Mode::Negotiating && queue_.empty()) {
    Write(xml);
    stats_.events_out++;
    return std::string();
  }
  if (queue_.size() >= opts_.max_queue) {
    queue_.pop_front();
    stats_.queue_dropped++;
  }
  queue_.push_back(Queued{xml, now_});
  return std::string();
}

void TakStreamClient::UpdateSelf(const TakSelf& self, bool send_now) {
  self_ = self;
  if (send_now && state_ == State::Connected) SendSelf(now_);
}

// ---------------------------------------------------------------------------------------------

bool EncodeDirected(const std::string& xml, bool v1, std::string& out, std::string* err) {
  if (!v1) {
    out = std::string(kDecl) + xml;
    return true;
  }
  std::string payload;
  if (!CotXmlToTakPayload(xml, payload, err)) return false;
  out = FrameTakMesh(payload);
  return true;
}

std::string DirectedUrl(const std::string& endpoint) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (true) {
    size_t c = endpoint.find(':', start);
    parts.push_back(endpoint.substr(start, c == std::string::npos ? std::string::npos : c - start));
    if (c == std::string::npos) break;
    start = c + 1;
  }
  if (parts.size() == 3 && parts[2] != "tcp") return std::string();
  if (parts.size() < 2 || parts.size() > 3 || parts[0].empty() || parts[0] == "*") return std::string();
  int port = std::atoi(parts[1].c_str());
  if (port <= 0 || port > 65535) return std::string();
  return "tcp://" + parts[0] + ":" + std::to_string(port);
}

struct TakDirect::Job : public ITransport::Listener {
  std::shared_ptr<ITransport> t;
  std::string bytes;
  Result cb;
  TimeMs deadline = 0;
  std::mutex mu;
  bool opened = false, closed = false, sent = false, done = false;
  std::string close_reason;
  void OnTransportOpen() override {
    std::lock_guard<std::mutex> l(mu);
    opened = true;
  }
  void OnTransportData(const char*, size_t) override {}
  void OnTransportClosed(const std::string& reason) override {
    std::lock_guard<std::mutex> l(mu);
    closed = true;
    close_reason = reason;
  }
};

TakDirect::~TakDirect() {
  for (auto& j : jobs_) {
    j->t->Close();
    j->t->SetListener(nullptr);
  }
}

void TakDirect::Send(const std::string& endpoint, const CotEvent& ev, bool v1, Result cb, TimeMs now,
                     int timeout_ms) {
  std::string why = ValidateOutgoingCot(ev);
  std::string url = DirectedUrl(endpoint);
  if (why.empty() && url.empty()) why = "endpoint is not host:port:tcp: " + endpoint;
  auto job = std::make_shared<Job>();
  if (why.empty() && !EncodeDirected(ev.ToXML(), v1, job->bytes, &why) && why.empty()) why = "encode failed";
  if (!why.empty()) {
    if (cb) cb(why);
    return;
  }
  job->cb = std::move(cb);
  job->deadline = now + timeout_ms;
  job->t = factory_();
  job->t->SetListener(job.get());
  if (!job->t->Open(url)) {
    job->t->SetListener(nullptr);
    if (job->cb) job->cb("bad url " + url);
    return;
  }
  jobs_.push_back(job);
}

void TakDirect::Poll(TimeMs now) {
  std::vector<std::shared_ptr<Job>> keep;
  for (auto& j : jobs_) {
    bool opened, closed;
    std::string reason;
    {
      std::lock_guard<std::mutex> l(j->mu);
      opened = j->opened;
      closed = j->closed;
      reason = j->close_reason;
    }
    std::string result;
    bool finished = false;
    if (opened && !j->sent) {
      j->t->Send(j->bytes);
      j->t->Close();  // transports flush queued bytes before closing
      j->sent = true;
      finished = true;
    } else if (closed && !j->sent) {
      result = reason.empty() ? "connection failed" : reason;
      finished = true;
    } else if (now > j->deadline) {
      j->t->Close();
      result = "timeout";
      finished = true;
    }
    if (finished) {
      j->t->SetListener(nullptr);
      if (j->cb) j->cb(result);
    } else {
      keep.push_back(j);
    }
  }
  jobs_.swap(keep);
}

}  // namespace om
