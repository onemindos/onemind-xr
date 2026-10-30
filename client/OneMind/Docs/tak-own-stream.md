# Proposal UE-P5: the client's own TAK stream (C3) as a named exception

Status: **proposed**, awaiting the owner (decision 0004 #8). To be added to the omos grammar
register (omos PR #556) by reference; this lane does not edit that PR. No new subject is
proposed here: the decision is about which CoT may reach TAK without passing the projector.

## Why a decision is needed

`docs/contracts/cot-projection.md` says:

- "The OneMind client never speaks TAK directly. It reads entities from the bus and sends CoT
  only through the projector" (header, lines 18-19);
- "Only the projector publishes `tak.cot.inject` (hub) and `tak.cot.mesh.tx` (site), so that
  every CoT that reaches TAK has passed the gates in §3" (§1 "Who may publish", lines 42-44).

Feature C3 ("the client appears as a TAK contact", `docs/FEATURES.md`) cannot be met that way: a
TAK contact is a client with its own connection to TAK Server (8089, mutual TLS, an enrolled
certificate) that sends its own SA. `UOneMindTakSubsystem` (`Plugins/OneMindCoT`) opens that
connection. The contract does not list it as an exception yet, so the owner has to ratify it.

## What is proposed

Add one row to the exception table in `cot-projection.md` §1 "Who may publish":

| Publisher | What it does | Fate |
|---|---|---|
| OneMind client, own TAK stream (C3, `UOneMindTakSubsystem`) | sends to TAK Server on 8089, with its own enrolled certificate, **only**: (1) its own SA (contact card: its own uid, `a-f-G-U-C`, `opex="s"` in this run), at most every 5 s; (2) GeoChat (`b-t-f`) typed by the human, to All Chat Rooms, a group room or one contact (`<marti><dest>`) | permanent while C3 exists; everything else goes through `svc.cot.send` |

And these rules for that stream:

1. Nothing else is sent on it: no entities, markers, shapes (B11), routes, geofences or SPI.
   Those go to `svc.cot.send`, so the projector's AOI, marking (G7) and rate gates apply.
2. Nothing received on it is republished to the bus (the bus stays the authority; already the
   case, `OneMindCore/TakStream.h`).
3. Every outgoing event passes `om::ValidateOutgoingCot` (already the case).
4. The TAK user is one per client device (proposal UE-P4), so TAK Server's own group
   filtering and audit apply to it.

Open question for the owner: chat on this stream carries no projector marking. If G7 marking
must apply to chat too, chat moves to `svc.cot.send` and the own stream carries SA only.

## What the code does until the owner decides

- SA and chat use the own stream (they are what C3 and C8 are).
- Drawn shapes go **only** through `svc.cot.send`. The copy over the own stream is behind
  `UOneMindTakSettings::bSendShapesOnOwnStream`, **off by default**
  (`Config/DefaultGame.ini` `[/Script/OneMindCoT.OneMindTakSettings]`). The host tests still send
  shapes over a test stream (`HostTests/scripts/e2e_c3_tak.py` part C) to check the TAK v1
  encoding of each shape; that is a codec test, not the production path.

If the owner rejects UE-P5, C3 cannot be delivered as written and FEATURES C3 needs changing.
