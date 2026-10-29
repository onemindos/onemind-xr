// Contacts, chat and channels (FEATURES C8) over GeoChat. Inbound chat and contacts are CoT XML
// on `tak.cot.out` (TAK Server, `COT` stream) and `tak.cot.mesh.rx` (mesh gateway, omos #551).
// Outbound chat is ephemeral, so it goes through `svc.cot.send` to the projector, the only
// publisher of `tak.cot.inject` and `tak.cot.mesh.tx` (cot-projection.md §9; BUS-DESIGN §3.4).
// GeoChat structure (`b-t-f`, `<__chat chatroom id senderCallsign>`, `<chatgrp uid0 uid1>`,
// `<remarks source to time>`) is from the gateway's test event (omos #551 gateway_test.go).
// A channel is a TAK group (`<__group name>`): switching channel changes which contacts and
// group chats are shown.
import { element, parseCot } from "./cot";

export const ALL_ROOM = "All Chat Rooms";

export interface Contact {
  uid: string;
  callsign: string;
  group?: string;
  role?: string;
  lat?: number;
  lon?: number;
  lastT: number;
  via: string;
}

export interface ChatMsg {
  uid: string;
  roomId: string;
  roomName: string;
  fromUid: string;
  fromCallsign: string;
  text: string;
  t: number;
  mine: boolean;
  /** delivery state for my own messages */
  state?: "sending" | "sent" | "failed";
  error?: string;
}

export interface Room {
  id: string;
  name: string;
  kind: "all" | "group" | "direct";
  unread: number;
}

const MAX_MSGS = 500;

export class ChatStore {
  private contacts = new Map<string, Contact>();
  private msgs: ChatMsg[] = [];
  private seen = new Set<string>();
  private groupRooms = new Set<string>();
  private read = new Map<string, number>();
  constructor(private selfUid: string, private now: () => number = () => Date.now()) {}

  /** Ingest one CoT XML message. Returns what it was. */
  ingest(xml: string, via: string): "chat" | "contact" | "other" {
    const ev = parseCot(xml);
    if (!ev) return "other";
    if (ev.type === "b-t-f" || ev.type.startsWith("b-t-f-")) {
      if (this.seen.has(ev.uid)) return "chat";
      const chat = element(ev.detail, "__chat");
      const rem = element(ev.detail, "remarks");
      if (!chat || !rem) return "other";
      const grp = element(ev.detail, "chatgrp");
      const roomName = chat.attrs.chatroom || ALL_ROOM;
      let roomId = chat.attrs.id || roomName;
      const fromUid = grp?.attrs.uid0 || (rem.attrs.source ?? "").replace(/^BAO\.F\.ATAK\./, "") || "unknown";
      // A direct message to me is filed under the sender, as ATAK does.
      if (roomId === this.selfUid) roomId = fromUid;
      this.seen.add(ev.uid);
      const t = Date.parse(rem.attrs.time ?? ev.time ?? "") || this.now();
      if (roomId !== ALL_ROOM && roomId !== fromUid && !this.contacts.has(roomId)) this.groupRooms.add(roomId);
      this.push({ uid: ev.uid, roomId, roomName: roomId === fromUid ? chat.attrs.senderCallsign || fromUid : roomName, fromUid, fromCallsign: chat.attrs.senderCallsign || fromUid, text: rem.text, t, mine: fromUid === this.selfUid });
      return "chat";
    }
    const c = element(ev.detail, "contact");
    if (c?.attrs.callsign && ev.type.startsWith("a-")) {
      const g = element(ev.detail, "__group");
      this.contacts.set(ev.uid, {
        uid: ev.uid,
        callsign: c.attrs.callsign,
        group: g?.attrs.name,
        role: g?.attrs.role,
        lat: ev.lat,
        lon: ev.lon,
        lastT: this.now(),
        via,
      });
      if (g?.attrs.name) this.groupRooms.add(g.attrs.name);
      return "contact";
    }
    return "other";
  }

  private push(m: ChatMsg): void {
    this.msgs.push(m);
    if (this.msgs.length > MAX_MSGS) this.msgs.splice(0, this.msgs.length - MAX_MSGS);
  }

  addOutgoing(room: Room, text: string, uid: string, callsign: string): ChatMsg {
    const m: ChatMsg = { uid, roomId: room.id, roomName: room.name, fromUid: this.selfUid, fromCallsign: callsign, text, t: this.now(), mine: true, state: "sending" };
    this.seen.add(uid);
    this.push(m);
    return m;
  }

  setState(uid: string, state: "sent" | "failed", error?: string): void {
    const m = this.msgs.find((x) => x.uid === uid);
    if (m) {
      m.state = state;
      m.error = error;
    }
  }

  channels(): string[] {
    const s = new Set<string>();
    for (const c of this.contacts.values()) if (c.group) s.add(c.group);
    return [...s].sort();
  }

  /** Contacts in a channel (all when channel is null). */
  contactList(channel: string | null): Contact[] {
    return [...this.contacts.values()].filter((c) => !channel || c.group === channel).sort((a, b) => a.callsign.localeCompare(b.callsign));
  }

  rooms(channel: string | null): Room[] {
    const out: Room[] = [{ id: ALL_ROOM, name: ALL_ROOM, kind: "all", unread: this.unread(ALL_ROOM) }];
    for (const g of [...this.groupRooms].sort()) if (!channel || g === channel) out.push({ id: g, name: g, kind: "group", unread: this.unread(g) });
    for (const c of this.contactList(channel)) out.push({ id: c.uid, name: c.callsign, kind: "direct", unread: this.unread(c.uid) });
    return out;
  }

  messages(roomId: string): ChatMsg[] {
    return this.msgs.filter((m) => m.roomId === roomId);
  }

  markRead(roomId: string): void {
    this.read.set(roomId, this.messages(roomId).length);
  }

  private unread(roomId: string): number {
    return Math.max(0, this.messages(roomId).filter((m) => !m.mine).length - (this.read.get(roomId) ?? 0));
  }
}

/**
 * Body of a `svc.cot.send` GeoChat request (owner proposal WEB-P4; cot-projection.md §9 leaves
 * the body to FEATURES C2). Structured fields, not client-written XML: the projector builds the
 * CoT, so a client cannot inject arbitrary CoT or spoof another sender's uid.
 */
export function geochatRequest(room: Room, text: string, me: { uid: string; callsign: string }, msgUid: string, target: "server" | "mesh" | "both" = "both") {
  return {
    v: 1,
    kind: "geochat",
    target,
    message_uid: msgUid,
    room: { id: room.id, name: room.name, kind: room.kind },
    text: text.slice(0, 1000),
    sender: me,
  };
}
