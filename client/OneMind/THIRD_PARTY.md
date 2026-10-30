# Third-party code and notices (client/OneMind)

The OneMind client is GPL-3.0-or-later (repository `LICENSE`, decision 0001).

## Ported code: VR-TAK (GPL-3.0)

| Item | Value |
|---|---|
| Origin | TAK Product Center, VR-TAK, https://github.com/TAK-Product-Center/VR |
| Branch and commit read | `main` @ `4bd384978b48b0a46e7f17b272933a152a61c013` (5.4-RELEASE-7) |
| Licence | GPL-3.0. `LICENSE` at that commit begins "GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007" (fetched with `gh api repos/TAK-Product-Center/VR/contents/LICENSE?ref=4bd38497...` on 2026-10-29). The `upstream/5.5-RELEASE-0` branch has no `LICENSE` file ([vrtak-porting-map.md](../../docs/research/vrtak-porting-map.md) S11), so the port is taken from `main`, where the grant is |
| Authors | "Virtual Heroes, a division of ARA" (`Game/Plugins/VH_COT/VH_COT.uplugin`, `CreatedBy`) |
| Files ported from | `Game/Plugins/VH_COT/Source/VH_COT/Public/VH_COTEvent.h`, `Private/VH_COTEvent.cpp`, `Private/VH_MilStd2525.cpp` (and its header) |
| Ported into | `Plugins/OneMindCore/Source/OneMindCore/{Public/OneMindCore,Private}/CoT.{h,cpp}` (class `om::CotEvent`), `MilStd2525.{h,cpp}` |
| What was ported | The CoT event model API (tab-separated paths from `<event>`, the keyword alias table `MakeXmlKeywords`, Get/Set attribute, AddNode with replace, GetNodes, RemoveNodes, HasNode, GetNodeContent, GetPoint/SetPoint with unknown ce/le 9999999, PopulateTime, ToXML, default `how="h-g-i-g-o"`); the MIL-STD-2525 icon-name to CoT-type conversion, the CoT-type to id conversion and the parent/child id hierarchy |
| Modified by OneMind | 2026-10-29: rewritten in engine-independent C++17 (no `UObject`, `FXmlFile`, `FString`, `FVariant`); XML DOM, time handling and number formatting replaced; typed parse (`om::ParseCot`), drop rules, builders and send validation added; icon textures, `symbols.dat` loading, archive extraction and Spatialite are not ported |
| Not copied | `EnginePatch/**` (Epic source), `ThirdParty/**` (Commo, Gv2F), prebuilt DLLs, `.uasset` content, `VH_COTChat` (it is a Spatialite record, not portable logic) |

Each ported file carries a header naming the origin, the commit and the modification date
(GPL-3.0 §5(a)).

## Design references only (no code copied)

- Cesium for Unreal (Apache-2.0): used as an engine plugin from Fab; API names checked against
  headers at tag `v2.30.0` (`Cesium3DTileset.h`, `CesiumGeoreference.h`,
  `CesiumUrlTemplateRasterOverlay.h`, `CesiumRuntimeSettings.h`, `GlobeAwareDefaultPawn.h`,
  `CesiumSunSky.h`). The horizon test in `om::Geo::AboveHorizon` follows the published
  ellipsoidal-occluder method (scaled space); it is written from the description, not copied.
- NATS protocol, JetStream API and ADR-4 headers: implemented from the public protocol documentation
  (https://docs.nats.io/reference/reference-protocols/nats-protocol). No nats.c code is included.
- RFC 6455 (WebSocket), RFC 4648 (Base64/Base32), FIPS 180-4 (SHA-1, SHA-256): implemented from the
  standards. `DaysFromCivil`/`CivilFromDays` follow Howard Hinnant's public-domain date algorithms.
- worldmonitor (AGPL-3.0), worldwideview (Elastic 2.0) and CloudTAK (AGPL-3.0): not read for this
  code and nothing copied (decision 0004 #15).

## Map data shown by default

- OpenStreetMap standard tiles (`https://tile.openstreetmap.org/{z}/{x}/{y}.png`): data ODbL,
  "(c) OpenStreetMap contributors" shown on screen; the OSM tile usage policy applies (identifying
  User-Agent, no bulk download). Configurable in `Config/DefaultGame.ini`. No Cesium ion and no
  Google Photorealistic 3D Tiles (decision 0004 #4).
