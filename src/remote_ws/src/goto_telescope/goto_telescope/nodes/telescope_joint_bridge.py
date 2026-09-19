#!/usr/bin/env python3
"""
Bridge node that connects Stellarium commands to telescope joint states.

This node:
1. Subscribes to Stellarium target commands (RA/DEC)
2. Converts RA/DEC to joint positions
3. Publishes joint commands to /joint_states for telescope_description visualization
"""
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from geometry_msgs.msg import Vector3
from goto_telescope.msg import TelescopeState
import math


class TelescopeJointBridge(Node):
    """
    Bridges between Stellarium RA/DEC commands and telescope joint states.
    """
    
    def __init__(self):
        super().__init__('telescope_joint_bridge')

        # Slew simulation parameters (degrees per second)
        self.declare_parameter('slew.enable', True)
        self.declare_parameter('slew.rate_hz', 30.0)
        self.declare_parameter('slew.ra_speed_deg_s', 4.0)
        self.declare_parameter('slew.dec_speed_deg_s', 4.0)
        self.declare_parameter('slew.stop_deadband_deg', 0.05)

        self.slew_enable = bool(self.get_parameter('slew.enable').value)
        self.slew_rate_hz = float(self.get_parameter('slew.rate_hz').value)
        self.slew_ra_speed_deg_s = float(self.get_parameter('slew.ra_speed_deg_s').value)
        self.slew_dec_speed_deg_s = float(self.get_parameter('slew.dec_speed_deg_s').value)
        self.slew_stop_deadband_deg = float(self.get_parameter('slew.stop_deadband_deg').value)
        
        # Current target RA/DEC from Stellarium
        self.target_ra = None
        self.target_dec = None

        # Internal joint targets for slew simulation
        self._target_ra_joint = None
        self._target_dec_joint = None
        
        # Current joint states (for reference)
        self.current_polar_align = math.radians(33.3)  # Default polar alignment at 33.3°
        self.current_ra_joint = 0.0
        self.current_dec_joint = 0.0
        
        # Subscribers
        # Subscribe to Stellarium target commands
        self.create_subscription(
            Vector3,
            '/stellarium/target',
            self.stellarium_target_callback,
            10
        )
        
        # Subscribe to current joint states to know current position
        self.create_subscription(
            JointState,
            '/joint_states_merged',
            self.joint_state_callback,
            10
        )
        
        # Publishers
        # Publish joint commands (this will be merged with joint_state_publisher_gui)
        # In simulation, we publish to a separate topic that can be merged
        self.joint_cmd_pub = self.create_publisher(
            JointState,
            '/telescope/joint_commands',
            10
        )

        self._last_update_time = self.get_clock().now()
        self._slew_timer = None
        if self.slew_enable:
            period = 1.0 / max(1.0, self.slew_rate_hz)
            self._slew_timer = self.create_timer(period, self._slew_step)

        self.get_logger().info('TelescopeJointBridge started')
    
    def stellarium_target_callback(self, msg: Vector3):
        """Handle target from Stellarium (RA in degrees, DEC in degrees)."""
        self.target_ra = msg.x
        self.target_dec = msg.y
        
        self.get_logger().info(
            f'Received Stellarium target: RA={self.target_ra}°, DEC={self.target_dec}°'
        )
        
        # Convert RA/DEC to joint positions
        ra_joint, dec_joint = self.radec_to_joint(self.target_ra, self.target_dec)

        # If slew simulation is enabled, move gradually; otherwise jump immediately.
        if self.slew_enable:
            self._target_ra_joint = ra_joint
            self._target_dec_joint = dec_joint
        else:
            self.publish_joint_command(ra_joint, dec_joint)
    
    def joint_state_callback(self, msg: JointState):
        """Update current joint states for reference."""
        try:
            polar_idx = msg.name.index('polar_align_joint') if 'polar_align_joint' in msg.name else None
            ra_idx = msg.name.index('ra_joint') if 'ra_joint' in msg.name else None
            dec_idx = msg.name.index('dec_joint') if 'dec_joint' in msg.name else None
            
            if polar_idx is not None and polar_idx < len(msg.position):
                self.current_polar_align = msg.position[polar_idx]
            if ra_idx is not None and ra_idx < len(msg.position):
                self.current_ra_joint = msg.position[ra_idx]
            if dec_idx is not None and dec_idx < len(msg.position):
                self.current_dec_joint = msg.position[dec_idx]
        except (ValueError, IndexError):
            pass
    
    def radec_to_joint(self, ra_degrees, dec_degrees):
        """
        Convert RA/DEC coordinates to joint positions.

        RA degrees (0-360) -> RA joint (0 to 2π radians)
        DEC degrees (-90 to +90) -> DEC joint (-π/2 to +π/2 radians)
        """
        # Convert RA degrees to RA joint position
        ra_normalized = ra_degrees % 360.0
        ra_joint = math.radians(ra_normalized)
        
        # Convert DEC degrees to DEC joint position directly
        dec_radians = math.radians(dec_degrees)
        dec_joint = max(-math.pi / 2.0, min(math.pi / 2.0, dec_radians))
        
        return ra_joint, dec_joint

    @staticmethod
    def _wrap_to_pi(angle_rad: float) -> float:
        """Wrap angle to [-pi, pi]."""
        a = (angle_rad + math.pi) % (2.0 * math.pi) - math.pi
        return a

    def _slew_step(self):
        if self._target_ra_joint is None or self._target_dec_joint is None:
            self._last_update_time = self.get_clock().now()
            return

        now = self.get_clock().now()
        dt = (now - self._last_update_time).nanoseconds / 1e9
        self._last_update_time = now
        if dt <= 0:
            return

        # RA is circular: choose shortest direction
        ra_err = self._wrap_to_pi(self._target_ra_joint - self.current_ra_joint)
        dec_err = self._target_dec_joint - self.current_dec_joint

        ra_err_deg = abs(math.degrees(ra_err))
        dec_err_deg = abs(math.degrees(dec_err))

        # Stop if we're close enough
        if ra_err_deg < self.slew_stop_deadband_deg and dec_err_deg < self.slew_stop_deadband_deg:
            # Snap to target
            self.current_ra_joint = self._target_ra_joint
            self.current_dec_joint = self._target_dec_joint
            self.publish_joint_command(self.current_ra_joint, self.current_dec_joint)
            self._target_ra_joint = None
            self._target_dec_joint = None
            return

        max_ra_step_rad = math.radians(self.slew_ra_speed_deg_s) * dt
        max_dec_step_rad = math.radians(self.slew_dec_speed_deg_s) * dt

        ra_step = max(-max_ra_step_rad, min(max_ra_step_rad, ra_err))
        dec_step = max(-max_dec_step_rad, min(max_dec_step_rad, dec_err))

        self.current_ra_joint = (self.current_ra_joint + ra_step) % (2.0 * math.pi)
        self.current_dec_joint = self.current_dec_joint + dec_step
        self.current_dec_joint = max(-math.pi / 2.0, min(math.pi / 2.0, self.current_dec_joint))

        self.publish_joint_command(self.current_ra_joint, self.current_dec_joint)
    
    def publish_joint_command(self, ra_joint, dec_joint):
        """Publish joint command message with all required joints."""
        msg = JointState()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = 'telescope'
        # Include all joints: polar_align_joint (use current value), ra_joint, dec_joint
        msg.name = ['polar_align_joint', 'ra_joint', 'dec_joint']
        # Use current polar_align value, or default to 0 if not set
        polar_align = self.current_polar_align if hasattr(self, 'current_polar_align') else 0.0
        msg.position = [polar_align, ra_joint, dec_joint]
        msg.velocity = []
        msg.effort = []
        
        self.joint_cmd_pub.publish(msg)
        self.get_logger().debug(
            f'Published joint command: polar_align={math.degrees(polar_align):.2f}°, '
            f'RA={math.degrees(ra_joint):.2f}°, DEC={math.degrees(dec_joint):.2f}°'
        )


def main(args=None):
    rclpy.init(args=args)
    node = TelescopeJointBridge()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()