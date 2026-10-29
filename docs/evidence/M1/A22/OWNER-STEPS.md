# A22 owner steps (omos PR #555, branch feat/stream-caps)

Merging PR #555 is activation: `platform-nats-streams` auto-syncs (selfHeal) and NACK applies
the new limits within minutes. The COT 72h cut deletes about 4.45 GB per replica.

## 1. Decide A22-P1..P5
Read `docs/proposals/2026-10-29-stream-caps.md` in the PR. To keep the 30-day COT history,
revert commit e19d4d61 on the branch (or ask for it to be split out) before merging.

## 2. Before merging: keep a COT copy (only if the history matters)
Confirm the latest nightly archive and its marker in the backup bucket (prefix `backups/nats-onemind`).
Optional fresh archive (cluster write):
    kubectl --context do-nyc3-omos-cloud -n nats create job --from=cronjob/backup-nats-streams backup-nats-streams-pre-a22
    kubectl --context do-nyc3-omos-cloud -n nats wait --for=condition=complete job/backup-nats-streams-pre-a22 --timeout=2h
Expected: Complete in about 5 min (recent runs took 4m14s to 4m37s).

## 3. Merge (Hermes or the owner; this run never merges)
Then watch the sync:
    kubectl --context do-nyc3-omos-cloud -n argocd get application platform-nats-streams
Expected: Synced / Healthy.

## 4. Verify (read-only)
    kubectl --context do-nyc3-omos-cloud -n nats get streams.jetstream.nats.io
Expected: 15 Stream CRs, all Ready (`det` new).
    kubectl --context do-nyc3-omos-cloud get --raw '/api/v1/namespaces/nats/pods/nats-0:8222/proxy/jsz?streams=1&config=1' \
      | python3 -c 'import json,sys; [print(s["name"], s["config"].get("max_age"), s["config"].get("max_bytes"), s["config"].get("discard"), s["state"]["bytes"]) for a in json.load(sys.stdin)["account_details"] for s in a["stream_detail"] if not s["name"].startswith("KV_")]'
Expected: COT 259200000000000 3221225472 old, bytes about 0.5e9; CMD 31536000000000000 536870912 new;
TEL 259200000000000 4294967296 old; DET 604800000000000 2147483648 old; INTEL 7776000000000000 1073741824 old.
Grafana/Prometheus: JetStream storage ratio drops from about 10% to about 1%; alert NATSCmdStreamNearFull exists and is inactive.

## Review-fix round (2026-10-29): steps blocked for this run

The permission system denied external GitHub writes other than the push and the PR edit, so these are for the owner.

### 1. File the KV maxBytes follow-up issue (finding 4)

```bash
gh issue create -R onemindos/omos --title "nats: maxBytes caps on the remaining KV CRs (A22 follow-up)" --body-file /Users/danielcarku/Desktop/work/onemind-xr/docs/evidence/M1/A22/followup-kv-maxbytes-issue.md
```
Expected: the command prints a new issue URL. Link it in docs/proposals/2026-10-29-stream-caps.md under 'Follow-up'.

### 2. Post the A22-P2 option (a) requirement on #554 and #550 (finding 1)

```bash
gh pr comment 554 -R onemindos/omos --body-file /Users/danielcarku/Desktop/work/onemind-xr/docs/evidence/M1/A22/cmd-jetstream-consumer-requirement.md
gh pr comment 550 -R onemindos/omos --body-file /Users/danielcarku/Desktop/work/onemind-xr/docs/evidence/M1/A22/cmd-jetstream-consumer-requirement.md
```
Expected: each command prints a comment URL. CMD stays on discard: old until both PRs meet the requirement.
