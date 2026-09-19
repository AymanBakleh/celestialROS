#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Vector3


class SkyviewNode(Node):
    def __init__(self):
        super().__init__('skyview_node')

        self.publisher = self.create_publisher(Vector3, '/skyview/target', 10)
        self.request_sub = self.create_subscription(
            Vector3,
            '/skyview/request',
            self.request_callback,
            10,
        )

        self.get_logger().info('Skyview node started. Listening for skyview requests.')

    def request_callback(self, msg):
        self._publish_target(msg, source='skyview_request')

    def _publish_target(self, msg, source):
        target = Vector3()
        target.x = float(msg.x)
        target.y = float(msg.y)
        target.z = 0.0
        self.publisher.publish(target)
        self.get_logger().info(
            f'Forwarded target from {source} to /skyview/target: ' \
            f'RA={target.x} DEC={target.y}'
        )


def main(args=None):
    rclpy.init(args=args)
    node = SkyviewNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
