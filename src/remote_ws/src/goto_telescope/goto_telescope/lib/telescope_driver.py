"""Telescope driver talking to RoArm-M2-S controller over HTTP.

This implementation assumes:
- The RoArm-M2-S firmware is running the standard Waveshare JSON/HTTP server
  (as in the official repo / web GUI).
- We *only* use the last two joints (ELBOW and EOAT/HAND) as our telescope
  axes for now:
    - ELBOW  (joint 3) -> RA axis
    - EOAT/HAND (joint 4) -> DEC axis

The other three servos (base + 2×shoulder) are never commanded here, so they
stay where the firmware left them (typically at the init pose).
"""
from typing import Tuple
import math
import json

import requests


class TelescopeDriver:
    """HTTP/JSON driver for a RoArm-M2-S-based telescope mount."""

    def __init__(self, ip: str = "192.168.4.1", timeout: float = 1.0) -> None:
        """Create a driver for a RoArm controller at the given IP address.

        The firmware exposes an HTTP endpoint at:
            GET http://<ip>/js?json=<JSON-COMMAND>
        as documented in the Waveshare tutorials.
        """
        # Base URL like "http://192.168.4.1"
        self._base_url = f"http://{ip}"
        self._timeout = timeout

    def _send_json_cmd(self, payload: dict) -> bool:
        """Send one JSON command via HTTP /js endpoint."""
        try:
            # Use requests' param encoding instead of manually concatenating
            resp = requests.get(
                f"{self._base_url}/js",
                params={"json": json.dumps(payload)},
                timeout=self._timeout,
            )
            resp.raise_for_status()
        except Exception:
            # For now just signal failure; caller can log/report if needed
            return False
        return True

    def move_to(self, ra_h: float, dec_deg: float, speed: float | None = None) -> bool:
        """Command the mount to move to the given RA (hours) and Dec (deg).

        Mapping:
        - RA hours  (0–24)    -> ELBOW joint (joint=3), 0–π rad -> 0–180°
        - DEC deg   (-90–+90) -> EOAT/HAND joint (joint=4), -π/2..+π/2 rad -> -90..+90°

        Only joints 3 and 4 are moved; base/shoulder joints are untouched.
        """
        # Normalize RA into 0–12h for a 0–π range (12h span on one mechanical turn)
        ra_normalized = ra_h % 12.0
        ra_joint_rad = (ra_normalized / 12.0) * math.pi

        # Map DEC -90..+90 → -π/2..+π/2 radians
        dec_radians = math.radians(dec_deg)
        dec_joint_rad = max(-math.pi / 2.0, min(math.pi / 2.0, dec_radians))

        # Convert to servo degrees for CMD_SINGLE_JOINT_ANGLE (T=121)
        ra_angle_deg = math.degrees(ra_joint_rad)
        dec_angle_deg = math.degrees(dec_joint_rad)

        # Speed in °/s; keep within a safe/useful range for the servos
        if speed is None:
            speed_deg = 10.0
        else:
            speed_deg = max(1.0, min(50.0, float(speed)))

        cmd_ra = {
            "T": 121,  # CMD_SINGLE_JOINT_ANGLE
            "joint": 3,  # ELBOW_JOINT
            "angle": ra_angle_deg,
            "spd": speed_deg,
            "acc": 10,
        }
        cmd_dec = {
            "T": 121,  # CMD_SINGLE_JOINT_ANGLE
            "joint": 4,  # EOAT_JOINT (used as DEC)
            "angle": dec_angle_deg,
            "spd": speed_deg,
            "acc": 10,
        }

        ok_ra = self._send_json_cmd(cmd_ra)
        ok_dec = self._send_json_cmd(cmd_dec)
        return ok_ra and ok_dec

    def get_position(self) -> Tuple[float, float]:
        """Return current (ra_h, dec_deg) inferred from joints 3 and 4.

        Uses CMD_SERVO_RAD_FEEDBACK (T=105 / 1051 response) and interprets:
        - 'e': ELBOW_JOINT angle in radians  -> RA hours   (0–12h span)
        - 't': EOAT/HAND angle in radians    -> DEC deg    (-90–+90)

        If anything fails, returns (0.0, 0.0).
        """
        payload = {"T": 105}  # CMD_SERVO_RAD_FEEDBACK
        try:
            resp = requests.get(
                f"{self._base_url}/js",
                params={"json": json.dumps(payload)},
                timeout=self._timeout,
            )
            resp.raise_for_status()
            data = resp.json()
        except Exception:
            return 0.0, 0.0

        try:
            elbow_rad = float(data.get("e", 0.0))
            hand_rad = float(data.get("t", 0.0))
        except (TypeError, ValueError):
            return 0.0, 0.0

        # Inverse of the mapping used in move_to()
        ra_h = (elbow_rad / math.pi) * 12.0
        dec_deg = math.degrees(hand_rad)
        return ra_h, dec_deg

