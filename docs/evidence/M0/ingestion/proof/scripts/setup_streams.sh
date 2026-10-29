#!/bin/sh
# Streams/KV for the proof. Subjects follow the locked omos grammar.
# dupe window is scaled down to 20s (omos uses 2m) so the window-expiry case is testable quickly.
set -e
N="/Users/danielcarku/Desktop/work/.tools/bin/nats -s nats://127.0.0.1:34222"
$N stream add INTEL --subjects 'intel.>' --storage file --replicas 1 --retention limits --discard old \
  --max-age 2160h --max-bytes 256MB --max-msgs=-1 --max-msgs-per-subject=-1 --max-msg-size 1MB --dupe-window 20s --defaults >/dev/null
$N stream add WORLD --subjects 'evt.world.>' --storage file --replicas 1 --retention limits --discard old \
  --max-age 24h --max-bytes 256MB --max-msgs=-1 --max-msgs-per-subject 100 --max-msg-size 64KB --dupe-window 20s --defaults >/dev/null
$N stream add TEL --subjects 'tel.>' --storage file --replicas 1 --retention limits --discard old \
  --max-age 168h --max-bytes 128MB --max-msgs=-1 --max-msgs-per-subject=-1 --max-msg-size 64KB --dupe-window 20s --defaults >/dev/null
$N stream add FEEDEVT --subjects 'evt.feed.>' --storage file --replicas 1 --retention limits --discard old \
  --max-age 720h --max-bytes 64MB --max-msgs=-1 --max-msgs-per-subject=-1 --max-msg-size 1MB --dupe-window 20s --defaults >/dev/null
$N kv add ingest-state --history 1 --storage file --replicas 1 >/dev/null
$N kv add ingest-locks --history 1 --ttl 10s --storage file --replicas 1 >/dev/null
$N kv add overwatch-video --history 1 --ttl 5m --storage file --replicas 1 >/dev/null
$N stream ls -n
$N kv ls -n
