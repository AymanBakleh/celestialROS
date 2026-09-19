#!/usr/bin/env python3
"""Coordinate transformation between joint states and RA/DEC coordinates."""
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from geometry_msgs.msg import Vector3
import math


class CoordinateTransformer(Node):
    """
    Transforms between telescope joint states (polar_align, ra, dec) and RA/DEC coordinates.
    
    Joint states from telescope_description:
    - polar_align_joint: polar alignment angle (0 to 90 degrees)
    - ra_joint: right ascension (0 to 180 degrees)
    - dec_joint: declination (-90 to +90 degrees)
    
    This node subscribes to /joint_states and publishes telescope state with RA/DEC.
    It also can convert RA/DEC back to joint positions.
    """
    
    def __init__(self):
        super().__init__('coordinate_transformer')
        
        # Parameters
        self.declare_parameter('location.lat', 37.7749)  # Default: San Francisco
        self.declare_parameter('location.lon', -122.4194)
        self.declare_parameter('joint_states_topic', '/joint_states_merged')
        self.declare_parameter('state_topic', '/telescope/state')
        
        self.latitude = self.get_parameter('location.lat').value
        self.longitude = self.get_parameter('location.lon').value
        self.joint_states_topic = self.get_parameter('joint_states_topic').value
        self.state_topic = self.get_parameter('state_topic').value
        
        # Current joint states
        self.polar_align = 0.0  # radians
        self.ra_joint = 0.0     # radians
        self.dec_joint = 0.0    # radians
        
        # Subscribers - subscribe to joint states (from joint_state_publisher or merged)
        self.create_subscription(
            JointState,
            self.joint_states_topic,
            self.joint_state_callback,
            10
        )
        
        # Publishers
        self.telescope_state_pub = self.create_publisher(
            Vector3,
            self.state_topic,
            10
        )
        
        # Timer to publish telescope state
        self.create_timer(0.1, self.publish_telescope_state)
        
        self.get_logger().info('CoordinateTransformer started')
        self.get_logger().info(f'Location: lat={self.latitude}°, lon={self.longitude}°')
        self.get_logger().info(f'State topic: {self.state_topic}')
    
    def joint_state_callback(self, msg: JointState):
        """Update internal joint state from /joint_states topic."""
        try:
            # Find the joint indices
            polar_idx = msg.name.index('polar_align_joint') if 'polar_align_joint' in msg.name else None
            ra_idx = msg.name.index('ra_joint') if 'ra_joint' in msg.name else None
            dec_idx = msg.name.index('dec_joint') if 'dec_joint' in msg.name else None
            
            if polar_idx is not None and polar_idx < len(msg.position):
                self.polar_align = msg.position[polar_idx]
            
            if ra_idx is not None and ra_idx < len(msg.position):
                self.ra_joint = msg.position[ra_idx]
            
            if dec_idx is not None and dec_idx < len(msg.position):
                self.dec_joint = msg.position[dec_idx]
                
        except (ValueError, IndexError) as e:
            self.get_logger().debug(f'Error parsing joint states: {e}')
    
    def joint_to_radec(self, polar_align, ra_joint, dec):
        """
        Convert joint positions to RA/DEC coordinates.
        
        This is a simplified conversion. In reality, the conversion depends on:
        - Polar alignment angle
        - Mount type (equatorial, alt-az, etc.)
        - Current time and location
        
        For now, we use a simple mapping:
        - RA joint position (0 to 2π) maps to RA (0 to 360 degrees)
        - DEC joint position (-π/2 to +π/2) maps to DEC (-90 to +90 degrees)
        """
        # Convert RA joint (0 to 2π radians) to RA degrees (0 to 360 degrees)
        ra_degrees = math.degrees(ra_joint) % 360.0
        
        # Convert DEC joint (-π/2 to +π/2 radians) to DEC degrees (-90 to +90)
        dec_degrees = math.degrees(dec)
        
        return ra_degrees, dec_degrees
    
    def radec_to_joint(self, ra_degrees, dec_degrees):
        """
        Convert RA/DEC coordinates to joint positions.

        Inverse of joint_to_radec.
        """
        # Convert RA degrees (0 to 360) to RA joint (0 to 2π)
        ra_normalized = ra_degrees % 360.0
        ra_joint = math.radians(ra_normalized)

        # Convert DEC degrees (-90 to +90) to DEC joint (-π/2 to +π/2)
        dec_radians = math.radians(dec_degrees)
        dec_joint = max(-math.pi / 2.0, min(math.pi / 2.0, dec_radians))

        return ra_joint, dec_joint
    
    def publish_telescope_state(self):
        """Publish current telescope state based on joint positions."""
        ra_degrees, dec_degrees = self.joint_to_radec(
            self.polar_align,
            self.ra_joint,
            self.dec_joint
        )

        msg = Vector3()
        msg.x = ra_degrees
        msg.y = dec_degrees
        msg.z = 0.0

        self.get_logger().debug(
            f'CoordinateTransformer -> {self.state_topic} RA={ra_degrees:.6f}° DEC={dec_degrees:.6f}°'
        )
        self.telescope_state_pub.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    node = CoordinateTransformer()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()