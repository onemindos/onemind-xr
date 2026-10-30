// Unit tests: JSON, codecs, WebSocket framing.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/Codec.h"
#include "OneMindCore/Json.h"
#include "OneMindCore/WebSocketFraming.h"
#include "TestHarness.h"

using namespace om;

TEST(json_roundtrip_and_types) {
  Json j;
  EXPECT_TRUE(Json::Parse(R"({"a":1,"b":[true,false,null],"c":"x\"y\u00e9\n","d":-1.5e3,"e":{}})", j));
  EXPECT_EQ(j.i64("a"), 1);
  EXPECT_EQ(j["b"].size(), 3u);
  EXPECT_TRUE(j["b"][0].as_bool());
  EXPECT_TRUE(j["b"][2].is_null());
  EXPECT_EQ(j.str("c"), std::string("x\"y\xc3\xa9\n"));
  EXPECT_NEAR(j.num("d"), -1500.0, 1e-9);
  EXPECT_TRUE(j["e"].is_object());
  EXPECT_TRUE(j["missing"]["deeper"].is_null());
  Json k;
  EXPECT_TRUE(Json::Parse(j.Dump(), k));
  EXPECT_EQ(k.Dump(), j.Dump());
  // 64-bit epoch ms survive (doubles are exact to 2^53).
  Json t;
  EXPECT_TRUE(Json::Parse(R"({"t":1793275200123})", t));
  EXPECT_EQ(t.i64("t"), int64_t(1793275200123));
  EXPECT_EQ(t.Dump(), std::string(R"({"t":1793275200123})"));
}

TEST(json_rejects_bad_input) {
  Json j;
  std::string err;
  EXPECT_FALSE(Json::Parse("{\"a\":}", j, &err));
  EXPECT_FALSE(err.empty());
  EXPECT_FALSE(Json::Parse("[1,2", j));
  EXPECT_FALSE(Json::Parse("{} trailing", j));
  EXPECT_FALSE(Json::Parse("\"\\ud800\"", j));
  std::string deep(300, '[');
  EXPECT_FALSE(Json::Parse(deep, j));
}

TEST(base64_base32_sha) {
  EXPECT_EQ(Base64Encode(std::string("")), std::string(""));
  EXPECT_EQ(Base64Encode(std::string("f")), std::string("Zg=="));
  EXPECT_EQ(Base64Encode(std::string("foobar")), std::string("Zm9vYmFy"));
  std::string out;
  EXPECT_TRUE(Base64Decode("Zm9vYg==", out));
  EXPECT_EQ(out, std::string("foob"));
  const char* abc = "abc";
  auto s1 = Sha1(reinterpret_cast<const uint8_t*>(abc), 3);
  EXPECT_EQ(HexLower(s1.data(), s1.size()), std::string("a9993e364706816aba3e25717850c26c9cd0d89d"));
  auto s2 = Sha256(reinterpret_cast<const uint8_t*>(abc), 3);
  EXPECT_EQ(HexLower(s2.data(), s2.size()),
            std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  const char* fb = "foobar";
  EXPECT_EQ(Base32LowerNoPad(reinterpret_cast<const uint8_t*>(fb), 6), std::string("mzxw6ytboi"));
  EXPECT_EQ(NewToken().size(), 22u);
  EXPECT_TRUE(NewToken() != NewToken());
}

TEST(ws_handshake_accept_rfc6455_example) {
  // RFC 6455 §1.3 example key and accept value.
  EXPECT_EQ(WsAcceptFor("dGhlIHNhbXBsZSBub25jZQ=="), std::string("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
  std::string resp =
      "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
  std::string err;
  EXPECT_TRUE(WsCheckHandshakeResponse(resp, "dGhlIHNhbXBsZSBub25jZQ==", &err));
  EXPECT_FALSE(WsCheckHandshakeResponse("HTTP/1.1 400 Bad\r\n\r\n", "x", &err));
  WsUrl u;
  EXPECT_TRUE(WsUrl::Parse("ws://127.0.0.1:9222", u));
  EXPECT_EQ(u.port, 9222);
  EXPECT_EQ(u.path, std::string("/"));
  EXPECT_TRUE(WsUrl::Parse("wss://user:pw@bus.example:443/nats", u));
  EXPECT_TRUE(u.secure);
  EXPECT_EQ(u.host, std::string("bus.example"));
  EXPECT_EQ(u.path, std::string("/nats"));
  EXPECT_FALSE(WsUrl::Parse("nats://127.0.0.1:4222", u));
  std::string req = WsBuildHandshake(u, "k");
  EXPECT_TRUE(req.find("user:pw") == std::string::npos);  // credentials never in the request line
}

TEST(ws_frame_encode_decode_and_fragmentation) {
  uint8_t mask[4] = {1, 2, 3, 4};
  std::string payload(70000, 'x');
  std::string f = WsEncodeFrame(WsOpcode::Binary, payload, mask);
  // Decode a masked frame byte by byte (servers never mask, but the decoder accepts it).
  WsDecoder d;
  std::vector<WsDecoder::Message> msgs;
  for (size_t i = 0; i < f.size(); i += 997) {
    EXPECT_TRUE(d.Feed(f.data() + i, (std::min)(size_t(997), f.size() - i), msgs));
  }
  EXPECT_EQ(msgs.size(), 1u);
  EXPECT_EQ(msgs[0].payload.size(), payload.size());
  EXPECT_TRUE(msgs[0].payload == payload);
  // Unmasked server frames: fragmented text + interleaved ping.
  std::string s;
  s += "\x01\x03" "PUB";            // text, not FIN
  s += "\x89\x02" "hi";             // ping (control, FIN)
  s += "\x80\x04" " foo";           // continuation, FIN
  msgs.clear();
  WsDecoder d2;
  EXPECT_TRUE(d2.Feed(s.data(), s.size(), msgs));
  EXPECT_EQ(msgs.size(), 2u);
  EXPECT_TRUE(msgs[0].op == WsOpcode::Ping);
  EXPECT_EQ(msgs[1].payload, std::string("PUB foo"));
  // Reserved bits (compression) are a protocol error since none was negotiated.
  std::string bad("\xC2\x00", 2);
  WsDecoder d3;
  EXPECT_FALSE(d3.Feed(bad.data(), bad.size(), msgs));
}
