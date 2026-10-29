// Role modes (FEATURES J2). The selected mode only ever narrows what the UI offers; the
// permission comes from the policy service (OpenFGA through a dry run, policy.ts `roleProbe`),
// which stands in for the Authentik role until auth callout exists (omos #562 stage notes).
// Hiding a button is a convenience: the policy service still refuses an unauthorized proposal.
import type { Permission } from "./policy";

export type Role = "newcomer" | "observer" | "operator";
export const ROLES: Role[] = ["newcomer", "observer", "operator"];

export interface Abilities {
  /** see drawing tools and share drawings as CoT */
  draw: boolean;
  /** publish AOIs and geofences (`cmd.map.geofence.*`) */
  geofence: boolean;
  /** acknowledge alerts through policy (A1) */
  ackAlerts: boolean;
  /** see asset command buttons (goto, rtl, hold) */
  assetCommands: boolean;
  /** confirm pending actions (human confirm, asset-command.md §4 step 4) */
  confirm: boolean;
  /** chat over GeoChat */
  chat: boolean;
  /** ask Hermes */
  hermes: boolean;
}

const BY_ROLE: Record<Role, Abilities> = {
  newcomer: { draw: false, geofence: false, ackAlerts: false, assetCommands: false, confirm: false, chat: true, hermes: true },
  observer: { draw: true, geofence: true, ackAlerts: true, assetCommands: false, confirm: false, chat: true, hermes: true },
  operator: { draw: true, geofence: true, ackAlerts: true, assetCommands: true, confirm: true, chat: true, hermes: true },
};

export const ROLE_TEXT: Record<Role, string> = {
  newcomer: "Newcomer: view, chat and ask Hermes. No drawing, no asset commands.",
  observer: "Observer: draw, share AOIs and acknowledge alerts. No asset commands.",
  operator: "Operator: task assets and confirm actions, when the policy service says you operate the asset.",
};

/**
 * The role the UI acts on: the selected one, but never operator unless the policy service
 * granted it. With no answer yet (or the service down), operator falls back to observer.
 */
export function effectiveRole(selected: Role, permission: Permission): Role {
  if (selected === "operator" && permission !== "operator") return "observer";
  return selected;
}

export function abilities(role: Role): Abilities {
  return BY_ROLE[role];
}

export function parseRole(v: string | null | undefined): Role {
  return v === "observer" || v === "operator" ? v : "newcomer";
}
