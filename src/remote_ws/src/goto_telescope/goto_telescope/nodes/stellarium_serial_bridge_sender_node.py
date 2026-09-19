#!/usr/bin/env python3
"""ROS2 node: helper for sending LX200 commands to Stellarium via ROS topics.

Important:
- The actual serial endpoint is owned by `stellarium_serial_bridge_receiver_node.py`,
  which opens the PTY master and talks to Stellarium.
- This node publishes to `/stellarium/outgoing` for optional manual/control use.

If you only need Stellarium to show the telescope circle and send goto commands,
you can run the receiver node alone.
"""

import struct
import time

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Vector3
from sensor_msgs.msg import JointState
from std_msgs.msg import String
import math


def _ra_hours_to_lx200(ra_hours: float) -> str:
    ra_hours = ra_hours % 24.0
    h = int(ra_hours)
    m = int((ra_hours - h) * 60)
    s = int(round(((ra_hours - h) * 60 - m) * 60))
    if s >= 60:
        s -= 60
        m += 1
    if m >= 60:
        m -= 60
        h = (h + 1) % 24
    return f"{h:02d}:{m:02d}:{s:02d}#"


def _dec_deg_to_lx200(dec_deg: float) -> str:
    sign = '+' if dec_deg >= 0 else '-'
    dec_deg = abs(dec_deg)
    d = int(dec_deg)
    m = int((dec_deg - d) * 60)
    s = int(round(((dec_deg - d) * 60 - m) * 60))
    if s >= 60:
        s -= 60
        m += 1
    if m >= 60:
        m -= 60
        d += 1
    return f"{sign}{d:02d}*{m:02d}:{s:02d}#"


def _build_stellarium_packet(ra_hours, dec_deg):
    msize = 0x1800
    mtype = 0x0000
    mtime = int(time.time())
    ra_proto = int(ra_hours * 2147483648.0 / 12.0) & 0xFFFFFFFF
    dec_proto = int(dec_deg * 1073741824.0 / 90.0)
    return struct.pack('<HHQIi', msize, mtype, mtime, ra_proto, dec_proto)


class StellariumSerialSenderNode(Node):
    def __init__(self):
        super().__init__('stellarium_serial_sender')
        self.declare_parameter('enable_push', False)
        self.declare_parameter('publish_interval', 0.2)
        self.declare_parameter('protocol', 'ascii')  # ascii or binary

        self.enable_push = bool(self.get_parameter('enable_push').value)
        self.publish_interval = self.get_parameter('publish_interval').value
        self.protocol = self.get_parameter('protocol').value

        # /telescope/state is RA/DEC in DEGREES.
        self.current_ra_deg = 0.0
        self.current_dec_deg = 0.0

        self.command_pub = self.create_publisher(String, '/stellarium/outgoing', 10)

        self.create_subscription(Vector3, '/telescope/state', self._telescope_state_cb, 10)
        self.create_subscription(JointState, '/joint_states_merged', self._joint_states_merged_cb, 10)

        self._timer = None
        if self.enable_push:
            self._timer = self.create_timer(self.publish_interval, self._on_timer)
            self.get_logger().info(
                f'Stellarium sender node push enabled (protocol={self.protocol}, interval={self.publish_interval}s)'
            )
        else:
            self.get_logger().info(
                'Stellarium sender node started with push disabled (set enable_push:=true to spam SR/SD/MS)'
            )

    # No direct serial port usage in sender; publish commands to /stellarium/outgoing instead.

    def _telescope_state_cb(self, msg: Vector3):
        self.current_ra_deg = float(msg.x) % 360.0
        self.current_dec_deg = float(msg.y)

    def _joint_states_merged_cb(self, msg: JointState):
        try:
            if 'ra_joint' in msg.name and 'dec_joint' in msg.name:
                ra_joint = msg.position[msg.name.index('ra_joint')]
                dec_joint = msg.position[msg.name.index('dec_joint')]
                self.current_ra_deg = math.degrees(ra_joint) % 360.0
                self.current_dec_deg = math.degrees(dec_joint)
                self.get_logger().debug(
                    f"From /joint_states_merged -> RA={self.current_ra_deg:.6f}° DEC={self.current_dec_deg:.6f}°"
                )
        except Exception as e:
            self.get_logger().debug(f"Error parsing /joint_states_merged: {e}")

    def _send_lx200_commands(self):
        # This is optional "push" control: send SR/SD/MS to the peer.
        # Note: For normal Stellarium mount operation, the mount should mainly RESPOND
        # to GR/GD polls (handled by receiver node). Keep this only for manual tests.
        ra_hours = (self.current_ra_deg % 360.0) / 15.0
        ra_cmd = f':SR{_ra_hours_to_lx200(ra_hours)[:-1]}#'
        dec_cmd = f':SD{_dec_deg_to_lx200(self.current_dec_deg)[:-1]}#'
        ms_cmd = ':MS#'

        for cmd in [ra_cmd, dec_cmd, ms_cmd]:
            msg = String(data=cmd)
            self.command_pub.publish(msg)
            self.get_logger().debug(f'Published outgoing command: {cmd}')
            time.sleep(0.01)

    def _on_timer(self):
        if self.protocol == 'ascii':
            self._send_lx200_commands()
        else:
            ra_hours = (self.current_ra_deg % 360.0) / 15.0
            packet = _build_stellarium_packet(ra_hours, self.current_dec_deg)
            msg = String(data=packet.hex())
            self.command_pub.publish(msg)
            self.get_logger().debug(f'Published outgoing binary packet: {packet.hex()}')

    def destroy_node(self):
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = StellariumSerialSenderNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
