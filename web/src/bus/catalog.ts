// Default layer catalog for L0. Ids, entity types and tiers come from
// docs/contracts/layer-snapshot.md §1 (marked provisional there). The `alerts` layer is a
// client-side grouping of `ent.alert.*` (docs/design/ONTOLOGY.md §3, alert row); the layer service
// does not list it yet, so it is marked `clientDefined` until the layer catalog includes it.

export type Tier = "small" | "medium" | "large" | "static";

export interface LayerDef {
  id: string;
  title: string;
  entityTypes: string[];
  tier: Tier;
  defaultOn: boolean;
  clientDefined?: boolean;
}

export const DEFAULT_LAYERS: LayerDef[] = [
  { id: "fleet", title: "Own fleet", entityTypes: ["drone", "robot", "vehicle", "node"], tier: "small", defaultOn: true },
  { id: "tracks", title: "Tracks", entityTypes: ["track"], tier: "medium", defaultOn: true },
  { id: "zones", title: "Zones and AOIs", entityTypes: ["zone"], tier: "small", defaultOn: true },
  { id: "alerts", title: "Alerts", entityTypes: ["alert"], tier: "small", defaultOn: true, clientDefined: true },
  { id: "tak", title: "TAK", entityTypes: ["person", "place"], tier: "small", defaultOn: true },
  { id: "hazards", title: "Hazards", entityTypes: ["hazard", "event"], tier: "medium", defaultOn: true },
  { id: "air", title: "Aircraft", entityTypes: ["aircraft"], tier: "large", defaultOn: false },
  { id: "sea", title: "Vessels", entityTypes: ["vessel"], tier: "large", defaultOn: false },
];

/** Subject for one entity type on the interim/direct path (entity.md §1; locked grammar). */
export function entSubject(entityType: string): string {
  return `ent.${entityType}.*.state`;
}
