// Cursor-on-Target for the OneMind client (OneMindCoT, features C1, C2, B6).
//
// CotEvent is a port of VR-TAK's UVH_COTEvent (VH_COT plugin) to engine-independent
// C++: the same tab-separated path API with keyword aliases ("callsign" ->
// detail/contact/callsign), Get/Set attribute, AddNode, GetNodes, RemoveNodes,
// HasNode, GetPoint/SetPoint, PopulateTime and ToXML, built on om::XmlNode instead of
// the engine's patched FXmlFile.
//   Derived from TAK-Product-Center/VR, Game/Plugins/VH_COT/Source/VH_COT/
//   {Public,Private}/VH_COTEvent.{h,cpp} at 4bd384978b48b0a46e7f17b272933a152a61c013
//   (branch main, GPL-3.0; authors "Virtual Heroes, a division of ARA").
//   Modified by OneMind on 2026-10-29: rewritten without Unreal types, XML DOM and
//   time handling replaced, typed parse and builders added. See THIRD_PARTY.md.
//
// The client never speaks TAK directly: it reads entities from the bus and sends only
// through `svc.cot.send` (cot-projection.md §9). Parsing here serves the inspector's
// raw-CoT view, local validation, and building the CoT that goes into svc.cot.send.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OneMindCore/Json.h"
#include "OneMindCore/Xml.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace om {

struct GeoPoint {
  double lat = 0, lon = 0, hae = 9999999.0;
};

/// CoT time: "YYYY-MM-DDThh:mm:ss[.sss]Z" <-> epoch ms (UTC).
OM_CORE_API std::string CotTime(TimeMs ms);
OM_CORE_API bool ParseCotTime(const std::string& s, TimeMs& out);

class OM_CORE_API CotEvent {
 public:
  /// A new event: <event version="2.0" how="h-g-i-g-o"/> (UVH_COTEvent::Initialize).
  CotEvent();
  CotEvent(const CotEvent& o);
  CotEvent& operator=(const CotEvent& o);

  bool PopulateFromXML(const std::string& xml, std::string* err = nullptr);
  std::string ToXML(bool declaration = false) const;

  /// Paths are tab-separated from the <event> element, e.g. "point\tlat"; a final
  /// segment names an attribute. A single-token path is an attribute of <event>.
  /// Keyword aliases: callsign, endpoint, groupName, groupRole, archive, latitude,
  /// longitude, hae, link, fillColor, strokeColor (VH_COTEvent.cpp MakeXmlKeywords).
  bool TryGetAttribute(const std::string& path, std::string& value) const;
  std::string GetAttribute(const std::string& path) const;
  double GetAttributeAsDouble(const std::string& path, double def = 0) const;
  void SetAttribute(const std::string& path, const std::string& value);
  void SetAttribute(const std::string& path, double value);
  std::string GetNodeContent(const std::string& path) const;
  bool HasNode(const std::string& path) const;
  void RemoveNodes(const std::string& path);
  /// Appends (or with replace=true, updates) the element at `path`.
  XmlNode* AddNode(const std::string& path, const std::vector<std::pair<std::string, std::string>>& attrs = {},
                   bool replace = false, const std::string& content = std::string());
  std::vector<XmlNode*> GetNodes(const std::string& path) const;

  GeoPoint GetPoint() const;
  /// Sets lat/lon/hae; ce/le default to unknown (9999999), as VR-TAK does.
  void SetPoint(const GeoPoint& p, double ce = 9999999.0, double le = 9999999.0);
  /// time = start = now, stale = now + stale_s.
  void PopulateTime(TimeMs now, int stale_s);

  XmlNode* root() { return root_.get(); }
  const XmlNode* root() const { return root_.get(); }

 private:
  static std::vector<std::string> Resolve(const std::string& path);
  std::unique_ptr<XmlNode> root_;
};

/// Typed view of the fields the client uses (contract cot-projection.md §8.3).
struct OM_CORE_API CotInfo {
  std::string uid, type, how, access, opex;
  TimeMs time = 0, start = 0, stale = 0;
  GeoPoint point;
  double ce = 9999999.0, le = 9999999.0;
  std::string callsign, endpoint, group_name, group_role, remarks;
  double course = -1, speed = -1;
  int battery = -1;
  bool has_takv = false;
  std::string takv_platform, takv_device, takv_version;
  std::vector<GeoPoint> link_points;  // <link point="lat,lon[,hae]"/> vertices
  std::vector<std::string> link_uids;
  bool is_chat = false;
  std::string chat_room, chat_sender;
  bool flow_tag_projector = false;
};

OM_CORE_API bool ParseCot(const std::string& xml, CotInfo& out, std::string* err = nullptr);

/// CoT type helpers (MIL-STD-2525 atoms, cot-projection.md §4.2).
struct OM_CORE_API CotType {
  static bool IsAtom(const std::string& type) { return type.size() >= 3 && type[0] == 'a' && type[1] == '-'; }
  /// 'f','h','u','n','s','j','k','a','p','o','x' or 0.
  static char Affiliation(const std::string& type);
  /// 'P' space, 'A' air, 'G' ground, 'S' sea surface, 'U' subsurface, 'F' SOF, or 0.
  static char Dimension(const std::string& type);
  /// Contract dispositions: friendly assumed_friendly neutral unknown suspicious pending.
  /// Hostile affiliations map to "suspicious" (no hostile in OneMind, entity.md §4.1).
  static std::string Disposition(const std::string& type);
  static std::string Environment(const std::string& type);
};

/// Reverse-path drop rules (cot-projection.md §8.2). Returns the reason or "".
OM_CORE_API std::string CotDropReason(const CotInfo& c);

/// entity_id for a TAK uid (entity.md §3, cot-projection.md §8.3): "tak-" + uid
/// lowercased if it fits [a-z0-9_-], else "tak-" + base32(sha256(uid))[0:26].
OM_CORE_API std::string TakEntityId(const std::string& cot_uid);

// --- Builders for svc.cot.send (feature C2) -----------------------------------------

struct OM_CORE_API CotBuild {
  std::string uid;       // client-chosen; never starts with "OM." or "INTEL." (reserved)
  std::string callsign;  // marker label
  std::string remarks;
  int stale_s = 300;
  bool simulated = true;  // run rule: everything in this run is simulated -> opex="s"
};

OM_CORE_API CotEvent BuildMarker(const CotBuild& b, const std::string& cot_type, const GeoPoint& p, TimeMs now);
/// Spot point of interest b-m-p-s-p-i (in CoTtypes.xml, cot-projection.md §9).
OM_CORE_API CotEvent BuildSpi(const CotBuild& b, const GeoPoint& p, TimeMs now);
/// Route b-m-r with b-m-p-w waypoint links (cot-projection.md §5.5).
OM_CORE_API CotEvent BuildRoute(const CotBuild& b, const std::vector<GeoPoint>& wps, TimeMs now);
/// Free-form polygon u-d-f, closed; kind sets the colour (cot-projection.md §5.5 table).
OM_CORE_API CotEvent BuildGeofence(const CotBuild& b, const std::vector<GeoPoint>& ring, const std::string& kind,
                                   TimeMs now);
/// GeoChat b-t-f to a room (default "All Chat Rooms"). Element layout follows common
/// ATAK GeoChat; marked unverified in cot-projection.md §13.
OM_CORE_API CotEvent BuildChat(const CotBuild& b, const std::string& sender_uid, const std::string& room,
                               const std::string& text, TimeMs now);

/// Validates a CoT the client intends to send: uid prefix rules, no hostile atoms,
/// no tasking types, size. Returns "" when acceptable.
OM_CORE_API std::string ValidateOutgoingCot(const CotEvent& ev);

/// Request body for `svc.cot.send` v1 (proposed in client/OneMind/Docs/svc-cot-send.md).
struct OM_CORE_API CotSendRequest {
  std::string kind;                                // marker | spi | route | geofence | chat
  std::vector<std::string> targets{"server", "mesh"};
  std::string client_id;                           // the sending client instance
  std::string client_ref;                          // idempotency key for retries
  std::string tlp = "CLEAR";
  std::string dest_uid;                            // optional directed send (mesh Tak-Dest-Uid)
  std::string cot_xml;
  std::string ToJson() const;
};

struct OM_CORE_API CotSendReply {
  bool ok = false;
  std::string uid, error, detail;
  std::vector<std::string> targets;
  static bool Parse(const std::string& body, CotSendReply& out);
};

}  // namespace om
