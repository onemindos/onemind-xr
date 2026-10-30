// JetStream helpers over the plain NATS client (no JetStream library needed):
//  - KV get through Direct Get, with the STREAM.MSG.GET fallback for buckets
//    without allow_direct (nats-platform.md P1; layer-snapshot.md §3.2, §3.3);
//  - Direct Get of one stream message by sequence (inspector raw CoT, B6);
//  - an ordered push consumer (ephemeral, ack none, flow control, idle heartbeats,
//    gap detection and reset) used for KV watch and the `ENT` direct path
//    (layer-delta.md §3; nats-platform.md P10).
// JetStream API subjects: https://docs.nats.io/reference/reference-protocols/nats_api_reference
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/NatsClient.h"

#include <functional>
#include <memory>
#include <string>

namespace om {

struct OM_CORE_API StreamMsg {
  std::string stream;
  std::string subject;
  uint64_t seq = 0;
  std::string data;
  NatsHeaders headers;
  bool found = false;
};

/// Metadata carried in a JetStream push message's reply subject.
struct OM_CORE_API JsAckMeta {
  std::string stream, consumer;
  uint64_t delivered = 0, stream_seq = 0, consumer_seq = 0, pending = 0;
  int64_t timestamp_ns = 0;
  /// Parses `$JS.ACK.<stream>.<consumer>.<delivered>.<sseq>.<cseq>.<ts>.<pending>` and the
  /// newer 12-token form with domain and account hash.
  static bool Parse(const std::string& reply, JsAckMeta& out);
};

class OM_CORE_API JetStream {
 public:
  explicit JetStream(NatsClient& nc, std::string api_prefix = "$JS.API") : nc_(nc), api_(std::move(api_prefix)) {}

  using MsgCallback = std::function<void(const StreamMsg& msg, const std::string& error)>;

  /// KV get: `found=false` with empty error means the key is absent or deleted.
  void KvGet(const std::string& bucket, const std::string& key, MsgCallback cb, int timeout_ms = 3000);
  /// Message by sequence, Direct Get first, then STREAM.MSG.GET.
  void GetBySeq(const std::string& stream, uint64_t seq, MsgCallback cb, int timeout_ms = 3000);
  /// Last message on a subject.
  void GetLastBySubject(const std::string& stream, const std::string& subject, MsgCallback cb,
                        int timeout_ms = 3000);

  NatsClient& client() { return nc_; }
  const std::string& api() const { return api_; }

 private:
  void Fallback(const std::string& stream, const std::string& body, MsgCallback cb, int timeout_ms);
  NatsClient& nc_;
  std::string api_;
};

/// Ordered consumer. Delivers every message once, in stream order; recreates itself
/// from the last seen stream sequence after a gap, a missed heartbeat or a reconnect.
class OM_CORE_API OrderedConsumer {
 public:
  struct Config {
    std::string stream;
    std::string filter_subject;
    /// "all", "last_per_subject" (snapshot then live), "new", or "by_start_sequence".
    std::string deliver_policy = "last_per_subject";
    uint64_t start_seq = 0;
    int idle_heartbeat_ms = 5000;
    int inactive_threshold_ms = 30000;
    bool headers_only = false;
  };
  struct Delivery {
    const NatsMsg* msg = nullptr;  // never null for data messages
    JsAckMeta meta;
    /// Set for subject delete markers and KV delete/purge operations.
    std::string marker_reason;
  };
  using OnMsg = std::function<void(const Delivery&)>;
  /// Called once when the consumer has delivered everything that existed at creation
  /// (pending reached 0): the end of the snapshot.
  using OnCaughtUp = std::function<void()>;
  using OnError = std::function<void(const std::string& error)>;

  OrderedConsumer(JetStream& js, Config cfg, OnMsg on_msg, OnCaughtUp on_caught_up = nullptr,
                  OnError on_error = nullptr);
  ~OrderedConsumer();
  OrderedConsumer(const OrderedConsumer&) = delete;
  OrderedConsumer& operator=(const OrderedConsumer&) = delete;

  void Start(TimeMs now);
  void Stop();
  /// Call every frame: heartbeat watchdog and reset retries.
  void Tick(TimeMs now);

  bool caught_up() const { return caught_up_; }
  uint64_t last_stream_seq() const { return last_sseq_; }
  uint64_t resets() const { return resets_; }
  bool active() const { return sid_ != 0; }

 private:
  void Create(TimeMs now);
  void Reset(const std::string& why, TimeMs now);
  void OnDelivery(const NatsMsg& m);

  JetStream& js_;
  Config cfg_;
  OnMsg on_msg_;
  OnCaughtUp on_caught_up_;
  OnError on_error_;
  std::shared_ptr<bool> alive_;
  int64_t sid_ = 0;
  std::string deliver_;
  std::string consumer_name_;
  uint64_t last_sseq_ = 0;
  uint64_t expected_cseq_ = 1;
  bool caught_up_ = false;
  bool creating_ = false;
  bool stopped_ = true;
  TimeMs last_activity_ = 0;
  TimeMs retry_at_ = 0;
  TimeMs now_ = 0;
  uint64_t resets_ = 0;
  uint64_t generation_ = 0;
  uint64_t seen_reconnects_ = 0;
};

}  // namespace om
