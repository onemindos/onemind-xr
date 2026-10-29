Requirement from #555 (A22-P2 option a), needed before `CMD` can switch to `discard: new`:

A full `discard: new` stream refuses to *store* a publish, but NATS still *delivers* it to core subscribers (a disposable-server test stored 2 of 5 publishes and refused 3; a core `cmd.>` subscriber received all 5; engine test `test_cmd_discard_new_refused_publish_still_reaches_core_subscribers`). The bridge and task identities here subscribe to `cmd.fleet.drone.*` / `cmd.mission.>` on core, so a full CMD would stop recording commands and keep delivering them.

1. Every `cmd.*` consumer (MAVLink bridges, the task service) reads commands through a JetStream consumer on `CMD`, never a core subscription; the NATS permissions deny those identities a core `cmd.>` subscription.
2. The policy service stores the audit record and checks its ack before it publishes the command, and treats a refused `CMD` publish as a failed command.
3. An engine test shows a full `discard: new` `CMD` delivers nothing to the bridge.

Until then `CMD` stays on `discard: old` with the 80% `NATSCmdStreamNearFull` alert.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
