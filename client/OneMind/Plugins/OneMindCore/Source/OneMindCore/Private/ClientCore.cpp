// SPDX-License-Identifier: GPL-3.0-or-later
#include "OneMindCore/ClientCore.h"

#include "OneMindCore/Codec.h"

#include <algorithm>

namespace om {

ClientCore::ClientCore(std::shared_ptr<ITransport> transport, ClientConfig cfg)
    : cfg_(std::move(cfg)), alive_(std::make_shared<bool>(true)) {
  nc_.reset(new NatsClient(std::move(transport), cfg_.nats));
  js_.reset(new JetStream(*nc_));
  layers_.reset(new LayerManager(*nc_, *js_));
  for (const auto& l : cfg_.layers) layers_->Register(l);
  layers_->SetRecordSink([this](const Json& r) { RecordEntity(r); });
}

ClientCore::~ClientCore() {
  Stop();
  *alive_ = false;
}

void ClientCore::Start(TimeMs now) {
  now_ = now;
  nc_->Start(now);
  for (const auto& kv : cfg_.kv_watches) {
    std::string bucket = kv.first;
    LayerManager* lm = layers_.get();
    WatchKv(
        kv.first, kv.second,
        [lm, bucket](const std::string& key, const std::string& value, bool deleted) {
          if (bucket != "world-state") return;
          // `{layer}.meta`: a missing (deleted/expired) manifest means the layer is down.
          if (key.size() > 5 && key.compare(key.size() - 5, 5, ".meta") == 0) {
            std::string layer = key.substr(0, key.size() - 5);
            LayerManifest m;
            Json j;
            if (!deleted && Json::Parse(value, j) && LayerManifest::FromJson(j, m)) lm->OnManifest(layer, m);
            else lm->OnManifest(layer, LayerManifest());
          }
        },
        now);
  }
}

void ClientCore::Stop() {
  for (const auto& id : layers_->Ids()) layers_->Toggle(id, false, now_);
  for (auto& kv : kv_) kv.second.consumer->Stop();
  kv_.clear();
  nc_->Stop();
}

void ClientCore::Tick(TimeMs now) {
  now_ = now;
  nc_->Poll(now, cfg_.max_msgs_per_tick);
  for (auto& kv : kv_) kv.second.consumer->Tick(now);
  layers_->Tick(now);
}

int ClientCore::WatchKv(const std::string& bucket, const std::string& key_filter, KvHandler h, TimeMs now) {
  OrderedConsumer::Config c;
  c.stream = "KV_" + bucket;
  c.filter_subject = "$KV." + bucket + "." + key_filter;
  c.deliver_policy = "last_per_subject";
  std::string prefix = "$KV." + bucket + ".";
  int id = next_kv_++;
  KvWatch w;
  w.bucket = bucket;
  w.consumer.reset(new OrderedConsumer(
      *js_, c,
      [h, prefix](const OrderedConsumer::Delivery& d) {
        std::string key = d.msg->subject.substr((std::min)(d.msg->subject.size(), prefix.size()));
        bool deleted = !d.marker_reason.empty();
        h(key, deleted ? std::string() : d.msg->data, deleted);
      }));
  w.consumer->Start(now);
  kv_[id] = std::move(w);
  return id;
}

void ClientCore::StopKvWatch(int id) {
  auto it = kv_.find(id);
  if (it == kv_.end()) return;
  it->second.consumer->Stop();
  kv_.erase(it);
}

void ClientCore::KvGet(const std::string& bucket, const std::string& key, JetStream::MsgCallback cb) {
  js_->KvGet(bucket, key, std::move(cb));
}

void ClientCore::RecordEntity(const Json& record) {
  std::string id = record.str("entity_id");
  if (id.empty()) return;
  if (details_.size() >= cfg_.max_detail_records && !details_.count(id)) {
    // Bounded: drop an arbitrary old entry (the inspector refetches on demand).
    details_.erase(details_.begin());
  }
  details_[id].Update(record);
}

const EntityDetails* ClientCore::Details(const std::string& entity_id) const {
  auto it = details_.find(entity_id);
  return it == details_.end() ? nullptr : &it->second;
}

void ClientCore::FetchEntity(const std::string& entity_id,
                             std::function<void(const EntityDetails*, const std::string&)> cb) {
  Json q = Json::MakeObject();
  q.set("entity_id", entity_id);
  std::weak_ptr<bool> alive = alive_;
  nc_->Request("svc.entity.get", q.Dump(), [this, alive, entity_id, cb](const NatsMsg* m, const std::string& err) {
    auto a = alive.lock();
    if (!a || !*a) return true;
    if (!m) {
      cb(Details(entity_id), err);
      return true;
    }
    if (const std::string* code = m->headers.Get("Nats-Service-Error-Code")) {
      cb(Details(entity_id), "svc.entity.get error " + *code + " " + m->headers.GetOr("Nats-Service-Error"));
      return true;
    }
    Json j;
    if (!Json::Parse(m->data, j)) {
      cb(Details(entity_id), "bad JSON");
      return true;
    }
    RecordEntity(j);
    cb(Details(entity_id), std::string());
    return true;
  });
}

void ClientCore::FetchRawCot(const std::string& entity_id,
                             std::function<void(const std::string&, const std::string&)> cb) {
  const EntityDetails* d = Details(entity_id);
  std::string stream, subject;
  uint64_t seq = 0;
  if (!d || !FindRawCotSource(d->record, stream, seq, subject)) {
    cb(std::string(), "no CoT source for " + entity_id);
    return;
  }
  js_->GetBySeq(stream, seq, [cb](const StreamMsg& m, const std::string& err) {
    if (!err.empty()) cb(std::string(), err);
    else if (!m.found) cb(std::string(), "message no longer in the stream");
    else cb(m.data, std::string());
  });
}

std::string ClientCore::NewCotUid() {
  return "onemind-" + cfg_.client_id + "-" + std::to_string(++cot_counter_) + "-" + NewToken().substr(0, 8);
}

void ClientCore::SendCot(const std::string& kind, const CotEvent& ev, SendResult cb,
                         const std::vector<std::string>& targets, const std::string& tlp) {
  std::string invalid = ValidateOutgoingCot(ev);
  if (!invalid.empty()) {
    cb(CotSendReply{}, "invalid: " + invalid);
    return;
  }
  CotSendRequest req;
  req.kind = kind;
  req.targets = targets;
  req.client_id = cfg_.client_id;
  req.client_ref = ev.GetAttribute("uid");
  req.tlp = tlp;
  req.cot_xml = ev.ToXML(false);
  nc_->Request("svc.cot.send", req.ToJson(), [cb](const NatsMsg* m, const std::string& err) {
    if (!m) {
      cb(CotSendReply{}, err);
      return true;
    }
    CotSendReply r;
    if (!CotSendReply::Parse(m->data, r)) {
      cb(r, "bad reply");
      return true;
    }
    cb(r, r.ok ? std::string() : (r.error.empty() ? "rejected" : r.error));
    return true;
  });
}

}  // namespace om
