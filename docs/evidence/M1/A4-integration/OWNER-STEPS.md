# A4 integration (PR #552): owner steps

Nothing here needs the owner's Mac, headset or ATAK device to test: all checks ran locally with a
disposable nats-server. These steps are for after the owner syncs the entity service (manual
Argo sync of `platform-omos-entity`, already described in #552) and #558's grammar pipelines.

## 1. Satellites (FEATURES A12) once `feed-satellites-grammar` publishes TLEs

```bash
nats --context omos stream subjects INTEL 'intel.space.celestrak.tle'      # expect 1 subject, recent
nats --context omos sub 'ent.vehicle.norad-25544.state' --count 3           # one record about every 30 s
```

Expected: `kind: satellite`, `environment: space`, `location.measured_t` advancing by about
30 000 ms, `location.hae_m` 400 000-430 000. Compare one record's lat/lon at `measured_t` with
an independent tracker (for example `https://celestrak.org/` "Orbit visualisation" or
python-sgp4 with the same TLE): within 5 km.

## 2. MTA trains once BUS-P6 (#558 A1-P1) is ruled and `feed-mta-grammar` runs

```bash
nats --context omos sub 'ent.vehicle.*.state' --count 50 | grep -A2 'ent.vehicle.gtfs-mnr-'   # NATS wildcards match whole tokens only
```

Expected: one entity per train; during the parallel run the legacy `MTA-MNR-*` CoT and the
`evt.world.rail.mta-mnr-*` envelope both appear under `sources[]` of the same entity (not two
entities).

## 3. omos-ingest NWS (#553) through the entity service

```bash
nats --context omos sub 'ent.event.*.state' --count 50 | grep -A2 'ent.event.noaa-'
```

Expected: only alerts with a polygon become entities (callsign = NWS event, e.g. "Flood
Warning"; `expiry_t` = the alert `expires`). Zone-only alerts are skipped (counter
`skip_no_position` on `/metrics`).
