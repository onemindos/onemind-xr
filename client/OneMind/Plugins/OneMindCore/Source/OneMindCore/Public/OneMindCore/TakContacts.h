// TAK contacts, chat and channels for the client (feature C8).
//
// Contacts are learned from received SA that carries a <contact> card (the way Common
// Commo discovers contacts, atak-civ commoncommo/core/contact-rxtx-rules.txt, tak-protocols.md
// S4): uid, callsign, endpoint, team (<__group>), takv and position. A contact drops off the
// list at its CoT stale time.
//
// Chat is GeoChat (b-t-f) as in the existing builder (CoT.h BuildChat, element layout from
// the atak-civ takcot Chat.xsd: __chat{parent, groupOwner, messageId, chatroom, id,
// senderCallsign} with chatgrp{uid0, uid1, id}, link, remarks{source, to, time}).
// Conversations:
//   "All Chat Rooms"         group chat for everyone (chatroom = id = "All Chat Rooms");
//   direct chat with a uid   chatroom = the recipient's callsign, id = the recipient uid,
//                            chatgrp uid0 = sender, uid1 = recipient; on a server stream a
//                            <marti><dest callsign=.../></marti> asks TAK Server to deliver it
//                            only to that contact (tak-protocols.md S4: directed sends through a
//                            server add marti/dest). Element values for direct chat follow
//                            common ATAK behaviour and are unverified until the owner's ATAK check;
//   any other room id        a named group room.
//
// Channels: in TAK a "channel" is a TAK Server group; the server sends a client only the CoT
// of its active groups (tak-protocols.md §5 GroupsApi/SubscriptionApi `PUT /groups/active`).
// The client keeps the active channel set, filters what it shows by the sender's group
// (<__group name>, the team on the mesh, where there is no server), and builds the server
// request for the active-group change. The request body is unverified against 5.8 and is the
// owner's check.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/CoT.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace om {

struct OM_CORE_API TakContact {
  std::string uid, callsign, endpoint, group_name, group_role, cot_type;
  std::string takv_platform, takv_device, takv_version;
  GeoPoint point;
  TimeMs last_seen = 0, stale = 0;
  std::string via;  // "stream", "mesh", "bus"
};

struct OM_CORE_API ChatMessage {
  std::string message_id, conversation, sender_uid, sender_callsign, text;
  TimeMs time = 0;
  bool outgoing = false;
};

struct OM_CORE_API ChatConversation {
  std::string id;     // "All Chat Rooms", a contact uid (direct), or a room id
  std::string title;  // room name or contact callsign
  bool direct = false;
  std::vector<ChatMessage> messages;
  size_t unread = 0;
};

class OM_CORE_API TakContacts {
 public:
  static constexpr const char* kAllChatRooms = "All Chat Rooms";

  explicit TakContacts(std::string self_uid, std::string self_callsign)
      : self_uid_(std::move(self_uid)), self_callsign_(std::move(self_callsign)) {}

  /// Feeds one received CoT. Returns "contact", "chat", "other" or "" (ignored: own echo,
  /// stream control, malformed).
  std::string Ingest(const std::string& xml, TimeMs now, const std::string& via);
  /// Drops contacts past their stale time. Returns how many were removed.
  size_t Expire(TimeMs now);

  /// Contacts visible under the active channels, sorted by callsign.
  std::vector<const TakContact*> Visible() const;
  const TakContact* Find(const std::string& uid) const;
  size_t size() const { return contacts_.size(); }

  // --- channels -------------------------------------------------------------------------
  /// Every group name heard so far plus the active ones.
  std::vector<std::string> Channels() const;
  /// Empty set = all channels visible.
  void SetActiveChannels(const std::set<std::string>& names) { active_ = names; }
  const std::set<std::string>& ActiveChannels() const { return active_; }
  bool ChannelVisible(const std::string& group_name) const;
  /// JSON body for TAK Server `PUT /Marti/api/groups/active?clientUid=` (unverified shape:
  /// a list of {name, direction, active}; owner check against the live server).
  std::string ActiveGroupsRequestBody(const std::vector<std::string>& all_groups) const;

  // --- chat -----------------------------------------------------------------------------
  /// Builds a chat to a conversation id (kAllChatRooms, a contact uid or a room id) and
  /// records it as outgoing. `for_server` adds the marti/dest element for direct chat.
  CotEvent BuildChatTo(const std::string& conversation, const std::string& text, TimeMs now, bool for_server,
                       const std::string& message_uid);
  const std::map<std::string, ChatConversation>& Conversations() const { return convs_; }
  const ChatConversation* Conversation(const std::string& id) const;
  void MarkRead(const std::string& id);

 private:
  ChatConversation& Conv(const std::string& id, const std::string& title, bool direct);
  std::string self_uid_, self_callsign_;
  std::map<std::string, TakContact> contacts_;
  std::map<std::string, ChatConversation> convs_;
  std::set<std::string> seen_msgs_;
  std::set<std::string> active_;
  std::set<std::string> heard_groups_;
};

}  // namespace om
