// RFC 8785 JSON Canonicalization Scheme (https://www.rfc-editor.org/rfc/rfc8785) and the policy
// `params_hash` (docs/contracts/asset-command.md §6.1): SHA-256 over the JCS form of
// `{action_type, action_version, target, params}`, as the policy service computes it (omos #554
// `internal/policy/jcs.go` `ParamsHash`). The client recomputes it to check that the parameters it
// shows the confirmer are the ones the server's hash binds.
//
// JCS serialises numbers and strings exactly as ECMAScript JSON.stringify does, and sorts object
// keys by UTF-16 code units, which is JavaScript's default string order.

export function canonicalize(v: unknown): string {
  if (v === null || typeof v === "boolean" || typeof v === "string") return JSON.stringify(v);
  if (typeof v === "number") {
    if (!Number.isFinite(v)) throw new Error("jcs: NaN or Infinity is not valid JSON");
    return JSON.stringify(v); // ES6 Number.prototype.toString form; -0 becomes "0"
  }
  if (Array.isArray(v)) return `[${v.map((x) => canonicalize(x === undefined ? null : x)).join(",")}]`;
  if (typeof v === "object") {
    const o = v as Record<string, unknown>;
    const keys = Object.keys(o)
      .filter((k) => o[k] !== undefined)
      .sort();
    return `{${keys.map((k) => `${JSON.stringify(k)}:${canonicalize(o[k])}`).join(",")}}`;
  }
  throw new Error(`jcs: unsupported type ${typeof v}`);
}

export interface HashInput {
  action_type: string;
  action_version: number;
  target: { entity_type: string; entity_id: string };
  params: Record<string, unknown>;
}

/** The document the hash covers; the target carries only its two fields, as the Go struct does. */
export function hashDocument(a: HashInput): string {
  return canonicalize({
    action_type: a.action_type,
    action_version: a.action_version,
    target: { entity_type: a.target.entity_type, entity_id: a.target.entity_id },
    params: a.params ?? {},
  });
}

/** True where Web Crypto digests exist (a secure context: https or localhost). */
export function canHash(): boolean {
  return typeof globalThis.crypto?.subtle?.digest === "function";
}

/** `sha256:<hex>` of the JCS document, or null without Web Crypto. */
export async function paramsHash(a: HashInput): Promise<string | null> {
  if (!canHash()) return null;
  const d = await globalThis.crypto.subtle.digest("SHA-256", new TextEncoder().encode(hashDocument(a)));
  return `sha256:${[...new Uint8Array(d)].map((b) => b.toString(16).padStart(2, "0")).join("")}`;
}
