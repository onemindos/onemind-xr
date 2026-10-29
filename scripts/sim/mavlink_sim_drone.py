#!/usr/bin/env python3
"""Scripted MAVLink drone for local development (fallback for ArduPilot SITL).

Simulated asset only (`sim-*`, decision 0004 #13). It speaks real MAVLink 2 via
pymavlink: HEARTBEAT, SYS_STATUS, GLOBAL_POSITION_INT, ATTITUDE and VFR_HUD,
orbiting a centre point, and it ACKs COMMAND_LONG (arm, disarm, takeoff, land,
RTL) by changing its simulated state. It never talks to real hardware.

Usage: mavlink_sim_drone.py [--out udpout:127.0.0.1:14550] [--sysid 1]
       [--lat 0] [--lon 0] [--radius 150] [--alt 60]
"""
import argparse
import math
import time

from pymavlink import mavutil

M = mavutil.mavlink


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="udpout:127.0.0.1:14550")
    ap.add_argument("--sysid", type=int, default=1)
    ap.add_argument("--lat", type=float, default=0.0)
    ap.add_argument("--lon", type=float, default=0.0)
    ap.add_argument("--radius", type=float, default=150.0)
    ap.add_argument("--alt", type=float, default=60.0)
    ap.add_argument("--speed", type=float, default=12.0, help="ground speed m/s")
    args = ap.parse_args()

    link = mavutil.mavlink_connection(args.out, source_system=args.sysid, source_component=1)
    armed, airborne, alt, battery = False, False, 0.0, 100.0
    t0 = time.monotonic()
    last_hb = 0.0
    boot = time.monotonic()

    while True:
        now = time.monotonic()
        t = now - t0
        # Orbit only when airborne; otherwise sit at the centre.
        omega = args.speed / args.radius
        ang = omega * t if airborne else 0.0
        r = args.radius if airborne else 0.0
        north, east = r * math.cos(ang), r * math.sin(ang)
        lat = args.lat + north / 111_320.0
        lon = args.lon + east / (111_320.0 * max(math.cos(math.radians(args.lat)), 1e-6))
        target_alt = args.alt if airborne else 0.0
        alt += max(min(target_alt - alt, 2.0), -2.0) * 0.1
        heading = (math.degrees(ang) + 90.0) % 360.0
        vn, ve = (-args.speed * math.sin(ang), args.speed * math.cos(ang)) if airborne else (0.0, 0.0)
        if armed:
            battery = max(battery - 0.002, 0.0)
        ms = int((now - boot) * 1000)

        if now - last_hb >= 1.0:
            mode = M.MAV_MODE_FLAG_CUSTOM_MODE_ENABLED | (M.MAV_MODE_FLAG_SAFETY_ARMED if armed else 0)
            link.mav.heartbeat_send(M.MAV_TYPE_QUADROTOR, M.MAV_AUTOPILOT_ARDUPILOTMEGA, mode, 0,
                                    M.MAV_STATE_ACTIVE if armed else M.MAV_STATE_STANDBY)
            link.mav.sys_status_send(0, 0, 0, 500, 15800, -1, int(battery), 0, 0, 0, 0, 0, 0)
            last_hb = now
        link.mav.global_position_int_send(ms, int(lat * 1e7), int(lon * 1e7), int(alt * 1000),
                                          int(alt * 1000), int(vn * 100), int(ve * 100), 0, int(heading * 100))
        link.mav.attitude_send(ms, 0.0, 0.0, math.radians(heading), 0.0, 0.0, omega if airborne else 0.0)
        link.mav.vfr_hud_send(args.speed if airborne else 0.0, args.speed if airborne else 0.0,
                              int(heading), 50 if airborne else 0, alt, 0.0)

        msg = link.recv_match(type="COMMAND_LONG", blocking=False)
        if msg is not None:
            result = M.MAV_RESULT_ACCEPTED
            if msg.command == M.MAV_CMD_COMPONENT_ARM_DISARM:
                armed = msg.param1 >= 1
                airborne = airborne and armed
            elif msg.command == M.MAV_CMD_NAV_TAKEOFF and armed:
                airborne = True
            elif msg.command in (M.MAV_CMD_NAV_LAND, M.MAV_CMD_NAV_RETURN_TO_LAUNCH):
                airborne = False
            else:
                result = M.MAV_RESULT_UNSUPPORTED
            link.mav.command_ack_send(msg.command, result)
        time.sleep(0.1)


if __name__ == "__main__":
    main()
