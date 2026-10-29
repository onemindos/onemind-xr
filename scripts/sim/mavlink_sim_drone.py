#!/usr/bin/env python3
"""Scripted MAVLink drone for local development (fallback for ArduPilot SITL).

Simulated asset only (`sim-*`, decision 0004 #13). It speaks real MAVLink 2 via
pymavlink: HEARTBEAT, SYS_STATUS, GLOBAL_POSITION_INT, ATTITUDE and VFR_HUD,
orbiting a centre point, and it ACKs COMMAND_LONG (arm, disarm, takeoff, land,
RTL, flight termination for estop) by changing its simulated state. MAV_CMD_DO_REPOSITION (goto) is accepted
over COMMAND_INT (x/y degE7, z altitude) or COMMAND_LONG (param5/6/7) when armed
and airborne: the drone flies to the point and holds there until land or RTL.
It never talks to real hardware.

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
    goto = None  # (lat, lon, alt) after an accepted DO_REPOSITION
    cur_lat, cur_lon = args.lat, args.lon
    m_per_deg_lon = 111_320.0 * max(math.cos(math.radians(args.lat)), 1e-6)
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
        lon = args.lon + east / m_per_deg_lon
        target_alt = args.alt if airborne else 0.0
        heading = (math.degrees(ang) + 90.0) % 360.0
        vn, ve = (-args.speed * math.sin(ang), args.speed * math.cos(ang)) if airborne else (0.0, 0.0)
        if airborne and goto is not None:
            # Fly straight to the reposition point at --speed (0.1 s ticks), then hold.
            dn = (goto[0] - cur_lat) * 111_320.0
            de = (goto[1] - cur_lon) * m_per_deg_lon
            dist = math.hypot(dn, de)
            step = min(dist, args.speed * 0.1)
            if dist > 0:
                vn, ve = args.speed * dn / dist, args.speed * de / dist
                heading = math.degrees(math.atan2(de, dn)) % 360.0
                cur_lat += step * dn / dist / 111_320.0
                cur_lon += step * de / dist / m_per_deg_lon
            if dist <= step:
                vn, ve = 0.0, 0.0
            lat, lon, target_alt = cur_lat, cur_lon, goto[2]
        else:
            cur_lat, cur_lon = lat, lon
        alt += max(min(target_alt - alt, 2.0), -2.0) * 0.1
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

        msg = link.recv_match(type=["COMMAND_LONG", "COMMAND_INT"], blocking=False)
        if msg is not None:
            result = M.MAV_RESULT_ACCEPTED
            is_int = msg.get_type() == "COMMAND_INT"
            if msg.command == M.MAV_CMD_DO_REPOSITION:
                if not (armed and airborne):
                    result = M.MAV_RESULT_TEMPORARILY_REJECTED
                elif is_int:
                    goto = (msg.x / 1e7, msg.y / 1e7, msg.z)
                else:
                    goto = (msg.param5, msg.param6, msg.param7)
            elif is_int:
                result = M.MAV_RESULT_UNSUPPORTED
            elif msg.command == M.MAV_CMD_COMPONENT_ARM_DISARM:
                armed = msg.param1 >= 1
                airborne = airborne and armed
            elif msg.command == M.MAV_CMD_NAV_TAKEOFF and armed:
                airborne = True
            elif msg.command in (M.MAV_CMD_NAV_LAND, M.MAV_CMD_NAV_RETURN_TO_LAUNCH):
                airborne = False
            elif msg.command == M.MAV_CMD_DO_FLIGHTTERMINATION and msg.param1 >= 1:
                # estop: simulated flight termination, motors off at once.
                armed, airborne, alt = False, False, 0.0
            else:
                result = M.MAV_RESULT_UNSUPPORTED
            if not airborne:
                goto = None
            link.mav.command_ack_send(msg.command, result)
        time.sleep(0.1)


if __name__ == "__main__":
    main()
