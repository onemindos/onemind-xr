# F-ALERT: owner steps (live check, after deployment)

Everything below needs the live cluster with omos #562 (identities stage A1 or later), #569
(geofence service) and, for the newcomer step, #565 (auth callout with Authentik groups)
deployed by Argo. Nothing here was run against the cluster.

1. Confirm that the server carries the CLIENT import (read-only):
   `kubectl --context do-nyc3-omos-cloud -n nats get cm nats-config -o jsonpath='{.data.nats\.conf}' | grep -c 'evt.so.alert'`
   Expected: at least 2 (the ONEMIND export and the CLIENT import).
2. Open the L0 web client as `zeus` (password from the owner keychain), with a sim drone flying.
   Draw a keep-in zone that the drone is outside, and confirm it (zone.create, A2).
   Expected: the Alerts tab lists "SIM-X1 is outside keep-in zone ..." marked WARNING. In the
   browser console, `document.getElementById("tab-alerts").dataset.alertService` is `ok/ok`.
3. Remove the zone (zone.remove, A2). Expected: the alert leaves the list within a few seconds.
   No reload is needed, because the live `evt.so.alert.resolved` fact removes it.
4. Sign in as an Authentik user in `onemind-newcomers` only (through the auth callout).
   Expected: no service alert is listed, and `dataset.alertService` starts with `denied/`. The
   NATS server log has one `Publish Violation - Subject "svc.geofence.alerts"` line for that
   user per 5 minutes, not more. Selecting the drone shows no task buttons.
5. Optional: switch zeus's UI role to Newcomer and click the alert's entity link.
   Expected: the detail shows "Asset commands are hidden in Newcomer mode." and no Draw tab.
