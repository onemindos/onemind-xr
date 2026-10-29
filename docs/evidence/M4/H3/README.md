# H3 evidence: ROS 2 bridge

omos PR #566 (draft), branch `feat/ros2-bridge`, commit `737edeca`, worktree
`/Users/danielcarku/Desktop/work/wt/h3-ros2`. Code in `services/bridge/bridges/ros2`.

FEATURES H3 acceptance: "A ROS 2 turtlebot sim moves on the map and takes a goto command
after a human confirm". Status: **built, awaiting owner check.** The goto-after-human-confirm
path is proved here against a scripted TurtleBot. The real ROS 2 / Gazebo / Nav2 run is in
`OWNER-STEPS.md`.

| File | What it shows |
|---|---|
| `unit-tests.txt` | `pytest tests`: 150 passed, 1 skipped (the e2e test without its environment). Covers the CDR codec, JCS, the verifier including policy-signed vectors, the bridge core, Zenoh with real sessions, and rosbridge with a fake server |
| `ci-github-test-ros2-bridge.txt` | the same suite on GitHub Actions (`test-ros2-bridge.yml`, ubuntu-latest): 150 passed, 1 skipped; all PR checks green |
| `e2e-test.txt` | `tests/e2e/run.sh`: 1 passed. Runs nats-server (per-identity accounts), the policy service binary built from #554 at the commit in `e2e/policy-ref.txt`, a stub entity service, the robot sim and the bridge |
| `e2e/e2e-report.json` | each e2e step and its result. The human-confirmed goto arrived within 0.35 m. Refusals: `agent_cannot_confirm`, client `cmd.fleet` publish blocked, `replayed`, `bad_signature`, bridge fence `precondition_failed/keep_in` after policy approval, policy fence denial, `not_simulated`, e-stop latch `estopped`. Invariant: every policy `cmd.fleet` message was human-approved |
| `e2e/transcript.jsonl` | bus transcript (all `svc`/`cmd`/`evt` traffic, every 10th telemetry message) |
| `e2e/robot-sim.jsonl` | what reached the robot: one goal (1.2, 0.6) in `map`, then the e-stop zero `cmd_vel` and cancel. The goals that were refused never reached it |
| `e2e/*.log` | nats-server, policy, bridge and sim logs. Passwords and the signing key were generated per run in a temp dir that was deleted |
| `ci-equivalent.txt` | yamllint, `validate_changed_yaml.py`, `validate_manifests.py`, shellcheck, gitleaks: clean |
| `OWNER-STEPS.md` | the TurtleBot3 Gazebo + Nav2 run, the zenoh-bridge-ros2dds key and framing checks, the rosbridge fallback |

Not done here:
- **Blocker:** the agent permission policy denied running the downloaded `zenoh-bridge-ros2dds`
  1.10.2 binary (and by extension `zenohd`). The adapter was therefore tested against a scripted
  robot on eclipse-zenoh (pip). That robot follows the bridge's documented namespace mapping,
  but the action key names and the payload framing are unverified (OWNER-STEPS step 3).
- ROS 2 and Gazebo are not installed (no Docker, and ROS 2 on macOS is Tier 3). None of it was built.
