// Click-to-task with a human confirm through the policy service (feature D3).
//
// Contract: docs/contracts/asset-command.md (§1 subjects, §2 ownership, §4 flow, §6 bodies)
// and the omos policy service (PR #554, services/policy/README.md). The client:
//  - never publishes cmd.*: it only requests svc.policy.command.propose and
//    svc.policy.command.confirm, and watches evt.so.action.> for the outcome (§2 "Client");
//  - checks locally before proposing (sim-* target, verb in the closed list, class), which
//    only saves a round trip: the policy service is the authority and checks everything again;
//  - recomputes params_hash itself (SHA-256 of the RFC 8785 JCS form of {action_type,
//    action_version, target, params}, §5, as services/policy/internal/policy/jcs.go does) and
//    refuses to confirm when the policy's hash differs, so the human approves exactly the
//    parameters on the screen;
//  - confirms only through Confirm(), which the UI calls from the human's button press after
//    showing the parameters and hash (MarkShown, called once the dialog has been rendered). An
//    approval is refused until the dialog has been on screen for kMinShownMs, so a key press in
//    the frame the propose reply lands cannot approve parameters nobody has seen. An expired or
//    already decided action cannot be confirmed. The policy service decides whether the caller is a human (asset-command.md
//    §4 step 4); agents use their own path and are refused there.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/CoT.h"
#include "OneMindCore/Json.h"
#include "OneMindCore/NatsClient.h"

#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace om {

/// RFC 8785 JSON Canonicalization Scheme for the client's own JSON values.
OM_CORE_API std::string JcsCanonical(const Json& v);
/// ECMAScript Number.prototype.toString form of a double (RFC 8785 §3.2.2.3).
OM_CORE_API std::string JcsNumber(double v);
/// "sha256:" + hex SHA-256 of JCS({action_type, action_version, target, params}).
OM_CORE_API std::string ParamsHash(const std::string& action_type, int action_version, const Json& target,
                                   const Json& params);

/// The asset verbs of asset-command.md §3 (closed list) and their levels.
OM_CORE_API bool IsAssetVerb(const std::string& verb);
OM_CORE_API std::string AssetVerbLevel(const std::string& verb);

struct OM_CORE_API TaskDraft {
  std::string asset_id;              // must start "sim-" in this run
  std::string asset_class = "drone"; // drone | robot | vehicle
  std::string verb = "goto";
  Json params = Json::MakeObject();  // e.g. {lat, lon, hae_m, speed_mps}
  std::string reason;
};

/// Draft for "fly to the clicked point" (asset.goto, asset-command.md §3).
OM_CORE_API TaskDraft GotoDraft(const std::string& asset_id, const std::string& asset_class, const GeoPoint& clicked,
                                double hae_m, double speed_mps, const std::string& reason);

struct OM_CORE_API ActionEvent {
  std::string event;  // proposed, denied, approved, rejected, expired, dispatched, acknowledged, executed, failed, refused
  std::string code, message, command_id;
  TimeMs at = 0;
};

struct OM_CORE_API PendingAction {
  std::string request_id, action_id, action_type, status;  // status: proposing, awaiting_confirmation, denied, confirming,
                                                           // approved, dispatched, executed, failed, refused, rejected, expired
  std::string level, params_hash, local_hash;
  std::string denial_code, denial_message;
  TimeMs confirm_by = 0;
  int approvals_needed = 0;
  bool shown = false;  // the UI displayed parameters and hash to the human
  TimeMs shown_at = 0; // when it was first displayed
  TaskDraft draft;
  std::vector<ActionEvent> timeline;
  bool Final() const;
};

class OM_CORE_API AssetTasking {
 public:
  using Done = std::function<void(const PendingAction& action, const std::string& error)>;
  AssetTasking(NatsClient& nc, std::string user_id, int timeout_ms = 5000);
  ~AssetTasking();

  /// Subscribes evt.so.action.> (filtered to this client's actions).
  void Watch();
  /// Local checks, then svc.policy.command.propose. Returns the request id, or "" with
  /// the reason in `done` when a local check failed.
  std::string Propose(const TaskDraft& d, TimeMs now, Done done);
  /// Minimum time the dialog must have been displayed before an approval is accepted.
  static constexpr TimeMs kMinShownMs = 500;
  /// The UI calls this once the confirm dialog has displayed the parameters and the hash
  /// (after rendering it, not when building it). Only the first call counts.
  void MarkShown(const std::string& action_id, TimeMs now);
  /// The human's decision (approve=true) or rejection with a reason.
  void Confirm(const std::string& action_id, bool approve, const std::string& reason, TimeMs now, Done done);
  void Tick(TimeMs now);

  const PendingAction* Find(const std::string& action_id) const;
  const std::map<std::string, PendingAction>& actions() const { return actions_; }

  /// The ActionRequest body (asset-command.md §6.1) for a draft.
  Json BuildRequest(const TaskDraft& d, const std::string& request_id) const;
  /// "" or the reason a draft is refused before it reaches policy.
  static std::string PreCheck(const TaskDraft& d);

 private:
  void OnEvent(const NatsMsg& m);
  static void Apply(PendingAction& a, const ActionEvent& e);
  NatsClient& nc_;
  std::string user_id_;
  int timeout_ms_;
  int64_t sid_ = 0;
  std::map<std::string, PendingAction> actions_;      // by action_id (or request id while proposing)
  // evt.so.action.* that arrived before the propose reply named the action (the policy
  // service publishes "proposed" before it replies); replayed when the reply lands.
  std::deque<std::pair<std::string, ActionEvent>> early_;
  std::shared_ptr<bool> alive_;
  TimeMs now_ = 0;
};

/// UUID v4 text from the core's token generator (idempotency keys, not security).
OM_CORE_API std::string NewUuid();

}  // namespace om
