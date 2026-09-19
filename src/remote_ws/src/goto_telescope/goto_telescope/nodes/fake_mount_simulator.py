#!/usr/bin/env python3
"""
Fake mount simulator:

- Subscribes to Stellarium target: /stellarium/target (Vector3: x=RA deg, y=DEC deg)
- Moves gradually using the same RA/DEC selection and stepping model as the firmware
- Publishes RViz-only mechanical joints on /telescope/fake_joint_states
- Publishes sky RA/DEC on /telescope/fake_RaDec

This lets you test Stellarium slews without the real ESP32/firmware connected.
"""

from datetime import datetime, timezone
import math

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Vector3
from sensor_msgs.msg import JointState


class FakeMountSimulator(Node):
    def __init__(self):
        super().__init__('fake_mount_simulator')

        self.declare_parameter('rate_hz', 30.0)
        self.declare_parameter('ra_speed_deg_s', 4.0)
        self.declare_parameter('dec_speed_deg_s', 4.0)
        self.declare_parameter('stop_deadband_deg', 0.05)
        self.declare_parameter('polar_align_deg', 33.3)
        self.declare_parameter('site_latitude_deg', 33.50917)
        self.declare_parameter('site_longitude_deg', 36.31167)
        self.declare_parameter('meridian_flip_ha_threshold_deg', 90.0)
        self.declare_parameter('visualize_meridian_flip', True)
        self.declare_parameter('visual_ra_inverted', True)
        self.declare_parameter('apply_pier_side_ra180', True)
        self.declare_parameter('firmware_emulation', True)
        self.declare_parameter('ticks_per_axis_deg', 4096.0 / 2.5)
        self.declare_parameter('deadband_steps', 8)
        self.declare_parameter('p_max_step', 100)

        self.rate_hz = float(self.get_parameter('rate_hz').value)
        self.ra_speed_deg_s = float(self.get_parameter('ra_speed_deg_s').value)
        self.dec_speed_deg_s = float(self.get_parameter('dec_speed_deg_s').value)
        self.stop_deadband_deg = float(self.get_parameter('stop_deadband_deg').value)
        self.polar_align_deg = float(self.get_parameter('polar_align_deg').value)
        self.site_latitude_deg = float(self.get_parameter('site_latitude_deg').value)
        self.site_longitude_deg = float(self.get_parameter('site_longitude_deg').value)
        self.meridian_flip_ha_threshold_deg = float(
            self.get_parameter('meridian_flip_ha_threshold_deg').value
        )
        self.visualize_meridian_flip = bool(self.get_parameter('visualize_meridian_flip').value)
        self.visual_ra_inverted = bool(self.get_parameter('visual_ra_inverted').value)
        self.apply_pier_side_ra180 = bool(self.get_parameter('apply_pier_side_ra180').value)
        self.firmware_emulation = bool(self.get_parameter('firmware_emulation').value)
        self.ticks_per_axis_deg = float(self.get_parameter('ticks_per_axis_deg').value)
        self.deadband_steps = int(self.get_parameter('deadband_steps').value)
        self.p_max_step = int(self.get_parameter('p_max_step').value)

        self.current_ra_deg = 0.0
        self.current_dec_deg = 90.0
        self.target_ra_deg = None
        self.target_dec_deg = None
        self.desired_ra_deg = 0.0
        self.desired_dec_deg = 90.0
        self.meridian_flipped = False

        self._fake_pub = self.create_publisher(JointState, '/telescope/fake_joint_states', 10)
        self._fake_radec_pub = self.create_publisher(Vector3, '/telescope/fake_RaDec', 10)
        self.create_subscription(Vector3, '/stellarium/target', self._target_cb, 10)

        self._last_time = self.get_clock().now()
        period = 1.0 / max(1.0, self.rate_hz)
        self.create_timer(period, self._step)

        self.get_logger().info(
            f'FakeMountSimulator running (rate={self.rate_hz}Hz, ra={self.ra_speed_deg_s}°/s, dec={self.dec_speed_deg_s}°/s, apply_pier_side_ra180={self.apply_pier_side_ra180}, firmware_emulation={self.firmware_emulation}, visual_ra_inverted={self.visual_ra_inverted})'
        )

    @staticmethod
    def _normalize_ra_deg(ra_deg: float) -> float:
        while ra_deg >= 360.0:
            ra_deg -= 360.0
        while ra_deg < 0.0:
            ra_deg += 360.0
        return ra_deg

    @staticmethod
    def _normalize_signed_deg(angle_deg: float) -> float:
        while angle_deg > 180.0:
            angle_deg -= 360.0
        while angle_deg <= -180.0:
            angle_deg += 360.0
        return angle_deg

    @staticmethod
    def _normalize_dec_deg(dec_deg: float) -> float:
        return max(-90.0, min(90.0, dec_deg))

    @classmethod
    def _ra_angular_distance(cls, current_ra_deg: float, target_ra_deg: float) -> float:
        current_ra_deg = cls._normalize_ra_deg(current_ra_deg)
        target_ra_deg = cls._normalize_ra_deg(target_ra_deg)
        diff = target_ra_deg - current_ra_deg
        if diff > 180.0:
            diff -= 360.0
        elif diff < -180.0:
            diff += 360.0
        return diff

    def _current_lst_deg(self) -> float:
        now = datetime.now(timezone.utc)
        year = now.year
        month = now.month
        day = now.day
        hour = now.hour
        minute = now.minute
        second = now.second + now.microsecond / 1e6

        if month <= 2:
            year -= 1
            month += 12

        a = year // 100
        b = 2 - a + a // 4
        ut_decimal = hour + minute / 60.0 + second / 3600.0
        jd = math.floor(365.25 * (year + 4716)) + math.floor(30.6001 * (month + 1)) + day + b - 1524.5
        jd += ut_decimal / 24.0

        jd_0h = math.floor(jd - 0.5) + 0.5
        d0 = jd_0h - 2451545.0
        gmst_0h = 100.4606 + 0.98564736628 * d0
        lst_deg = gmst_0h + self.site_longitude_deg + (15.041068 * ut_decimal)
        return self._normalize_ra_deg(lst_deg)

    def _select_best_pier_side_target(
        self,
        input_ra_deg: float,
        input_dec_deg: float,
        current_ra_deg: float,
        current_dec_deg: float,
        lst_deg: float,
    ) -> tuple[float, float, bool]:
        ra_a = self._normalize_ra_deg(input_ra_deg)
        dec_a = self._normalize_dec_deg(input_dec_deg)

        ra_b = self._normalize_ra_deg(ra_a + 180.0)
        dec_b = dec_a

        ra_move_a = abs(self._ra_angular_distance(current_ra_deg, ra_a))
        ra_move_b = abs(self._ra_angular_distance(current_ra_deg, ra_b))
        dec_move_a = abs(dec_a - current_dec_deg)
        dec_move_b = abs(dec_b - current_dec_deg)

        cost_a = ra_move_a + 0.35 * dec_move_a
        cost_b = ra_move_b + 0.35 * dec_move_b

        choose_flip = self.meridian_flipped
        force_flip_by_ra_limits = (ra_a > 90.0 and ra_a < 180.0) or (ra_a >= 180.0 and ra_a <= 270.0)

        target_ha_signed = self._normalize_signed_deg(lst_deg - ra_a)
        target_far_from_meridian = abs(target_ha_signed) > self.meridian_flip_ha_threshold_deg

        if force_flip_by_ra_limits:
            choose_flip = True
        elif target_far_from_meridian:
            if (cost_b + 5.0) < cost_a:
                choose_flip = True
            elif (cost_a + 5.0) < cost_b:
                choose_flip = False

        if choose_flip:
            return ra_b, dec_b, True
        return ra_a, dec_a, False

    def _sky_to_visual_dec(self, dec_deg: float) -> float:
        dec_deg = self._normalize_dec_deg(dec_deg)
        if self.meridian_flipped:
            return (270.0 - dec_deg) % 360.0
        return (90.0 + dec_deg) % 360.0

    def _motor_to_sky_ra_deg(self, motor_ra_deg: float) -> float:
        ra = self._normalize_ra_deg(motor_ra_deg)
        if self.meridian_flipped:
            ra = self._normalize_ra_deg(ra - 180.0)
        return ra

    def _target_cb(self, msg: Vector3):
        ra_deg = self._normalize_ra_deg(float(msg.x))
        dec_deg = self._normalize_dec_deg(float(msg.y))

        if self.apply_pier_side_ra180:
            current_ra_deg = self.current_ra_deg
            current_dec_deg = self.current_dec_deg
            lst_deg = self._current_lst_deg()

            selected_ra_deg, selected_dec_deg, flip_applied = self._select_best_pier_side_target(
                ra_deg,
                dec_deg,
                current_ra_deg,
                current_dec_deg,
                lst_deg,
            )
        else:
            selected_ra_deg, selected_dec_deg, flip_applied = ra_deg, dec_deg, False

        self.desired_ra_deg = ra_deg
        self.desired_dec_deg = dec_deg
        self.target_ra_deg = selected_ra_deg
        self.target_dec_deg = selected_dec_deg
        self.meridian_flipped = flip_applied

        self.get_logger().info(
            f'New target: RA={ra_deg:.3f}° DEC={dec_deg:.3f}° -> selected RA={selected_ra_deg:.3f}° DEC={selected_dec_deg:.3f}° flip={flip_applied}'
        )

    def _publish_feedback(self):
        stamp = self.get_clock().now().to_msg()
        visual_ra_deg = -self.current_ra_deg if self.visual_ra_inverted else self.current_ra_deg

        fake = JointState()
        fake.header.stamp = stamp
        fake.header.frame_id = 'telescope'
        fake.name = ['polar_align_joint', 'ra_joint', 'dec_joint']
        fake.position = [
            math.radians(self.polar_align_deg),
            math.radians(visual_ra_deg),
            math.radians(self._sky_to_visual_dec(self.current_dec_deg)) if self.visualize_meridian_flip else math.radians(self.current_dec_deg),
        ]
        fake.velocity = []
        fake.effort = []
        self._fake_pub.publish(fake)

        fake_radec = Vector3()
        fake_radec.x = self._motor_to_sky_ra_deg(self.current_ra_deg)
        fake_radec.y = self.current_dec_deg
        fake_radec.z = 0.0
        self._fake_radec_pub.publish(fake_radec)

    def _step(self):
        now = self.get_clock().now()
        dt = (now - self._last_time).nanoseconds / 1e9
        self._last_time = now
        if dt <= 0:
            return

        if self.target_ra_deg is None or self.target_dec_deg is None:
            self._publish_feedback()
            return

        ra_err = self._ra_angular_distance(self.current_ra_deg, self.target_ra_deg)
        dec_err = self.target_dec_deg - self.current_dec_deg

        if self.firmware_emulation:
            ra_err_steps = int(round(abs(ra_err) * self.ticks_per_axis_deg))
            dec_err_steps = int(round(abs(dec_err) * self.ticks_per_axis_deg))

            if ra_err_steps <= self.deadband_steps and dec_err_steps <= self.deadband_steps:
                self.current_ra_deg = self.target_ra_deg
                self.current_dec_deg = self.target_dec_deg
                self.target_ra_deg = None
                self.target_dec_deg = None
                self._publish_feedback()
                return

            if ra_err_steps > self.deadband_steps:
                ra_ticks_to_move = min(ra_err_steps, max(ra_err_steps // 10, 1))
                ra_ticks_to_move = min(ra_ticks_to_move, self.p_max_step)
                ra_step_deg = (ra_ticks_to_move / self.ticks_per_axis_deg)
                if ra_err < 0.0:
                    ra_step_deg = -ra_step_deg
                self.current_ra_deg = self._normalize_ra_deg(self.current_ra_deg + ra_step_deg)

            if dec_err_steps > self.deadband_steps:
                dec_ticks_to_move = min(dec_err_steps, max(dec_err_steps // 10, 1))
                dec_ticks_to_move = min(dec_ticks_to_move, self.p_max_step)
                dec_step_deg = (dec_ticks_to_move / self.ticks_per_axis_deg)
                if dec_err < 0.0:
                    dec_step_deg = -dec_step_deg
                self.current_dec_deg = self._normalize_dec_deg(self.current_dec_deg + dec_step_deg)
        else:
            if abs(ra_err) < self.stop_deadband_deg and abs(dec_err) < self.stop_deadband_deg:
                self.current_ra_deg = self.target_ra_deg
                self.current_dec_deg = self.target_dec_deg
                self.target_ra_deg = None
                self.target_dec_deg = None
                self._publish_feedback()
                return

            max_ra_step = self.ra_speed_deg_s * dt
            max_dec_step = self.dec_speed_deg_s * dt

            ra_step = max(-max_ra_step, min(max_ra_step, ra_err))
            dec_step = max(-max_dec_step, min(max_dec_step, dec_err))

            self.current_ra_deg = self._normalize_ra_deg(self.current_ra_deg + ra_step)
            self.current_dec_deg = self._normalize_dec_deg(self.current_dec_deg + dec_step)

        self._publish_feedback()


def main(args=None):
    rclpy.init(args=args)
    node = FakeMountSimulator()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()

