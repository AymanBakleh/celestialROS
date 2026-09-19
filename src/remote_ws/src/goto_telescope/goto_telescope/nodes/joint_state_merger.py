#!/usr/bin/env python3
"""
Merges joint states from multiple sources:
- joint_state_publisher_gui (for manual control) via /joint_states_gui
- telescope_joint_bridge (for Stellarium commands)
- telescope_driver (hardware feedback: /telescope/hardware_joint_states)

Priority for ra_joint and dec_joint: hardware feedback > Stellarium bridge > GUI,
so when the telescope driver is running, RViz shows actual mount position.
"""
import math
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState


class JointStateMerger(Node):
    """
    Merges joint states from GUI, Stellarium bridge, and hardware.
    Priority for ra/dec: hardware_joint_states > joint_commands (bridge) > GUI.
    """

    def __init__(self):
        super().__init__('joint_state_merger')

        self.gui_joint_state = None
        self.last_valid_gui_joint_state = None
        self.gui_valid = False
        self.bridge_joint_state = None
        self.hardware_joint_state = None
        self.vixen_joint_state = None
        self.initial_joint_state = None

        self.create_subscription(
            JointState, '/joint_states_gui', self.gui_joint_state_callback, 10
        )
        self.create_subscription(
            JointState, '/telescope/joint_commands', self.bridge_joint_state_callback, 10
        )
        self.create_subscription(
            JointState, '/vixen_g2/joint_states', self.vixen_joint_state_callback, 10
        )
        self.create_subscription(
            JointState, '/telescope/hardware_joint_states', self.hardware_joint_state_callback, 10
        )

        self.merged_pub = self.create_publisher(JointState, '/joint_states_merged', 10)
        self.declare_parameter('location.lat', 33.5138)
        self.declare_parameter('location.lon', 36.2765)

        self.latitude = self.get_parameter('location.lat').value
        self.longitude = self.get_parameter('location.lon').value

        self.declare_parameter('initial.ra', 0.0)
        self.declare_parameter('initial.dec', 90.0)
        # When True, allow Stellarium (/telescope/joint_commands) to override GUI for ra/dec
        # whenever hardware feedback is not available.
        self.declare_parameter('bridge_over_gui', False)

        self.initial_ra = self.get_parameter('initial.ra').value
        self.initial_dec = self.get_parameter('initial.dec').value
        self.bridge_over_gui = bool(self.get_parameter('bridge_over_gui').value)

        self.create_timer(0.05, self.publish_merged_state)
        self.get_logger().info('JointStateMerger started')
        self.get_logger().info(f'Initial set RA={self.initial_ra}h DEC={self.initial_dec}° at lat={self.latitude} lon={self.longitude}')

    def gui_joint_state_callback(self, msg: JointState):
        self.gui_joint_state = msg
        if not self._is_uninitialized_joint_state(msg):
            self.gui_valid = True
            self.last_valid_gui_joint_state = msg

    def bridge_joint_state_callback(self, msg: JointState):
        self.bridge_joint_state = msg

    def hardware_joint_state_callback(self, msg: JointState):
        self.hardware_joint_state = msg

    def initial_joint_state_callback(self, msg: JointState):
        """Capture initialization joint state (from stellarium_initialization)"""
        self.initial_joint_state = msg
        # keep bridge source for fallback if GUI uninitialized
        self.bridge_joint_state = msg

    def vixen_joint_state_callback(self, msg: JointState):
        self.vixen_joint_state = msg
    def _override_ra_dec(self, merged: JointState, source: JointState) -> bool:
        """Override ra_joint and dec_joint in merged from source. Returns True if any overridden."""
        try:
            names = source.name
            pos = source.position
            if 'ra_joint' in names and 'dec_joint' in names:
                ra_idx = names.index('ra_joint')
                dec_idx = names.index('dec_joint')
                if ra_idx < len(pos) and dec_idx < len(pos):
                    if 'ra_joint' in merged.name:
                        merged.position[merged.name.index('ra_joint')] = pos[ra_idx]
                    else:
                        merged.name.append('ra_joint')
                        merged.position.append(pos[ra_idx])
                    if 'dec_joint' in merged.name:
                        merged.position[merged.name.index('dec_joint')] = pos[dec_idx]
                    else:
                        merged.name.append('dec_joint')
                        merged.position.append(pos[dec_idx])
                    return True
        except (ValueError, IndexError):
            pass
        return False

    def _is_uninitialized_joint_state(self, msg: JointState) -> bool:
        try:
            if all(j in msg.name for j in ['polar_align_joint', 'ra_joint', 'dec_joint']):
                polar = msg.position[msg.name.index('polar_align_joint')]
                ra = msg.position[msg.name.index('ra_joint')]
                dec = msg.position[msg.name.index('dec_joint')]
                return abs(polar) < 1e-3 and abs(ra) < 1e-3 and abs(dec) < 1e-3
        except Exception:
            pass
        return False

    def publish_merged_state(self):
        # If GUI ever supplied non-zero moves, preserve GUI control.
        if self.gui_joint_state is not None and self._is_uninitialized_joint_state(self.gui_joint_state):
            if not self.gui_valid:
                self.gui_joint_state = None

        if self.gui_valid and self.last_valid_gui_joint_state is not None:
            merged = JointState()
            merged.header.stamp = self.get_clock().now().to_msg()
            merged.header.frame_id = 'telescope'
            merged.name = list(self.last_valid_gui_joint_state.name)
            merged.position = list(self.last_valid_gui_joint_state.position)
            merged.velocity = list(self.last_valid_gui_joint_state.velocity) if self.last_valid_gui_joint_state.velocity else []
            merged.effort = list(self.last_valid_gui_joint_state.effort) if self.last_valid_gui_joint_state.effort else []

            if self.hardware_joint_state is not None:
                self._override_ra_dec(merged, self.hardware_joint_state)
            elif self.bridge_over_gui and self.bridge_joint_state is not None:
                # No hardware feedback; allow Stellarium commands to drive ra/dec even if GUI moved.
                self._override_ra_dec(merged, self.bridge_joint_state)

            self.merged_pub.publish(merged)
            return

        if self.gui_joint_state is None:
            # No valid GUI yet: use hardware > initialization > bridge source
            src = self.hardware_joint_state if self.hardware_joint_state is not None else (
                self.vixen_joint_state if self.vixen_joint_state is not None else (
                    self.initial_joint_state if self.initial_joint_state is not None else self.bridge_joint_state
                )
            )
            if src is not None:
                msg = JointState()
                msg.header.stamp = self.get_clock().now().to_msg()
                msg.header.frame_id = 'telescope'
                msg.name = ['polar_align_joint', 'ra_joint', 'dec_joint']
                msg.position = [
                    math.radians(self.latitude),
                    (self.initial_ra % 12.0) * math.pi / 12.0,
                    max(-math.pi / 2.0, min(math.pi / 2.0, math.radians(self.initial_dec)))
                ]
                try:
                    if 'ra_joint' in src.name:
                        msg.position[1] = src.position[src.name.index('ra_joint')]
                    if 'dec_joint' in src.name:
                        msg.position[2] = src.position[src.name.index('dec_joint')]
                    if 'polar_align_joint' in src.name:
                        msg.position[0] = src.position[src.name.index('polar_align_joint')]
                except (ValueError, IndexError):
                    pass
                msg.velocity = []
                msg.effort = []
                self.merged_pub.publish(msg)
                return

            # No source; publish configured startup orientation.
            default_ra = self.initial_ra  # hours
            default_dec = self.initial_dec  # degrees
            msg = JointState()
            msg.header.stamp = self.get_clock().now().to_msg()
            msg.header.frame_id = 'telescope'
            msg.name = ['polar_align_joint', 'ra_joint', 'dec_joint']
            msg.position = [
                math.radians(self.latitude),
                (default_ra % 12.0) * math.pi / 12.0,
                max(-math.pi / 2.0, min(math.pi / 2.0, math.radians(default_dec)))
            ]
            msg.velocity = []
            msg.effort = []
            self.merged_pub.publish(msg)
            return

        merged = JointState()
        merged.header.stamp = self.get_clock().now().to_msg()
        merged.header.frame_id = 'telescope'
        merged.name = list(self.gui_joint_state.name)
        merged.position = list(self.gui_joint_state.position)
        merged.velocity = list(self.gui_joint_state.velocity) if self.gui_joint_state.velocity else []
        merged.effort = list(self.gui_joint_state.effort) if self.gui_joint_state.effort else []

        # Priority for manual GUI mode: GUI values should control unless hardware takes over.
        if self.hardware_joint_state is not None:
            self._override_ra_dec(merged, self.hardware_joint_state)

        self.merged_pub.publish(merged)


def main(args=None):
    rclpy.init(args=args)
    node = JointStateMerger()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()