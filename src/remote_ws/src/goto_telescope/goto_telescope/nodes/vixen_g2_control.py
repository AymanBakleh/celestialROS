#!/usr/bin/env python3
"""Vixen G2 manual control plus Park/Unpark converter (RA/DEC ticks).

Input topic:
- /telescope/g2/tick_command (geometry_msgs/Vector3): x=RA tick count, y=DEC tick count, z=unused

Services:
- /vixen_g2/park (std_srvs/Trigger)
- /vixen_g2/unpark (std_srvs/Trigger)

Published topics:
- /telescope/state (geometry_msgs/Vector3): RA/DEC target for Stellarium
- /joint_states_gui (sensor_msgs/JointState): for joint_state_merger

This node does not touch stellarium API directly, only joint-based telescope control.
"""

import math
import rclpy
from rclpy.node import Node
from std_srvs.srv import Trigger
from sensor_msgs.msg import JointState
from geometry_msgs.msg import Vector3


class VixenG2Control(Node):
    def __init__(self):
        super().__init__('vixen_g2_control')

        self.declare_parameter('location.lat', 33.5138)
        self.declare_parameter('location.lon', 36.2765)
        self.declare_parameter('initial.ra', 37.75)  # Polaris RA degrees (approx ~2h31m)
        self.declare_parameter('initial.dec', 89.2641667)  # Polaris DEC degrees
        self.declare_parameter('tick.ra_arcsec', 15.0)
        self.declare_parameter('tick.dec_arcsec', 15.0)
        self.declare_parameter('max_dec', 89.0)
        self.declare_parameter('min_dec', -89.0)

        self.latitude = self.get_parameter('location.lat').value
        self.longitude = self.get_parameter('location.lon').value
        self.current_ra = self.get_parameter('initial.ra').value
        self.current_dec = self.get_parameter('initial.dec').value
        self.ra_tick_arcsec = self.get_parameter('tick.ra_arcsec').value
        self.dec_tick_arcsec = self.get_parameter('tick.dec_arcsec').value
        self.max_dec = self.get_parameter('max_dec').value
        self.min_dec = self.get_parameter('min_dec').value

        self.parked = False

        self.joint_pub = self.create_publisher(JointState, '/vixen_g2/joint_states', 10)

        self.create_subscription(Vector3, '/telescope/g2/tick_command', self.tick_command_callback, 10)

        self.park_srv = self.create_service(Trigger, '/vixen_g2/park', self.park_callback)
        self.unpark_srv = self.create_service(Trigger, '/vixen_g2/unpark', self.unpark_callback)

        self.create_timer(0.1, self.publish_current_state)

        self.get_logger().info('Vixen G2 control node started')
        self.get_logger().info(f'Starting at RA={self.current_ra}h DEC={self.current_dec}° (Damascus/Polaris)')

        # Immediately publish initial state
        self.publish_current_state()

    def ra_dec_to_joint(self, ra_deg, dec_deg):
        ra_joint = math.radians(ra_deg % 360.0)
        dec_joint = max(-math.pi / 2.0, min(math.pi / 2.0, math.radians(dec_deg)))
        return ra_joint, dec_joint

    def joint_to_ra_dec(self, polar_align, ra_joint, dec_joint):
        ra_deg = math.degrees(ra_joint) % 360.0
        dec_deg = math.degrees(dec_joint)
        return ra_deg, dec_deg

    def publish_current_state(self):
        """Publish current RA/DEC converted into joint states for merger/coordinate transformer."""
        ra_joint, dec_joint = self.ra_dec_to_joint(self.current_ra, self.current_dec)

        joint_msg = JointState()
        joint_msg.header.stamp = self.get_clock().now().to_msg()
        joint_msg.name = ['polar_align_joint', 'ra_joint', 'dec_joint']
        joint_msg.position = [math.radians(self.latitude), ra_joint, dec_joint]
        self.joint_pub.publish(joint_msg)

    def tick_command_callback(self, msg: Vector3):
        if self.parked:
            self.get_logger().info('Vixen G2 is parked; tick command ignored')
            return

        ra_delta_deg = (msg.x * self.ra_tick_arcsec) / 3600.0
        dec_delta_deg = msg.y * self.dec_tick_arcsec

        self.current_ra = (self.current_ra + ra_delta_deg) % 360.0
        self.current_dec = max(self.min_dec, min(self.max_dec, self.current_dec + dec_delta_deg))

        self.get_logger().info(f'Applied ticks RA++={ra_delta_deg:.6f}° DEC++={dec_delta_deg:.3f}° => RA={self.current_ra:.6f}° DEC={self.current_dec:.3f}°')

        self.publish_current_state()

    def merged_joint_state_callback(self, msg: JointState):
        if self.parked:
            return

        if 'ra_joint' not in msg.name or 'dec_joint' not in msg.name:
            return

        try:
            ra_joint = msg.position[msg.name.index('ra_joint')]
            dec_joint = msg.position[msg.name.index('dec_joint')]
            ra_deg, dec_deg = self.joint_to_ra_dec(0.0, ra_joint, dec_joint)

            if abs(ra_deg - self.current_ra) > 1e-6 or abs(dec_deg - self.current_dec) > 1e-3:
                self.current_ra = ra_deg
                self.current_dec = dec_deg
                self.publish_current_state()
        except Exception as e:
            self.get_logger().debug(f'Error parsing merged joint state: {e}')

    def park_callback(self, request, response):
        self.parked = True
        response.success = True
        response.message = 'Vixen G2 parked'
        self.get_logger().info(response.message)
        return response

    def unpark_callback(self, request, response):
        self.parked = False
        response.success = True
        response.message = 'Vixen G2 unparked'
        self.get_logger().info(response.message)
        return response


def main(args=None):
    rclpy.init(args=args)
    node = VixenG2Control()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
