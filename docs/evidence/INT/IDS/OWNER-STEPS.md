# INT/IDS: owner steps (omos PR #562, A19 second pass)

None of this is live. These steps need the cluster, the owner's keychain or other PRs' owners.

1. **Seal the new identities** when their services are about to sync. Follow `platform/nats/IDENTITIES.md` steps 1 and 2. The new rows are `projector-gated`, `geofence`, `mission`, `agentops`, `video-registry` and `auth-callout`; edge rows `bridge-ros2-home`. `python3 scripts/nats_identities.py secrets` lists where each password goes.
   Expected: one SealedSecret per workload, with no plaintext in Git.
2. **Activate in the stages A1 → A2 → B** exactly as in the runbook (patch the ConfigMap, never `helm upgrade`). After A1:
   `nats --user zeus --password <kc> sub 'evt.so.action.>'` (on a tailnet client)
   Expected: no permission error, and propose/confirm events appear.
   `nats --user zeus --password <kc> pub cmd.fleet.drone.sim-x1.goto x`
   Expected: `Permissions Violation`.
3. **#565 owner:** add the per-role `subscribe`/`publish` additions of A19-P11 (`platform/nats/identities/permtest/testdata/client-roles.json`, `proposed_*` fields) to `platform/security/roles/onemind-roles.json`. Set the callout user JWT `resp` to `{max: -1, ttl: 5s}`. Then run
   `python3 scripts/nats_identities.py check-roles --roles platform/security/roles/onemind-roles.json`
   Expected: `ok: 5 roles fit the CLIENT account ...`
4. **#563 owner:** `svc.cot.send` accepts only CLIENT callers. To let agentops reply to ATAK chat, rule A19-P13.
5. **Rule** owner proposals A19-P11 to P16, and #564 I2-P1/I2-P2/I2-P3. The #556 owner adds A19-P11 to P16 to the register by reference.
6. **Real clients:** web (xr #4) and Unreal (xr #5) against a stage-B local leaf rendered from this spec. Expected: the Tasks tab no longer shows "evt.so.action.> not permitted", GeoChat from ATAK arrives (C8), and the B20 gaps panel is fed by `tel.feed.*.status`.
