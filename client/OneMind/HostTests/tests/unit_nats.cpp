// Unit tests: NATS protocol parser/serializer and NatsClient against a scripted fake
// transport (no network): connect, subscribe, request-many, no-responders, reconnect
// with resubscribe, publish buffering, stale-connection detection, JetStream ack meta.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/JetStream.h"
#include "OneMindCore/NatsClient.h"
#include "OneMindCore/NatsProtocol.h"
#include "TestHarness.h"

using namespace om;

TEST(nats_parser_ops_and_split_input) {
  NatsParser p;
  std::vector<NatsOp> ops;
  std::string wire =
      "INFO {\"server_id\":\"x\",\"version\":\"2.15.2\",\"headers\":true}\r\n"
      "PING\r\n+OK\r\n"
      "MSG foo.bar 7 _INBOX.r 5\r\nhello\r\n"
      "HMSG ent.drone.sim-x1.state 9 $JS.ACK.ENT.c.1.42.1.1793275200000000000.0 35 37\r\n"
      "NATS/1.0\r\nNats-Msg-Id: sim-x1:7\r\n\r\n{}\r\n"
      "HMSG _INBOX.a.b 3 16 16\r\nNATS/1.0 503\r\n\r\n\r\n"
      "-ERR 'Permissions Violation for Publish to \"x\"'\r\n";
  // Feed one byte at a time: the parser must handle any split.
  for (char c : wire) EXPECT_TRUE(p.Feed(&c, 1, ops));
  EXPECT_EQ(ops.size(), 7u);
  EXPECT_TRUE(ops[0].kind == NatsOp::Kind::Info);
  EXPECT_TRUE(ops[1].kind == NatsOp::Kind::Ping);
  EXPECT_TRUE(ops[3].kind == NatsOp::Kind::Msg);
  EXPECT_EQ(ops[3].msg.subject, std::string("foo.bar"));
  EXPECT_EQ(ops[3].msg.sid, 7);
  EXPECT_EQ(ops[3].msg.reply, std::string("_INBOX.r"));
  EXPECT_EQ(ops[3].msg.data, std::string("hello"));
  EXPECT_EQ(ops[4].msg.headers.GetOr("Nats-Msg-Id"), std::string("sim-x1:7"));
  EXPECT_EQ(ops[4].msg.data, std::string("{}"));
  EXPECT_EQ(ops[5].msg.headers.status, 503);
  EXPECT_TRUE(ops[5].msg.data.empty());
  EXPECT_TRUE(ops[6].kind == NatsOp::Kind::Err);
}

TEST(nats_parser_rejects_garbage) {
  NatsParser p;
  std::vector<NatsOp> ops;
  std::string bad = "BOGUS x\r\n";
  EXPECT_FALSE(p.Feed(bad.data(), bad.size(), ops));
  NatsParser p2;
  std::string bad2 = "MSG a 1 3\r\nabcX\r\n";  // payload longer than declared
  EXPECT_FALSE(p2.Feed(bad2.data(), bad2.size(), ops));
}

TEST(nats_serializer_and_subjects) {
  EXPECT_EQ(nats_proto::Pub("a.b", "", "hi"), std::string("PUB a.b 2\r\nhi\r\n"));
  NatsHeaders h;
  h.Add("Nats-Msg-Id", "x:1");
  std::string hp = nats_proto::HPub("a", "r", h, "{}");
  EXPECT_EQ(hp, std::string("HPUB a r 30 32\r\nNATS/1.0\r\nNats-Msg-Id: x:1\r\n\r\n{}\r\n"));
  NatsHeaders back;
  EXPECT_TRUE(NatsHeaders::Parse("NATS/1.0 100 Idle Heartbeat\r\nNats-Last-Consumer: 5\r\n\r\n", back));
  EXPECT_EQ(back.status, 100);
  EXPECT_EQ(back.description, std::string("Idle Heartbeat"));
  EXPECT_EQ(back.GetOr("nats-last-consumer"), std::string("5"));
  EXPECT_TRUE(nats_proto::ValidSubject("ent.*.*.state", true));
  EXPECT_FALSE(nats_proto::ValidSubject("ent.*.*.state", false));
  EXPECT_FALSE(nats_proto::ValidSubject("ent..state", true));
  EXPECT_FALSE(nats_proto::ValidSubject("a.>.b", true));
  EXPECT_FALSE(nats_proto::ValidSubject("a b", true));
  EXPECT_TRUE(nats_proto::SubjectMatches("ent.*.*.state", "ent.drone.sim-x1.state"));
  EXPECT_FALSE(nats_proto::SubjectMatches("ent.*.*.state", "ent.drone.sim-x1.state.x"));
  EXPECT_TRUE(nats_proto::SubjectMatches("tak.cot.>", "tak.cot.mesh.rx"));
  EXPECT_FALSE(nats_proto::SubjectMatches("tak.cot.>", "tak.cot"));
}

TEST(js_ack_meta_both_forms) {
  JsAckMeta m;
  EXPECT_TRUE(JsAckMeta::Parse("$JS.ACK.ENT.abc.1.42.7.1793275200000000000.3", m));
  EXPECT_EQ(m.stream, std::string("ENT"));
  EXPECT_EQ(m.stream_seq, 42u);
  EXPECT_EQ(m.consumer_seq, 7u);
  EXPECT_EQ(m.pending, 3u);
  EXPECT_TRUE(JsAckMeta::Parse("$JS.ACK.hub.ACC123.ENT.abc.1.43.8.1793275200000000000.0.rnd", m));
  EXPECT_EQ(m.stream_seq, 43u);
  EXPECT_EQ(m.consumer_seq, 8u);
  EXPECT_EQ(m.pending, 0u);
  EXPECT_FALSE(JsAckMeta::Parse("_INBOX.x", m));
}

namespace {
// A scripted in-memory "server": records what the client sends, lets the test inject
// server bytes, and simulates open/close.
class FakeTransport : public ITransport {
 public:
  Listener* l = nullptr;
  std::vector<std::string> sent;
  int opens = 0, closes = 0;
  bool is_open = false;
  void SetListener(Listener* x) override { l = x; }
  bool Open(const std::string&) override {
    opens++;
    return true;
  }
  void Send(const std::string& b) override { sent.push_back(b); }
  void Close() override {
    closes++;
    is_open = false;
  }
  void ServerAccept() {
    is_open = true;
    l->OnTransportOpen();
    Inject("INFO {\"server_id\":\"fake\",\"version\":\"2.15.2\",\"headers\":true,\"max_payload\":1048576}\r\n");
  }
  void Inject(const std::string& s) { l->OnTransportData(s.data(), s.size()); }
  void Drop(const std::string& why) {
    is_open = false;
    l->OnTransportClosed(why);
  }
  std::string All() const {
    std::string a;
    for (const auto& s : sent) a += s;
    return a;
  }
  // Extracts the reply subject of the last PUB/HPUB to `subject`.
  std::string LastReplyFor(const std::string& subject) const {
    std::string all = All();
    std::string key = "PUB " + subject + " ";
    size_t p = all.rfind(key);
    if (p == std::string::npos) return std::string();
    size_t s = p + key.size();
    size_t e = all.find(' ', s);
    return all.substr(s, e - s);
  }
};
}  // namespace

TEST(nats_client_connect_subscribe_request_many) {
  auto t = std::make_shared<FakeTransport>();
  NatsOptions o;
  o.user = "u";
  o.password = "p";
  NatsClient nc(t, o);
  TimeMs now = 1000;
  nc.Start(now);
  EXPECT_EQ(t->opens, 1);
  int got = 0;
  int64_t sid = nc.Subscribe("ent.drone.*.state", [&](const NatsMsg& m) {
    got++;
    EXPECT_EQ(m.subject, std::string("ent.drone.sim-x1.state"));
  });
  EXPECT_TRUE(sid > 0);
  t->ServerAccept();
  nc.Poll(now);
  EXPECT_TRUE(nc.connected());
  std::string all = t->All();
  EXPECT_TRUE(all.find("CONNECT {") == 0);
  EXPECT_TRUE(all.find("\"headers\":true") != std::string::npos);
  EXPECT_TRUE(all.find("\"no_responders\":true") != std::string::npos);
  EXPECT_TRUE(all.find("\"user\":\"u\"") != std::string::npos);
  EXPECT_TRUE(all.find("SUB ent.drone.*.state " + std::to_string(sid) + "\r\n") != std::string::npos);
  t->Inject("MSG ent.drone.sim-x1.state " + std::to_string(sid) + " 2\r\n{}\r\n");
  nc.Poll(now);
  EXPECT_EQ(got, 1);

  // Request-many: three chunks, the last one ends it.
  int chunks = 0;
  bool finished = false;
  nc.Request(
      "svc.layer.snapshot", "{}",
      [&](const NatsMsg* m, const std::string& err) {
        EXPECT_TRUE(m != nullptr);
        if (!m) return true;
        chunks++;
        finished = m->data.find("\"last\":true") != std::string::npos;
        return finished;
      },
      1000, true);
  nc.Poll(now);  // flush
  std::string reply = t->LastReplyFor("svc.layer.snapshot");
  EXPECT_TRUE(reply.rfind("_INBOX.", 0) == 0);
  // Find the resp mux sid from the SUB line.
  std::string all2 = t->All();
  size_t sp = all2.find("SUB _INBOX.");
  size_t sid_start = all2.find(' ', all2.find(' ', sp) + 1) + 1;
  std::string mux_sid = all2.substr(sid_start, all2.find("\r\n", sid_start) - sid_start);
  for (int i = 0; i < 3; ++i) {
    std::string body = i == 2 ? "{\"last\":true}" : "{\"last\":false}";
    t->Inject("MSG " + reply + " " + mux_sid + " " + std::to_string(body.size()) + "\r\n" + body + "\r\n");
    nc.Poll(now += 100);
  }
  EXPECT_EQ(chunks, 3);
  EXPECT_TRUE(finished);

  // No responders: an empty 503 status reply.
  std::string error;
  nc.Request("svc.nobody", "", [&](const NatsMsg* m, const std::string& err) {
    error = err;
    return true;
  });
  std::string r2 = t->LastReplyFor("svc.nobody");
  t->Inject("HMSG " + r2 + " " + mux_sid + " 16 16\r\nNATS/1.0 503\r\n\r\n\r\n");
  nc.Poll(now);
  EXPECT_EQ(error, std::string("no_responders"));

  // Timeout.
  std::string terr;
  nc.Request("svc.slow", "", [&](const NatsMsg*, const std::string& err) {
    terr = err;
    return true;
  }, 500);
  nc.Poll(now += 600);
  EXPECT_EQ(terr, std::string("timeout"));

  // Server PING gets a PONG.
  t->sent.clear();
  t->Inject("PING\r\n");
  nc.Poll(now);
  EXPECT_TRUE(t->All().find("PONG\r\n") != std::string::npos);
}

TEST(nats_client_reconnect_resubscribes_and_flushes_buffer) {
  auto t = std::make_shared<FakeTransport>();
  NatsOptions o;
  o.reconnect_wait_min_ms = 100;
  o.reconnect_wait_max_ms = 400;
  NatsClient nc(t, o);
  int connected = 0, reconnected = 0, disconnected = 0;
  nc.AddConnHandler([&](NatsClient::ConnEvent e) {
    if (e == NatsClient::ConnEvent::Connected) connected++;
    if (e == NatsClient::ConnEvent::Reconnected) reconnected++;
    if (e == NatsClient::ConnEvent::Disconnected) disconnected++;
  });
  TimeMs now = 0;
  nc.Start(now);
  int64_t sid = nc.Subscribe("tak.cot.mesh.rx", [](const NatsMsg&) {});
  t->ServerAccept();
  nc.Poll(now);
  EXPECT_EQ(connected, 1);
  std::string inflight_err;
  nc.Request("svc.layer.watch", "{}", [&](const NatsMsg*, const std::string& e) {
    inflight_err = e;
    return true;
  }, 30000, true);
  t->Drop("leaf restarted");
  nc.Poll(now += 10);
  EXPECT_EQ(disconnected, 1);
  EXPECT_FALSE(nc.connected());
  EXPECT_EQ(inflight_err, std::string("disconnected"));
  // Publishing while down is buffered.
  EXPECT_TRUE(nc.Publish("evt.test.client.buffered", "x"));
  t->sent.clear();
  // Backoff: no reconnect before the minimum wait.
  int opens_before = t->opens;
  nc.Poll(now += 10);
  EXPECT_EQ(t->opens, opens_before);
  nc.Poll(now += 1000);
  EXPECT_EQ(t->opens, opens_before + 1);
  t->ServerAccept();
  nc.Poll(now += 10);
  EXPECT_TRUE(nc.connected());
  EXPECT_EQ(reconnected, 1);
  std::string all = t->All();
  EXPECT_TRUE(all.find("SUB tak.cot.mesh.rx " + std::to_string(sid)) != std::string::npos);
  EXPECT_TRUE(all.find("PUB evt.test.client.buffered 1\r\nx\r\n") != std::string::npos);
  EXPECT_TRUE(all.find("SUB tak.cot.mesh.rx") < all.find("PUB evt.test.client.buffered"));
  EXPECT_EQ(nc.stats().reconnects, 1u);
}

TEST(nats_client_detects_stale_connection) {
  auto t = std::make_shared<FakeTransport>();
  NatsOptions o;
  o.ping_interval_ms = 1000;
  o.max_pings_out = 2;
  NatsClient nc(t, o);
  TimeMs now = 0;
  nc.Start(now);
  t->ServerAccept();
  nc.Poll(now);
  t->Inject("PONG\r\n");  // answers the connect PING
  nc.Poll(now);
  EXPECT_TRUE(nc.connected());
  // Server goes silent: two unanswered pings, then the client drops and reconnects.
  nc.Poll(now += 1001);
  nc.Poll(now += 1001);
  EXPECT_TRUE(nc.connected());
  nc.Poll(now += 1001);
  EXPECT_FALSE(nc.connected());
  EXPECT_TRUE(nc.last_error().find("stale") != std::string::npos);
}

TEST(nats_client_fatal_err_and_unsubscribe) {
  auto t = std::make_shared<FakeTransport>();
  NatsClient nc(t, NatsOptions());
  nc.Start(0);
  t->ServerAccept();
  nc.Poll(0);
  int64_t sid = nc.Subscribe("a.b", [](const NatsMsg&) {});
  EXPECT_EQ(nc.subscription_count(), 1u);
  t->sent.clear();
  nc.Unsubscribe(sid);
  EXPECT_EQ(nc.subscription_count(), 0u);
  EXPECT_EQ(t->All(), std::string("UNSUB " + std::to_string(sid) + "\r\n"));
  t->Inject("-ERR 'Permissions Violation for Subscription to \"x\"'\r\n");
  nc.Poll(1);
  EXPECT_TRUE(nc.connected());  // permission errors keep the connection
  t->Inject("-ERR 'Authorization Violation'\r\n");
  nc.Poll(2);
  EXPECT_FALSE(nc.connected());
}

TEST(nats_headers_reject_crlf_and_colon) {
  NatsHeaders h;
  EXPECT_TRUE(h.Add("Nats-Msg-Id", "sim-x1:7"));  // ':' is fine in a value
  EXPECT_FALSE(h.Add("Bad:Key", "v"));
  EXPECT_FALSE(h.Add("Bad Key", "v"));
  EXPECT_FALSE(h.Add("", "v"));
  EXPECT_FALSE(h.Add("X-Note", "a\r\nInjected: yes"));
  EXPECT_FALSE(h.Add("X-Note", "a\nb"));
  EXPECT_EQ(h.kv.size(), 1u);
  EXPECT_TRUE(h.Valid());
  EXPECT_FALSE(nats_proto::HPub("a", "", h, "{}").empty());
  // Filled directly, bypassing Add: framing refuses.
  NatsHeaders raw;
  raw.kv.emplace_back("X-Note", "a\r\n\r\nPUB evil 1");
  EXPECT_FALSE(raw.Valid());
  EXPECT_TRUE(raw.Serialize().empty());
  EXPECT_TRUE(nats_proto::HPub("a", "", raw, "{}").empty());
  NatsHeaders raw2;
  raw2.kv.emplace_back("Evil:Key", "v");
  EXPECT_TRUE(nats_proto::HPub("a", "", raw2, "{}").empty());
  // NatsClient refuses to publish them.
  auto t = std::make_shared<FakeTransport>();
  NatsClient nc(t, NatsOptions());
  nc.Start(0);
  t->ServerAccept();
  nc.Poll(0);
  EXPECT_TRUE(nc.connected());
  t->sent.clear();
  EXPECT_FALSE(nc.Publish("evt.test.client.x", "{}", std::string(), &raw));
  EXPECT_TRUE(t->sent.empty());
  std::string err;
  EXPECT_EQ(nc.Request("svc.x", "{}", [&](const NatsMsg*, const std::string& e) {
    err = e;
    return true;
  }, 1000, false, &raw), 0u);
  EXPECT_EQ(err, std::string("bad_request"));
}

TEST(nats_client_slow_consumer_cap_closes_and_reconnects) {
  auto t = std::make_shared<FakeTransport>();
  NatsOptions o;
  o.max_pending_bytes = 64 * 1024;
  o.reconnect_wait_min_ms = 100;
  o.reconnect_wait_max_ms = 100;
  NatsClient nc(t, o);
  TimeMs now = 0;
  nc.Start(now);
  int got = 0;
  int64_t sid = nc.Subscribe("ent.air.*.state", [&](const NatsMsg&) { got++; });
  t->ServerAccept();
  nc.Poll(now);
  EXPECT_TRUE(nc.connected());
  std::string body(1000, 'x');
  auto msg = [&](int i) {
    return "MSG ent.air.a" + std::to_string(i) + ".state " + std::to_string(sid) + " 1000\r\n" + body + "\r\n";
  };
  // (1) Parsed but held back by the per-frame budget: ops_ grows past the cap.
  for (int i = 0; i < 40; ++i) t->Inject(msg(i));
  nc.Poll(now += 1, 10);
  EXPECT_EQ(got, 10);
  EXPECT_TRUE(nc.connected());
  for (int round = 0; round < 3 && nc.connected(); ++round) {
    for (int i = 0; i < 40; ++i) t->Inject(msg(i));
    nc.Poll(now += 1, 10);
  }
  EXPECT_FALSE(nc.connected());
  EXPECT_EQ(nc.stats().slow_consumer_closes, 1u);
  EXPECT_TRUE(nc.last_error().find("slow consumer") == 0);
  int before = got;
  nc.Poll(now += 1);  // nothing from the dead connection is dispatched
  EXPECT_EQ(got, before);
  // Reconnects.
  nc.Poll(now += 500);
  t->ServerAccept();
  nc.Poll(now += 1);
  EXPECT_TRUE(nc.connected());
  EXPECT_EQ(nc.stats().reconnects, 1u);
  // (2) Game thread stalled: the transport queue itself passes the cap.
  for (int i = 0; i < 100; ++i) t->Inject(msg(i));  // ~100 KB, never polled in between
  nc.Poll(now += 1);
  EXPECT_FALSE(nc.connected());
  EXPECT_EQ(nc.stats().slow_consumer_closes, 2u);
  nc.Poll(now += 500);
  t->ServerAccept();
  nc.Poll(now += 1);
  EXPECT_TRUE(nc.connected());
  t->Inject(msg(1));
  int b2 = got;
  nc.Poll(now += 1);
  EXPECT_EQ(got, b2 + 1);
}
