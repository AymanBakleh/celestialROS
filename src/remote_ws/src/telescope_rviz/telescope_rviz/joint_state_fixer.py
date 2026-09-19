import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState


class JointStateFixer(Node):
    def __init__(self):
        super().__init__('joint_state_fixer')

        self.declare_parameter('input_topic', '/telescope/hardware_joint_states')
        self.declare_parameter('output_topic', '/telescope/hardware_joint_states_fixed')
        self.declare_parameter('default_polar_align', 0.0)

        self.input_topic = self.get_parameter('input_topic').value
        self.output_topic = self.get_parameter('output_topic').value
        self.default_polar_align = float(self.get_parameter('default_polar_align').value)

        self.publisher = self.create_publisher(JointState, self.output_topic, 10)
        self.subscription = self.create_subscription(
            JointState,
            self.input_topic,
            self.listener_callback,
            10,
        )

        self.get_logger().info(
            f'JointStateFixer starting: {self.input_topic} -> {self.output_topic}'
        )

    def listener_callback(self, msg: JointState):
        if 'polar_align_joint' in msg.name:
            self.publisher.publish(msg)
            return

        fixed = JointState()
        fixed.header = msg.header
        fixed.name = ['polar_align_joint'] + list(msg.name)
        fixed.position = [self.default_polar_align] + list(msg.position)

        if msg.velocity:
            fixed.velocity = [0.0] + list(msg.velocity)
        elif msg.velocity is not None:
            fixed.velocity = [0.0] * len(fixed.name)

        if msg.effort:
            fixed.effort = [0.0] + list(msg.effort)
        elif msg.effort is not None:
            fixed.effort = [0.0] * len(fixed.name)

        self.publisher.publish(fixed)


def main(args=None):
    rclpy.init(args=args)
    node = JointStateFixer()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
