// SPDX-License-Identifier: GPL-3.0-or-later
// TAK contacts, chat and channels (feature C8); see TakContacts.h.
#include "OneMindCore/TakContacts.h"

#include "OneMindCore/Json.h"

#include <algorithm>

namespace om {

std::string TakContacts::Ingest(const std::string& xml, TimeMs now, const std::string& via) {
  CotInfo c;
  if (!ParseCot(xml, c)) return std::string();
  if (c.type.rfind("t-x-", 0) == 0) return std::string();
  if (c.uid == self_uid_) return std::string();  // our own SA echoed by a server
  if (c.is_chat) {
    CotEvent ev;
    if (!ev.PopulateFromXML(xml)) return std::string();
    std::string msg_id = ev.GetAttribute("detail\t__chat\tmessageId");
    std::string key = msg_id.empty() ? c.uid : msg_id;
    if (seen_msgs_.count(key)) return "chat";  // the same message over two paths
    seen_msgs_.insert(key);
    std::string room = ev.GetAttribute("detail\t__chat\tchatroom");
    std::string id = ev.GetAttribute("detail\t__chat\tid");
    std::string sender_uid = ev.GetAttribute("detail\t__chat\tchatgrp\tuid0");
    if (sender_uid.empty()) sender_uid = ev.GetAttribute("detail\tlink\tuid");
    if (sender_uid == self_uid_) return "chat";
    std::string conv_id, title;
    bool direct = false;
    if (id == kAllChatRooms || room == kAllChatRooms || id.empty()) {
      conv_id = title = kAllChatRooms;
    } else if (id == self_uid_) {  // a direct message to us: the conversation is the sender
      conv_id = sender_uid;
      title = c.chat_sender.empty() ? sender_uid : c.chat_sender;
      direct = true;
    } else {
      conv_id = id;
      title = room.empty() ? id : room;
    }
    ChatMessage m;
    m.message_id = key;
    m.conversation = conv_id;
    m.sender_uid = sender_uid;
    m.sender_callsign = c.chat_sender;
    m.text = c.remarks;
    m.time = c.time ? c.time : now;
    ChatConversation& cv = Conv(conv_id, title, direct);
    cv.messages.push_back(m);
    cv.unread++;
    if (cv.messages.size() > 500) cv.messages.erase(cv.messages.begin());
    return "chat";
  }
  if (c.callsign.empty() || !CotType::IsAtom(c.type)) return "other";
  // A contact card: an atom with <contact callsign> and either an endpoint or takv
  // (ATAK, WinTAK, iTAK and TAK Server relays all set one of the two).
  if (c.endpoint.empty() && !c.has_takv) return "other";
  TakContact& k = contacts_[c.uid];
  k.uid = c.uid;
  k.callsign = c.callsign;
  k.endpoint = c.endpoint;
  k.group_name = c.group_name;
  k.group_role = c.group_role;
  k.cot_type = c.type;
  k.takv_platform = c.takv_platform;
  k.takv_device = c.takv_device;
  k.takv_version = c.takv_version;
  k.point = c.point;
  k.last_seen = now;
  k.stale = c.stale > 0 ? c.stale : now + 120000;
  k.via = via;
  if (!c.group_name.empty()) heard_groups_.insert(c.group_name);
  if (convs_.count(c.uid)) convs_[c.uid].title = c.callsign;
  return "contact";
}

size_t TakContacts::Expire(TimeMs now) {
  size_t n = 0;
  for (auto it = contacts_.begin(); it != contacts_.end();) {
    if (it->second.stale <= now) {
      it = contacts_.erase(it);
      ++n;
    } else {
      ++it;
    }
  }
  return n;
}

bool TakContacts::ChannelVisible(const std::string& group_name) const {
  return active_.empty() || active_.count(group_name) != 0;
}

std::vector<const TakContact*> TakContacts::Visible() const {
  std::vector<const TakContact*> out;
  for (const auto& kv : contacts_)
    if (ChannelVisible(kv.second.group_name)) out.push_back(&kv.second);
  std::sort(out.begin(), out.end(), [](const TakContact* a, const TakContact* b) { return a->callsign < b->callsign; });
  return out;
}

const TakContact* TakContacts::Find(const std::string& uid) const {
  auto it = contacts_.find(uid);
  return it == contacts_.end() ? nullptr : &it->second;
}

std::vector<std::string> TakContacts::Channels() const {
  std::set<std::string> all = heard_groups_;
  all.insert(active_.begin(), active_.end());
  return std::vector<std::string>(all.begin(), all.end());
}

std::string TakContacts::ActiveGroupsRequestBody(const std::vector<std::string>& all_groups) const {
  Json arr = Json::MakeArray();
  for (const auto& g : all_groups) {
    Json o = Json::MakeObject();
    o.set("name", g);
    o.set("direction", "IN");
    o.set("active", ChannelVisible(g));
    arr.push_back(o);
  }
  return arr.Dump();
}

ChatConversation& TakContacts::Conv(const std::string& id, const std::string& title, bool direct) {
  ChatConversation& cv = convs_[id];
  if (cv.id.empty()) {
    cv.id = id;
    cv.direct = direct;
  }
  if (!title.empty()) cv.title = title;
  return cv;
}

CotEvent TakContacts::BuildChatTo(const std::string& conversation, const std::string& text, TimeMs now,
                                  bool for_server, const std::string& message_uid) {
  CotBuild b;
  // GeoChat uids are "GeoChat.{sender}.{room}.{messageId}" in ATAK; the builder takes the
  // part after the last '.' as messageId.
  b.uid = "GeoChat." + self_uid_ + "." + message_uid;
  b.callsign = self_callsign_;
  b.stale_s = 86400;
  b.simulated = true;
  const TakContact* to = Find(conversation);
  bool direct = to != nullptr;
  std::string room = direct ? to->callsign : conversation;
  CotEvent ev = BuildChat(b, self_uid_, room, text, now);
  if (direct) {
    ev.SetAttribute("detail\t__chat\tid", to->uid);
    ev.SetAttribute("detail\t__chat\tchatgrp\tuid1", to->uid);
    ev.SetAttribute("detail\t__chat\tchatgrp\tid", to->uid);
    ev.SetAttribute("detail\tremarks\tto", to->uid);
    if (for_server) ev.AddNode("detail\tmarti\tdest", {{"callsign", to->callsign}});
  }
  ChatMessage m;
  m.message_id = message_uid;
  m.conversation = direct ? to->uid : conversation;
  m.sender_uid = self_uid_;
  m.sender_callsign = self_callsign_;
  m.text = text;
  m.time = now;
  m.outgoing = true;
  seen_msgs_.insert(message_uid);
  Conv(m.conversation, direct ? to->callsign : conversation, direct).messages.push_back(m);
  return ev;
}

const ChatConversation* TakContacts::Conversation(const std::string& id) const {
  auto it = convs_.find(id);
  return it == convs_.end() ? nullptr : &it->second;
}

void TakContacts::MarkRead(const std::string& id) {
  auto it = convs_.find(id);
  if (it != convs_.end()) it->second.unread = 0;
}

}  // namespace om
