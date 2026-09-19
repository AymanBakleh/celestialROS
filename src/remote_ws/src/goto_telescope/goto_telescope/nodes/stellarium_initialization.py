#!/usr/bin/env python3
"""Initialize Stellarium and coordinates for Damascus + Polaris.

- Set Stellarium location to Damascus
- Set view to target Polaris
- Send initial telescope position through /telescope/state and joint states
"""

import time
import math

import requests
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Vector3
from sensor_msgs.msg import JointState


def _call_api(host, port, path, params=None):
    url = f'http://{host}:{port}{path}'
    try:
        r = requests.post(url, params=params, timeout=3.0)
        r.raise_for_status()
        return r.text
    except Exception as e:
        raise RuntimeError(f'Failed Stellarium API call {url}: {e}')


class StellariumInitialization(Node):
    def __init__(self):
        super().__init__('stellarium_initialization')

        self.declare_parameter('stellarium.remote_control.host', 'localhost')
        self.declare_parameter('stellarium.remote_control.port', 8090)
        self.declare_parameter('location.lat', 33.5138)
        self.declare_parameter('location.lon', 36.2765)
        self.declare_parameter('stellarium.start_target', 'polaris')

        self.host = self.get_parameter('stellarium.remote_control.host').value
        self.port = self.get_parameter('stellarium.remote_control.port').value
        self.lat = self.get_parameter('location.lat').value
        self.lon = self.get_parameter('location.lon').value
        self.target = self.get_parameter('stellarium.start_target').value
        self.init_polaris = True


        from sensor_msgs.msg import JointState

        self.pub = self.create_publisher(Vector3, '/telescope/state', 10)
        self.joint_pub = self.create_publisher(JointState, '/joint_states', 10)
        self.joint_gui_pub = self.create_publisher(JointState, '/joint_states_gui', 10)
        self.joint_merged_pub = self.create_publisher(JointState, '/joint_states_merged', 10)

        # Defer actual init so ROS is running
        self.create_timer(1.0, self.initialize, callback_group=None)

    def initialize(self):
        if hasattr(self, '_initialized') and self._initialized:
            return
        self._initialized = True

        self.get_logger().info(f'Initializing Stellarium location {self.lat},{self.lon} and target "{self.target}"')

        # Wait until Stellarium remote API and plugins are available
        status_ready = False
        for attempt in range(60):
            try:
                r = requests.get(f'http://{self.host}:{self.port}/api/main/status', timeout=2.0)
                if r.status_code == 200:
                    plugins_resp = requests.get(f'http://{self.host}:{self.port}/api/main/plugins', timeout=2.0)
                    if plugins_resp.status_code == 200:
                        plugins = plugins_resp.json()
                        if 'RemoteControl' in plugins and 'TelescopeControl' in plugins:
                            status_ready = True
                            break
                # sometimes status is fine, but plugin system takes longer
                if r.status_code == 200:
                    status_ready = True
                    break
            except Exception:
                pass
            time.sleep(0.5)
        if not status_ready:
            self.get_logger().warning('Stellarium remote API and plugins not ready after 30s; continuing anyway')

        # Set location to Damascus
        try:
            _call_api(self.host, self.port, '/api/location/setlocationfields',
                      {'latitude': self.lat, 'longitude': self.lon})
            self.get_logger().info('Stellarium location set to Damascus')
        except Exception as e:
            self.get_logger().error(f'Location setup failed: {e}')

        # Focus at target
        try:
            _call_api(self.host, self.port, '/api/main/focus', {'target': self.target, 'mode': 'center'})
            self.get_logger().info(f'Stellarium focus set to {self.target}')
        except Exception as e:
            self.get_logger().error(f'Set focus failed: {e}')

        # Set telescope to a starting RA/DEC and joint state in world coordinates through /telescope/state to trigger pipeline
        if self.init_polaris:
            target_ra_deg = 0  
            target_dec_deg = 90

            msg = Vector3()
            msg.x = target_ra_deg
            msg.y = target_dec_deg
            msg.z = 0.0

            # Convert RA/DEC to joint values in coordinate_transformer/TelescopeJointBridge conventions
            polar_align_rad = math.radians(33.3)  # default polar align angle
            ra_joint = math.radians(target_ra_deg % 360.0)
            dec_joint = max(-math.pi / 2.0, min(math.pi / 2.0, math.radians(target_dec_deg)))

            joint_msg = JointState()
            joint_msg.header.stamp = self.get_clock().now().to_msg()
            joint_msg.name = ['polar_align_joint', 'ra_joint', 'dec_joint']
            joint_msg.position = [
                polar_align_rad,
                ra_joint,
                dec_joint,
            ]

            # Publish once on startup to set initial position at Polaris:
            self.pub.publish(msg)
            joint_msg.header.stamp = self.get_clock().now().to_msg()
            self.joint_pub.publish(joint_msg)
            self.joint_gui_pub.publish(joint_msg)
            self.joint_merged_pub.publish(joint_msg)

            self.get_logger().info('Published initial telescope position /telescope/state and /joint_states positions')

        # after initialization we can stop the node
        self.get_logger().info('Stellarium initialization complete, shutting down initializer node')
        self.destroy_node()
        rclpy.shutdown()


def main(args=None):
    rclpy.init(args=args)
    node = StellariumInitialization()
    rclpy.spin(node)


if __name__ == '__main__':
    main()
