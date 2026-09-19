#!/usr/bin/env python3
"""
Follow Mode Controller Node for Telescope Mount
Simple node to enable/disable follow mode and set speed
"""

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool, Float32
import argparse

class FollowModeController(Node):
    def __init__(self, enable=False, speed=1.0):
        super().__init__('follow_mode_controller')
        
        # Publishers
        self.follow_mode_pub = self.create_publisher(Bool, '/telescope/follow_mode', 10)
        self.follow_speed_pub = self.create_publisher(Float32, '/telescope/follow_speed', 10)
        self.torque_pub = self.create_publisher(Bool, '/telescope/torque', 10)
        
        # Parameters
        self.enable = enable
        self.speed = speed
        
        self.get_logger().info(f'Follow Mode Controller: enable={enable}, speed={speed} deg/sec')
        
        # Start with torque enabled
        self.enable_torque()
        
        # Set follow speed
        self.set_follow_speed(speed)
        
        # Enable/disable follow mode as requested
        if enable:
            self.enable_follow_mode()
        else:
            self.disable_follow_mode()
    
    def enable_torque(self):
        """Enable torque on the mount"""
        self.get_logger().info('Enabling torque...')
        msg = Bool()
        msg.data = True
        self.torque_pub.publish(msg)
    
    def enable_follow_mode(self):
        """Enable follow mode"""
        self.get_logger().info(f'Enabling follow mode at {self.speed} deg/sec')
        msg = Bool()
        msg.data = True
        self.follow_mode_pub.publish(msg)
    
    def disable_follow_mode(self):
        """Disable follow mode"""
        self.get_logger().info('Disabling follow mode')
        msg = Bool()
        msg.data = False
        self.follow_mode_pub.publish(msg)
    
    def set_follow_speed(self, speed):
        """Set follow mode speed"""
        self.speed = speed
        self.get_logger().info(f'Setting follow speed to {speed} deg/sec')
        msg = Float32()
        msg.data = speed
        self.follow_speed_pub.publish(msg)

def main():
    parser = argparse.ArgumentParser(description='Telescope Follow Mode Controller')
    parser.add_argument('--enable', action='store_true', help='Enable follow mode on startup')
    parser.add_argument('--disable', action='store_true', help='Disable follow mode on startup')
    parser.add_argument('--speed', type=float, default=1.0, help='Follow speed in degrees/sec')
    
    args = parser.parse_args()
    
    # Handle enable/disable logic
    if args.enable and args.disable:
        print("Error: Cannot specify both --enable and --disable")
        return
    
    enable = args.enable
    if not args.enable and not args.disable:
        # Default to disabled unless explicitly enabled
        enable = False
    
    rclpy.init()
    
    try:
        controller = FollowModeController(enable=enable, speed=args.speed)
        
        if enable:
            print(f"Follow mode enabled at {args.speed} deg/sec. Press Ctrl+C to disable and exit.")
            try:
                rclpy.spin(controller)
            except KeyboardInterrupt:
                print("\nDisabling follow mode...")
                controller.disable_follow_mode()
        else:
            print(f"Follow mode disabled. Use --enable to activate.")
            # Just send the disable command and exit
            rclpy.spin_once(controller, timeout_sec=1.0)
        
    finally:
        controller.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
