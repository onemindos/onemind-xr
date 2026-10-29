"""Reviewer reproducer: e-stop during an in-flight / queued goto."""
import asyncio, sys
sys.path.insert(0, "/Users/danielcarku/Desktop/work/wt/h3-ros2/services/bridge/bridges/ros2/tests")
from test_bridge import make, odom, target, build, PRIV, settle, outcomes
from ros2bridge.transport import GoalResult


def test_estop_during_inflight_and_queued_goto():
    async def go():
        b, bus, tr, clock = make()
        gate = asyncio.Event()
        orig = tr.send_goal
        async def slow(goal):
            await gate.wait()      # Nav2 answers slowly (zenoh query up to 3 s)
            return await orig(goal)
        tr.send_goal = slow
        b.on_odom(odom())
        b._spawn(b.handle_command(*build(PRIV, verb="goto", data=target(5, 0), now=clock.t)))
        b._spawn(b.handle_command(*build(PRIV, verb="goto", data=target(-5, 0), now=clock.t)))
        for _ in range(10): await asyncio.sleep(0)
        await b.handle_command(*build(PRIV, verb="estop", now=clock.t))
        gate.set()
        await settle(b)
        return b, bus, tr
    b, bus, tr = asyncio.run(go())
    print("estopped:", b.estopped, "mode:", b.mode, "goal:", b.goal is not None, "goals sent:", len(tr.goals))
    for o in outcomes(bus): print(o)
    # Safety expectation: after an executed estop no goal is sent and the robot has no goal.
    assert len(tr.goals) == 0 and b.goal is None, "goto executed after e-stop latched"
