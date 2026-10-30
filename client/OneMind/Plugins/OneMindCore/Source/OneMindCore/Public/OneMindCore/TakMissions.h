// TAK Data Sync missions in the client (feature C7), through omos tak-client (A21, PR #560).
//
// The client never calls TAK Server's Marti REST API itself for missions: tak-client holds
// the TAK identity and mission tokens, and serves a small NATS RPC surface. tak-client 0.x
// does not implement it yet ("Not in this version: Data Sync missions (FEATURES C7)", its
// README), so the surface is proposed in client/OneMind/Docs/svc-tak-mission.md (owner
// proposal UE-P2) and the client is tested against a stub that follows it.
//
//   svc.tak.mission.list         {v}                         -> {v, missions[]}
//   svc.tak.mission.get          {v, name}                   -> {v, mission}
//   svc.tak.mission.changes      {v, name, since_ms?}        -> {v, changes[]}
//   svc.tak.mission.logs         {v, name}                   -> {v, logs[]}
//   svc.tak.mission.subscribe    {v, name, client_uid}       -> {v, ok}
//   svc.tak.mission.unsubscribe  {v, name, client_uid}       -> {v, ok}
//   svc.tak.mission.content.put  {v, name, client_uid, cot}  -> {v, ok, uid}
//   evt.mission.tak-{token}.changed  (event envelope v1, data {name, token, change})
//
// Grammar: svc.{service}.{endpoint} with a dotted endpoint (as svc.policy.command.propose);
// evt.{domain}.{entity}.{event} with entity "tak-{token}", so TAK missions never collide
// with OneMind's own msn-/tsk- ids on evt.mission.* (mission-task.md §1). tak-client maps
// these onto MissionApi (/Marti/api/missions/{name}, /subscription, /changes, /contents,
// logs) and the t-x-m-c change CoT (tak-protocols.md §5).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/CoT.h"
#include "OneMindCore/NatsClient.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace om {

/// "tak-" + lowercased name if it fits [a-z0-9_-] and is at most 40 characters, else
/// "tak-" + base32(sha256(name))[0:26] (same rule as TakEntityId).
OM_CORE_API std::string TakMissionToken(const std::string& name);

struct OM_CORE_API TakMissionSummary {
  std::string name, token, description, creator_uid;
  TimeMs create_time = 0;
  bool subscribed = false;
  int uid_count = 0, content_count = 0;
};

struct OM_CORE_API TakMissionUid {
  std::string uid, cot_type, callsign, creator_uid;
  GeoPoint point;
  TimeMs timestamp = 0;
};

struct OM_CORE_API TakMissionContent {
  std::string hash, filename, mime_type, submitter;
  int64_t size = 0;
  TimeMs timestamp = 0;
};

struct OM_CORE_API TakMissionChange {
  std::string type;  // ADD_CONTENT, REMOVE_CONTENT, CREATE_MISSION, DELETE_MISSION, ...
  std::string uid, hash, callsign, cot_type, creator_uid;
  TimeMs timestamp = 0;
};

struct OM_CORE_API TakMissionLog {
  std::string id, content, creator_uid;
  TimeMs timestamp = 0;
};

struct OM_CORE_API TakMissionView {
  TakMissionSummary summary;
  std::vector<TakMissionUid> uids;
  std::vector<TakMissionContent> contents;
  std::vector<TakMissionChange> changes;
  std::vector<TakMissionLog> logs;
  TimeMs refreshed_at = 0;
  bool loading = false, dirty = false;
};

class OM_CORE_API TakMissionClient {
 public:
  using Done = std::function<void(const std::string& error)>;
  TakMissionClient(NatsClient& nc, std::string client_uid, int timeout_ms = 5000);
  ~TakMissionClient();

  /// Subscribes to evt.mission.*.changed and keeps open missions current.
  void Watch();
  void List(Done done);
  /// Loads the mission (contents, changes, logs) into Views() and keeps it current.
  void Open(const std::string& name, Done done);
  void Close(const std::string& name);
  void Subscribe(const std::string& name, Done done);
  void Unsubscribe(const std::string& name, Done done);
  /// Publishes one feature (CoT built by the client) into the mission.
  void PutFeature(const std::string& name, const CotEvent& ev, Done done);

  const std::vector<TakMissionSummary>& Missions() const { return missions_; }
  const std::map<std::string, TakMissionView>& Views() const { return views_; }
  const TakMissionView* View(const std::string& name) const;
  uint64_t change_events() const { return change_events_; }

  // Parsers (public for tests)
  static bool ParseSummary(const Json& j, TakMissionSummary& out);
  static bool ParseMission(const Json& j, TakMissionView& out);
  static void ParseChange(const Json& j, TakMissionChange& out);

 private:
  void Call(const std::string& endpoint, const Json& body, std::function<void(const Json&, const std::string&)> cb);
  void Refresh(const std::string& name);
  NatsClient& nc_;
  std::string client_uid_;
  int timeout_ms_;
  int64_t sid_ = 0;
  std::vector<TakMissionSummary> missions_;
  std::map<std::string, TakMissionView> views_;
  std::map<std::string, std::string> token_to_name_;
  uint64_t change_events_ = 0;
  std::shared_ptr<bool> alive_;
};

}  // namespace om
