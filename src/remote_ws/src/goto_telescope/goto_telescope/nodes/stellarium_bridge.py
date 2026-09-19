#!/usr/bin/env python3
"""Bridge between Stellarium and ROS telescope nodes.

Supports two Stellarium interfaces:
- Telescope Control Plugin (binary protocol, port 10001): 16-byte RA/DEC doubles (radians).
- Arduino/TelescopeServerLX200-like protocol (optional): 20-byte/24-byte.

Modes:
- mode1: manual GUI -> /telescope/state -> stellarium_bridge -> Stellarium
- mode2: remote plugin -> /stellarium/target -> telescope_joint_bridge -> RViz
- mode3: hybrid two-way.
"""

import math
import requests
import rclpy
from rclpy.node import Node
import socket
import struct
import threading
import time

from geometry_msgs.msg import Vector3

try:
    from goto_telescope.msg import TelescopeState
    TELESCOPESTATE_AVAILABLE = True
except ImportError:
    TELESCOPESTATE_AVAILABLE = False
    import dataclasses

    @dataclasses.dataclass
    class TelescopeState:
        header = None
        ra: float = 0.0
        dec: float = 0.0
        is_tracking: bool = False
        mode: str = "simulation"


def _precess_equatorial_deg(ra_deg: float, dec_deg: float, t_centuries: float, inverse: bool = False):
    """Precess equatorial coordinates using IAU 1976 angles (Meeus, Ch. 21).

    forward: J2000 -> JNow
    inverse: JNow -> J2000
    """
    if abs(t_centuries) < 1e-12:
        return ra_deg % 360.0, dec_deg

    arcsec_to_rad = math.pi / (180.0 * 3600.0)
    zeta = (
        2306.2181 * t_centuries
        + 0.30188 * (t_centuries ** 2)
        + 0.017998 * (t_centuries ** 3)
    ) * arcsec_to_rad
    z = (
        2306.2181 * t_centuries
        + 1.09468 * (t_centuries ** 2)
        + 0.018203 * (t_centuries ** 3)
    ) * arcsec_to_rad
    theta = (
        2004.3109 * t_centuries
        - 0.42665 * (t_centuries ** 2)
        - 0.041833 * (t_centuries ** 3)
    ) * arcsec_to_rad

    ra = math.radians(ra_deg)
    dec = math.radians(dec_deg)
    x = math.cos(dec) * math.cos(ra)
    y = math.cos(dec) * math.sin(ra)
    zc = math.sin(dec)

    def rot_z(xv, yv, zv, ang):
        ca = math.cos(ang)
        sa = math.sin(ang)
        return ca * xv - sa * yv, sa * xv + ca * yv, zv

    def rot_y(xv, yv, zv, ang):
        ca = math.cos(ang)
        sa = math.sin(ang)
        return ca * xv + sa * zv, yv, -sa * xv + ca * zv

    if not inverse:
        # J2000 -> JNow : R3(z) * R2(-theta) * R3(zeta)
        x, y, zc = rot_z(x, y, zc, zeta)
        x, y, zc = rot_y(x, y, zc, -theta)
        x, y, zc = rot_z(x, y, zc, z)
    else:
        # JNow -> J2000 : inverse transform
        x, y, zc = rot_z(x, y, zc, -z)
        x, y, zc = rot_y(x, y, zc, theta)
        x, y, zc = rot_z(x, y, zc, -zeta)

    out_ra = math.degrees(math.atan2(y, x)) % 360.0
    out_dec = math.degrees(math.asin(max(-1.0, min(1.0, zc))))
    return out_ra, out_dec


def _julian_centuries_since_j2000_now() -> float:
    jd = (time.time() / 86400.0) + 2440587.5
    return (jd - 2451545.0) / 36525.0


class StellariumBridge(Node):
    def __init__(self):
        super().__init__('stellarium_bridge')

        self.declare_parameter('stellarium.remote_control.host', 'localhost')
        self.declare_parameter('stellarium.remote_control.port', 8090)
        self.declare_parameter('stellarium.telescope_control.host', 'localhost')
        self.declare_parameter('stellarium.telescope_control.port', 10001)
        self.declare_parameter('stellarium.coordinate_system', 'J2000')
        self.declare_parameter('stellarium.use_telescope_control', True)
        self.declare_parameter('stellarium.telescope_control.frame_format', 'size-prefix')
        self.declare_parameter('stellarium.connection_delay', 0.5)
        self.declare_parameter('stellarium.remote_control.ready_check_interval', 1.0)
        self.declare_parameter('stellarium.remote_control.request_timeout', 2.0)
        self.declare_parameter('mode', 'simulation')

        self.remote_host = self.get_parameter('stellarium.remote_control.host').value
        self.remote_port = self.get_parameter('stellarium.remote_control.port').value
        self.remote_ready_interval = self.get_parameter('stellarium.remote_control.ready_check_interval').value
        self.remote_request_timeout = self.get_parameter('stellarium.remote_control.request_timeout').value
        self.telescope_port = self.get_parameter('stellarium.telescope_control.port').value
        self.coord_system = self.get_parameter('stellarium.coordinate_system').value
        self.use_telescope_control = self.get_parameter('stellarium.use_telescope_control').value
        self.telescope_frame_format = self.get_parameter('stellarium.telescope_control.frame_format').value
        self.connection_delay = self.get_parameter('stellarium.connection_delay').value
        self.mode = self.get_parameter('mode').value

        self.telescope_use_size_prefix = (self.telescope_frame_format == 'size-prefix')
        self.coords_are_j2000 = str(self.coord_system).strip().lower() == 'j2000'

        self.current_ra = 0.0
        self.current_dec = 90.0

        self.remote_api_ready = False
        self.last_remote_ready_log = 0.0

        self.telescope_server_socket = None
        self.telescope_client_socket = None
        self.telescope_connected = False
        self.server_running = False
        self.client_lock = threading.Lock()

        self.target_pub = self.create_publisher(Vector3, '/stellarium/target', 10)

        self.create_subscription(Vector3, '/telescope/state', self.telescope_state_callback, 10)
        self.get_logger().info('Subscribed to /telescope/state (Vector3)')

        self.api_probe_thread = threading.Thread(target=self._remote_api_probe_loop)
        self.api_probe_thread.daemon = True
        self.api_probe_thread.start()

        if self.use_telescope_control:
            self.telescope_thread = threading.Thread(target=self.telescope_server_loop)
            self.telescope_thread.daemon = True
            self.telescope_thread.start()
            self.get_logger().info(f'Telescope Control listening on port {self.telescope_port}')
        else:
            self.telescope_connected = False
            self.get_logger().info('Manual mode: disabled Telescope Control plugin port (10001), using remote API')

        self.get_logger().info('StellariumBridge started')
        self.get_logger().info(f'Mode: {self.mode}')
        self.get_logger().info(f'Telescope Control listening on port {self.telescope_port}')
        self.get_logger().info(
            f'Coordinate handling: {"J2000 <-> JNow precession enabled" if self.coords_are_j2000 else "JNow passthrough"}'
        )

    def _stellarium_to_mount_deg(self, ra_deg: float, dec_deg: float):
        """Convert Stellarium coordinates to mount (JNow) coordinates."""
        if not self.coords_are_j2000:
            return ra_deg % 360.0, dec_deg
        t = _julian_centuries_since_j2000_now()
        return _precess_equatorial_deg(ra_deg, dec_deg, t, inverse=False)

    def _mount_to_stellarium_deg(self, ra_deg: float, dec_deg: float):
        """Convert mount (JNow) coordinates to Stellarium coordinate system."""
        if not self.coords_are_j2000:
            return ra_deg % 360.0, dec_deg
        t = _julian_centuries_since_j2000_now()
        return _precess_equatorial_deg(ra_deg, dec_deg, t, inverse=True)

    @staticmethod
    def _normalize_ra_deg(value: float) -> float:
        return value % 360.0

    @staticmethod
    def _coerce_ra_to_deg(value: float) -> float:
        # /telescope/state contract is RA in degrees.
        # Do not infer hours based on numeric range because valid RA degrees in [0, 24]
        # would be misinterpreted and produce pointing offsets.
        return value % 360.0

    @staticmethod
    def _astro_to_stellarium_bytes(ra_deg: float, dec_deg: float):
        ra_rad = (ra_deg % 360.0) * (3.141592653589793 / 180.0)
        dec_rad = dec_deg * (3.141592653589793 / 180.0)
        # Use little-endian doubles to match Stellarium TCP internal binary format
        return struct.pack('<dd', ra_rad, dec_rad)

    def _build_telescope_packet(self, ra_deg: float, dec_deg: float):
        data = self._astro_to_stellarium_bytes(ra_deg, dec_deg)
        if self.telescope_use_size_prefix:
            # Add 4-byte length prefix, little-endian
            return struct.pack('<I', len(data)) + data
        return data

    def _remote_api_probe_loop(self):
        self.get_logger().info('Starting Stellarium remote API readiness probe')
        url = f'http://{self.remote_host}:{self.remote_port}/api/main/status'
        while rclpy.ok() and not self.remote_api_ready:
            try:
                r = requests.get(url, timeout=self.remote_request_timeout)
                if r.status_code == 200:
                    self.remote_api_ready = True
                    self.get_logger().info('✅ Stellarium remote API is ready (status endpoint reachable)')
                    break
            except requests.exceptions.RequestException as e:
                now = time.time()
                if now - self.last_remote_ready_log > 5.0:
                    self.last_remote_ready_log = now
                    self.get_logger().debug(f'Stellarium API not ready yet: {e}')
            time.sleep(self.remote_ready_interval)

    @staticmethod
    def _stellarium_bytes_to_astro(data: bytes):
        # Incoming payload matches little-endian format used by Telescope Control plugin.
        ra_rad, dec_rad = struct.unpack('<dd', data[:16])
        ra_deg = (ra_rad * (180.0 / 3.141592653589793)) % 360.0
        dec_deg = dec_rad * (180.0 / 3.141592653589793)
        return ra_deg, dec_deg

    @staticmethod
    def _arduino_bytes_to_astro(data: bytes):
        if len(data) < 20:
            raise ValueError('Arduino protocol packet too short')
        ra_proto = int.from_bytes(data[12:16], byteorder='little', signed=False)
        dec_proto = int.from_bytes(data[16:20], byteorder='little', signed=True)
        ra_hours = float(ra_proto) * 12.0 / 2147483648.0
        dec_deg = float(dec_proto) * 90.0 / 1073741824.0
        return ra_hours, dec_deg

    def telescope_state_callback(self, msg):
        self.current_ra = self._coerce_ra_to_deg(float(msg.x))
        self.current_dec = msg.y
        out_ra, out_dec = self._mount_to_stellarium_deg(self.current_ra, self.current_dec)

        if self.use_telescope_control:
            if self.telescope_connected:
                self.send_telescope_position(out_ra, out_dec)
            else:
                # Fallback to remote API view so Stellarium position is not stuck at zero
                if self.remote_api_ready:
                    self._set_stellarium_view(out_ra, out_dec)
        else:
            # Vector3 message from coordinate_transformer uses x=RA, y=DEC
            self._set_stellarium_view(out_ra, out_dec)

    def _set_stellarium_view(self, ra_deg, dec_deg):
        if not self.remote_api_ready:
            # avoid ties when Stellarium load just started
            self.get_logger().debug('Stellarium remote API not ready yet, skipping view command')
            return

        try:
            ra_rad = self._normalize_ra_deg(float(ra_deg)) * (3.141592653589793 / 180.0)
            dec_rad = dec_deg * (3.141592653589793 / 180.0)
            x = math.cos(dec_rad) * math.cos(ra_rad)
            y = math.cos(dec_rad) * math.sin(ra_rad)
            z = math.sin(dec_rad)
            url = f'http://{self.remote_host}:{self.remote_port}/api/main/view'
            data = {
                'j2000': f'[{x:.8f},{y:.8f},{z:.8f}]',
                'ref': 'auto'
            }
            r = requests.post(url, data=data, timeout=self.remote_request_timeout)
            if r.status_code == 200:
                self.get_logger().debug(f'Set remote Stellarium view RA={ra_deg:.4f}° DEC={dec_deg:.4f}° via API')
            else:
                self.get_logger().warning(f'Stellarium API view set failed {r.status_code}: {r.text}')
        except requests.exceptions.ConnectionError as e:
            self.get_logger().debug(f'Stellarium API view connection error: {e}')
            self.remote_api_ready = False
        except requests.exceptions.RequestException as e:
            self.get_logger().warning(f'Stellarium API view set exception: {e}')
        except Exception as e:
            self.get_logger().warning(f'Stellarium API view set exception: {e}')

    def send_telescope_position(self, ra_deg, dec_deg):
        if not self.telescope_connected or self.telescope_client_socket is None:
            return False

        payload = self._build_telescope_packet(ra_deg, dec_deg)

        try:
            with self.client_lock:
                self.telescope_client_socket.sendall(payload)
            summary = 'size-prefixed' if self.telescope_use_size_prefix else 'raw'
            self.get_logger().debug(
                f'Sent {summary} Stellarium pos RA={ra_deg:.4f}° DEC={dec_deg:.4f}° (payload {len(payload)} bytes)'
            )
            return True
        except (ConnectionResetError, BrokenPipeError, OSError) as e:
            self.get_logger().warning(f'Send failed: {e}')
            self.telescope_connected = False
            return False
        except Exception as e:
            self.get_logger().error(f'Unexpected send error: {e}')
            return False

    def telemetry_loop(self):
        while rclpy.ok() and self.telescope_connected:
            out_ra, out_dec = self._mount_to_stellarium_deg(self.current_ra, self.current_dec)
            self.send_telescope_position(out_ra, out_dec)
            time.sleep(0.1)

    def telescope_server_loop(self):
        self.get_logger().info('Starting Telescope Control server...')

        while rclpy.ok():
            try:
                self.telescope_server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.telescope_server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                self.telescope_server_socket.bind(('0.0.0.0', self.telescope_port))
                self.telescope_server_socket.listen(1)
                self.telescope_server_socket.settimeout(1.0)
                self.server_running = True

                self.get_logger().info(f'✅ Telescope Control server listening on port {self.telescope_port}')

                while rclpy.ok() and self.server_running:
                    try:
                        self.get_logger().info('Waiting for Stellarium connection...', throttle_duration_sec=10.0)
                        client_socket, client_address = self.telescope_server_socket.accept()
                        self.get_logger().info(f'✅ Stellarium connected from {client_address}')

                        self.telescope_client_socket = client_socket
                        self.telescope_client_socket.settimeout(1.0)
                        self.telescope_connected = True

                        if self.connection_delay > 0:
                            time.sleep(self.connection_delay)

                        telemetry_thread = threading.Thread(target=self.telemetry_loop)
                        telemetry_thread.daemon = True
                        telemetry_thread.start()

                        # Send immediate packet to avoid Stellarium side read-timeouts
                        out_ra, out_dec = self._mount_to_stellarium_deg(self.current_ra, self.current_dec)
                        self.send_telescope_position(out_ra, out_dec)

                        while rclpy.ok() and self.telescope_connected:
                            try:
                                data = self.telescope_client_socket.recv(1024)
                                if not data:
                                    self.get_logger().info('Stellarium disconnected')
                                    break

                                try:
                                    # If Stellarium uses framed protocol, it may send 4-byte length prefix
                                    if len(data) >= 4:
                                        possible_length = struct.unpack('<I', data[:4])[0]
                                        if possible_length in (16, 20, 24) and len(data) >= 4 + possible_length:
                                            framed = data[4:4+possible_length]
                                            if possible_length == 16:
                                                ra_deg, dec_deg = self._stellarium_bytes_to_astro(framed)
                                            else:
                                                ra_hours, dec_deg = self._arduino_bytes_to_astro(framed)
                                                ra_deg = (ra_hours * 15.0) % 360.0
                                        elif len(data) >= 16 and len(data) < 20:
                                            # raw Stellarium byte packet
                                            ra_deg, dec_deg = self._stellarium_bytes_to_astro(data)
                                        elif len(data) in (20, 24):
                                            ra_hours, dec_deg = self._arduino_bytes_to_astro(data)
                                            ra_deg = (ra_hours * 15.0) % 360.0
                                        else:
                                            self.get_logger().debug(f'Ignored unknown TCP length {len(data)}')
                                            continue
                                    else:
                                        self.get_logger().debug(f'Ignored tiny packet length {len(data)}')
                                        continue

                                    ra_deg, dec_deg = self._stellarium_to_mount_deg(ra_deg, dec_deg)

                                    self.get_logger().info(f'Received Stellarium target: RA={ra_deg:.4f}° DEC={dec_deg:.4f}°')
                                    self.current_ra = ra_deg
                                    self.current_dec = dec_deg

                                    target_msg = Vector3()
                                    target_msg.x = ra_deg
                                    target_msg.y = dec_deg
                                    target_msg.z = 0.0
                                    self.target_pub.publish(target_msg)

                                except Exception as e:
                                    self.get_logger().warning(f'Incoming protocol parse error: {e}')

                            except socket.timeout:
                                continue
                            except (ConnectionResetError, BrokenPipeError):
                                self.get_logger().info('Connection lost')
                                break
                            except Exception as e:
                                self.get_logger().error(f'Error reading from Stellarium: {e}')
                                break

                        self.telescope_connected = False
                        self.telescope_client_socket.close()
                        self.telescope_client_socket = None

                    except socket.timeout:
                        continue
                    except Exception as e:
                        self.get_logger().error(f'Error accepting connection: {e}')
                        time.sleep(1.0)

            except OSError as e:
                self.get_logger().error(f'Failed to start server: {e}')
                if 'Address already in use' in str(e):
                    self.get_logger().error(f'Port {self.telescope_port} is already in use!')
                time.sleep(5.0)
            except Exception as e:
                self.get_logger().error(f'Unexpected error in server loop: {e}')
                time.sleep(5.0)


def main(args=None):
    rclpy.init(args=args)
    node = StellariumBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
