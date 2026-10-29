// The OneMind client core in one object: bus connection, JetStream helpers, layers,
// entity details for the inspector, KV watches (world-state manifests, the S5 probe)
// and CoT sending. Both the Unreal subsystems and the headless host client drive it
// the same way: construct with a transport, Start(), then Tick(now) every frame.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/CoT.h"
#include "OneMindCore/Entity.h"
#include "OneMindCore/JetStream.h"
#include "OneMindCore/Layers.h"
#include "OneMindCore/NatsClient.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace om {

struct OM_CORE_API ClientConfig {
  NatsOptions nats;
  std::string client_id = "onemind-client";
  std::vector<LayerSpec> layers;
  /// KV watches started with the client: bucket -> key filter. "world-state" with
  /// "*.meta" feeds layer manifests (layer-snapshot.md §2).
  std::map<std::string, std::string> kv_watches;
  size_t max_msgs_per_tick = 20000;
  size_t max_detail_records = 2000;
};

class OM_CORE_API ClientCore {
 public:
  ClientCore(std::shared_ptr<ITransport> transport, ClientConfig cfg);
  ~ClientCore();

  void Start(TimeMs now);
  void Stop();
  void Tick(TimeMs now);

  NatsClient& nats() { return *nc_; }
  JetStream& js() { return *js_; }
  LayerManager& layers() { return *layers_; }
  const ClientConfig& config() const { return cfg_; }

  // --- KV (B3) -------------------------------------------------------------------
  using KvHandler = std::function<void(const std::string& key, const std::string& value, bool deleted)>;
  /// Watches `bucket` keys matching `key_filter` (NATS wildcards). Returns a watch id.
  int WatchKv(const std::string& bucket, const std::string& key_filter, KvHandler h, TimeMs now);
  void StopKvWatch(int id);
  void KvGet(const std::string& bucket, const std::string& key, JetStream::MsgCallback cb);

  // --- Inspector (B6) --------------------------------------------------------------
  /// Details for an entity from records already seen (direct/core paths), or nullptr.
  const EntityDetails* Details(const std::string& entity_id) const;
  /// Fetches the full record through svc.entity.get and updates the details cache.
  void FetchEntity(const std::string& entity_id, std::function<void(const EntityDetails*, const std::string&)> cb);
  /// Fetches the raw CoT behind a TAK-sourced entity (Direct Get on COT by sequence).
  void FetchRawCot(const std::string& entity_id, std::function<void(const std::string& xml, const std::string&)> cb);
  void RecordEntity(const Json& record);

  // --- CoT send (C2) ----------------------------------------------------------------
  using SendResult = std::function<void(const CotSendReply& reply, const std::string& error)>;
  /// Validates and sends through svc.cot.send. `kind`: marker | spi | route | geofence | chat.
  void SendCot(const std::string& kind, const CotEvent& ev, SendResult cb,
               const std::vector<std::string>& targets = {"server", "mesh"}, const std::string& tlp = "CLEAR");
  /// A unique uid for client-made CoT: "onemind-{client_id}-{n}-{token}".
  std::string NewCotUid();

 private:
  struct KvWatch {
    std::unique_ptr<OrderedConsumer> consumer;
    std::string bucket;
  };
  ClientConfig cfg_;
  std::unique_ptr<NatsClient> nc_;
  std::unique_ptr<JetStream> js_;
  std::unique_ptr<LayerManager> layers_;
  std::map<int, KvWatch> kv_;
  int next_kv_ = 1;
  std::map<std::string, EntityDetails> details_;
  std::shared_ptr<bool> alive_;
  uint64_t cot_counter_ = 0;
  TimeMs now_ = 0;
};

}  // namespace om
