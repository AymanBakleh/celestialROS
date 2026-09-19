#!/usr/bin/env python3
"""
Telescope Driver Node for ROS2

Receives GOTO targets from Stellarium (/stellarium/target), sends to mount,
and publishes mount feedback for RViz visualization.

Modes:
- Serial (default): Connects via USB serial to Arduino
- WiFi: Connects via TCP socket to Arduino ESP32 WiFi (SSID: Jarspace, pass: 12345678)

Mount Protocol:
- Send T=1 {"ra_deg": X, "dec_deg": Y} for GOTO
- Receive T=2 feedback with ra_deg, dec_deg, hour_angle_deg, local_sidereal_time_deg
"""

import rclpy
from rclpy.node import Node
import json
import serial
from serial import SerialException
from sensor_msgs.msg import JointState
from geometry_msgs.msg import Vector3
import queue
import threading
import time
import math
import socket


DEFAULT_SERIAL_PORT = "/dev/ttyUSB0"
DEFAULT_BAUD_RATE = 115200
DEFAULT_WIFI_IP = "192.168.4.1"
DEFAULT_WIFI_PORT = 10001


class ReadLine:
    """Buffered line reader for serial connections."""
    def __init__(self, s):
        self.buf = bytearray()
        self.s = s

    def readline(self):
        i = self.buf.find(b"\n")
        if i >= 0:
            r = self.buf[:i+1]
            self.buf = self.buf[i+1:]
            return r
        while True:
            i = max(1, min(512, self.s.in_waiting))
            data = self.s.read(i)
            i = data.find(b"\n")
            if i >= 0:
                r = self.buf + data[:i+1]
                self.buf[0:] = data[i+1:]
                return r
            else:
                self.buf.extend(data)

    def clear_buffer(self):
        self.s.reset_input_buffer()


class TelescopeDriver(Node):
    """ROS2 node that bridges Stellarium commands to telescope mount."""

    def __init__(self):
        super().__init__('telescope_driver')

        # Declare parameters
        self.declare_parameter('connection_mode', 'serial')  # 'serial' or 'wifi'
        self.declare_parameter('serial_port', DEFAULT_SERIAL_PORT)
        self.declare_parameter('baud_rate', DEFAULT_BAUD_RATE)
        self.declare_parameter('wifi_ip', DEFAULT_WIFI_IP)
        self.declare_parameter('wifi_port', DEFAULT_WIFI_PORT)
        self.declare_parameter('debug_rate', 1.0)  # Status print rate in Hz
        self.declare_parameter('target_min_interval_sec', 0.6)
        self.declare_parameter('target_min_delta_deg', 0.25)
        self.declare_parameter('ignore_near_current_deg', 0.35)

        # Get parameters
        self.connection_mode = self.get_parameter('connection_mode').get_parameter_value().string_value
        self.serial_port_name = self.get_parameter('serial_port').get_parameter_value().string_value
        self.baud_rate = self.get_parameter('baud_rate').get_parameter_value().integer_value
        self.wifi_ip = self.get_parameter('wifi_ip').get_parameter_value().string_value
        self.wifi_port = self.get_parameter('wifi_port').get_parameter_value().integer_value
        self.debug_rate = self.get_parameter('debug_rate').get_parameter_value().double_value
        self.target_min_interval_sec = self.get_parameter('target_min_interval_sec').get_parameter_value().double_value
        self.target_min_delta_deg = self.get_parameter('target_min_delta_deg').get_parameter_value().double_value
        self.ignore_near_current_deg = self.get_parameter('ignore_near_current_deg').get_parameter_value().double_value

        # Connection handles
        self.serial_conn = None
        self.wifi_socket = None
        self.rl = None
        self.command_queue = queue.Queue()

        # State
        self.current_ra_deg = 0.0
        self.current_dec_deg = 90.0
        self.target_ra_deg = None
        self.target_dec_deg = None
        self.is_moving = False
        self.tracking_mode = "Unknown"
        self.local_time = "--:--:--"
        self._received_valid_feedback = False
        self._last_feedback_time = time.time()
        self._last_target_cmd_time = 0.0
        self._last_cmd_ra_deg = None
        self._last_cmd_dec_deg = None

        # Connect to mount
        if self.connection_mode == 'wifi':
            self._connect_wifi()
        else:
            self._connect_serial()

        # ROS2 interfaces
        # Subscribe to Stellarium targets (RA in hours or degrees, DEC in degrees)
        self.stellarium_target_sub = self.create_subscription(
            Vector3, '/stellarium/target', self.stellarium_target_callback, 10)
        self.skyview_target_sub = self.create_subscription(
            Vector3, '/skyview/target', self.skyview_target_callback, 10)

        # Publisher for RViz (hardware joint states)
        self.joint_pub = self.create_publisher(
            JointState, '/telescope/hardware_joint_states', 10)

        # Publish mount state for Stellarium bridge (RA in hours, DEC in degrees).
        self.state_pub = self.create_publisher(Vector3, '/telescope/state', 10)



        # Start threads
        self._running = True
        self.command_thread = threading.Thread(target=self._process_commands, daemon=True)
        self.command_thread.start()

        # Timers
        self.feedback_timer = self.create_timer(0.1, self._read_feedback)  # 10Hz
        self.debug_timer = self.create_timer(1.0 / self.debug_rate, self._print_status)

        self.get_logger().info(f"TelescopeDriver started in {self.connection_mode.upper()} mode")

    def _connect_serial(self):
        """Connect to mount via serial port."""
        try:
            self.serial_conn = serial.Serial(
                self.serial_port_name,
                self.baud_rate,
                timeout=1.0
            )
            self.rl = ReadLine(self.serial_conn)
            self.get_logger().info(
                f"Connected to serial port {self.serial_port_name} @ {self.baud_rate} baud"
            )
            # Send init command
            self._send_command({'T': 605, 'cmd': 0})
        except SerialException as e:
            self.get_logger().error(f"Failed to connect to serial port: {e}")
            raise

    def _connect_wifi(self):
        """Connect to mount via WiFi TCP socket."""
        try:
            self.wifi_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.wifi_socket.settimeout(5.0)
            self.get_logger().info(f"Connecting to WiFi {self.wifi_ip}:{self.wifi_port}...")
            self.wifi_socket.connect((self.wifi_ip, self.wifi_port))
            self.wifi_socket.settimeout(0.1)  # Non-blocking for reads
            self.get_logger().info(f"Connected to WiFi at {self.wifi_ip}:{self.wifi_port}")
            # Send init command
            self._send_command({'T': 605, 'cmd': 0})
        except Exception as e:
            self.get_logger().error(f"Failed to connect via WiFi: {e}")
            raise

    def _send_command(self, data):
        """Queue a command to send to the mount."""
        self.command_queue.put(data)

    def _process_commands(self):
        """Background thread to send commands to mount."""
        while self._running:
            try:
                data = self.command_queue.get(timeout=0.1)
                cmd_str = json.dumps(data) + '\n'
                cmd_bytes = cmd_str.encode('utf-8')

                if self.connection_mode == 'wifi' and self.wifi_socket:
                    self.wifi_socket.sendall(cmd_bytes)
                elif self.serial_conn and self.serial_conn.is_open:
                    self.serial_conn.write(cmd_bytes)

            except queue.Empty:
                continue
            except Exception as e:
                self.get_logger().debug(f"Command send error: {e}")

    def _read_wifi_line(self):
        """Read a line from WiFi socket."""
        buffer = b''
        try:
            while not buffer.endswith(b'\n'):
                chunk = self.wifi_socket.recv(1)
                if not chunk:
                    return None
                buffer += chunk
            return buffer
        except socket.timeout:
            return None
        except Exception:
            return None

    def _read_feedback(self):
        """Read T=2 feedback from mount and publish to ROS."""
        try:
            if self.connection_mode == 'serial' and self.serial_conn:
                if self.serial_conn.in_waiting > 0:
                    line = self.rl.readline().decode('utf-8').strip()
                else:
                    self._check_feedback_timeout()
                    return
            elif self.connection_mode == 'wifi' and self.wifi_socket:
                data = self._read_wifi_line()
                if data:
                    line = data.decode('utf-8').strip()
                else:
                    self._check_feedback_timeout()
                    return
            else:
                self._check_feedback_timeout()
                return

            if not line:
                self._check_feedback_timeout()
                return

            try:
                feedback = json.loads(line)
            except json.JSONDecodeError:
                self._check_feedback_timeout()
                return  # Ignore incomplete lines

            if feedback.get('T') != 2:
                self._check_feedback_timeout()
                return  # Only process T=2 feedback

            # Only handle ra_deg and dec_deg
            self.current_ra_deg = float(feedback.get('ra_deg', 0.0)) % 360.0
            self.current_dec_deg = float(feedback.get('dec_deg', 0.0))
            self._received_valid_feedback = True
            self._last_feedback_time = time.time()
            # Debug: Log what we received from Arduino
            self.get_logger().info(f'Arduino feedback: RA={self.current_ra_deg:.6f}° DEC={self.current_dec_deg:.6f}°')

            # Publish joint states for RViz
            js = JointState()
            js.header.stamp = self.get_clock().now().to_msg()
            js.header.frame_id = 'telescope'
            js.name = ['ra_joint', 'dec_joint']
            # Keep joint convention aligned with coordinate_transformer/telescope_joint_bridge:
            # - ra_joint: radians(ra_deg)
            # - dec_joint: radians(dec_deg), clamped to [-pi/2, +pi/2]
            dec_joint = math.radians(self.current_dec_deg)
            dec_joint = max(-math.pi / 2.0, min(math.pi / 2.0, dec_joint))
            js.position = [
                math.radians(self.current_ra_deg),
                dec_joint
            ]
            js.velocity = [0.0, 0.0]
            self.joint_pub.publish(js)

            # Publish telescope state for Stellarium bridge
            state = Vector3()
            state.x = float(self.current_ra_deg)  # RA in degrees (/telescope/state contract)
            state.y = float(self.current_dec_deg)
            state.z = 0.0  # No movement info
            self.state_pub.publish(state)

        except Exception as e:
            self.get_logger().debug(f"Feedback read error: {e}")

    def _check_feedback_timeout(self):
        # Only warn if we have never received valid feedback
        if not self._received_valid_feedback:
            if time.time() - self._last_feedback_time > 2.0:
                self.get_logger().warn("No valid feedback received from mount yet. Not publishing state.")
        # If we have received feedback before, do nothing (just skip this cycle)

    def stellarium_target_callback(self, msg):
        self.target_callback(msg, source='stellarium')

    def skyview_target_callback(self, msg):
        self.target_callback(msg, source='skyview')

    def target_callback(self, msg, source='stellarium'):
        """Receive GOTO target from Stellarium or Skyview.

        msg.x = RA in hours or degrees
        msg.y = DEC in degrees (-90 to +90)
        """
        ra_value = float(msg.x)
        if abs(ra_value) <= 24.0:
            ra_deg = (ra_value % 24.0) * 15.0
        else:
            ra_deg = ra_value % 360.0
        dec_deg = max(-90.0, min(90.0, float(msg.y)))

        def ra_distance_deg(a, b):
            d = abs((a - b) % 360.0)
            return min(d, 360.0 - d)

        now = time.time()

        # Drop near-duplicate target spam that causes move-stop-move oscillation.
        if self._last_cmd_ra_deg is not None and self._last_cmd_dec_deg is not None:
            dra = ra_distance_deg(ra_deg, self._last_cmd_ra_deg)
            ddec = abs(dec_deg - self._last_cmd_dec_deg)
            if dra < self.target_min_delta_deg and ddec < self.target_min_delta_deg:
                if now - self._last_target_cmd_time < self.target_min_interval_sec:
                    return

        # Ignore feedback-loop echoes: target ~= current mount position.
        current_dra = ra_distance_deg(ra_deg, self.current_ra_deg)
        current_ddec = abs(dec_deg - self.current_dec_deg)
        if current_dra < self.ignore_near_current_deg and current_ddec < self.ignore_near_current_deg:
            return

        self.target_ra_deg = ra_deg
        self.target_dec_deg = dec_deg
        
        # Convert to hours for logging only
        ra_hours = ra_deg / 15.0

        self.get_logger().info(
            f"GOTO Target from {source}: RA={ra_hours:.4f}h ({ra_deg:.2f}°), DEC={dec_deg:.2f}°"
        )

        # Send T=1 command to mount with RA in degrees
        cmd = {
            'T': 19,
            'ra_deg': ra_deg,
            'dec_deg': dec_deg
        }
        self._send_command(cmd)
        self._last_target_cmd_time = now
        self._last_cmd_ra_deg = ra_deg
        self._last_cmd_dec_deg = dec_deg

    def _print_status(self):
        """Print detailed status to terminal for debugging."""
        if not self._received_valid_feedback:
            self.get_logger().warn("No valid feedback received from mount yet. Not publishing state.")
            return
        ra_hours = self.current_ra_deg / 15.0

        status_icon = "Moving" if self.is_moving else "Idle"
        mode_str = f"[{self.tracking_mode}]" if not self.is_moving else "[MOVING]"

        # Target info
        if self.target_ra_deg is not None and self.target_dec_deg is not None:
            target_ra_h = self.target_ra_deg / 15.0
            target_str = f"Target: RA={target_ra_h:.4f}h DEC={self.target_dec_deg:.2f} | "
        else:
            target_str = ""

        self.get_logger().info(
            f"[{status_icon}] {mode_str} | "
            f"{target_str}"
            f"Current: RA={ra_hours:.4f}h ({self.current_ra_deg:.2f}) "
            f"DEC={self.current_dec_deg:.2f} | "
        )

    def destroy_node(self):
        """Cleanup when node is destroyed."""
        self._running = False
        if self.command_thread.is_alive():
            self.command_thread.join(timeout=1.0)
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
        if self.wifi_socket:
            self.wifi_socket.close()
        super().destroy_node()


def main_serial(args=None):
    """Main entry point for serial mode (default)."""
    rclpy.init(args=args)
    node = TelescopeDriver()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


def main_wifi(args=None):
    """Main entry point for WiFi mode."""
    rclpy.init(args=args)
    # Override connection_mode via parameter
    node = TelescopeDriver()
    # Re-set connection mode to wifi after init (parameter is already set via launch)
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main_serial()
