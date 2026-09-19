#!/usr/bin/env python3
"""Serial bridge for Stellarium LX200-style protocol.

This node creates a stable serial endpoint for Stellarium (custom or pseudo-TTY).
It accepts basic LX200 commands and publishes '/stellarium/target' (RA/DEC) for
`telescope_joint_bridge` in hardware mode.

Supported commands:
  :GR#   -> current RA
  :GD#   -> current DEC
  :SrHH:MM:SS# -> set target RA
  :Sd+DD*MM:SS# / :Sd-DD*MM:SS# -> set target DEC
  :MS#   -> execute goto to pending target
  :Q#    -> status query
  :GQ#   -> status (returns 0)
  :ME#   -> stop

If serial_port is missing/unreadable, a new pseudo tty is created and printed.
"""

import math
import os
import re
import select
import struct
import threading
import time

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Vector3
from std_msgs.msg import String
from sensor_msgs.msg import JointState

try:
    import serial
    from serial import SerialException
except ImportError:
    serial = None
    SerialException = Exception


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


def _parse_ra(ra_str: str):
    m = re.match(r"^(\d{1,2}):(\d{1,2}):(\d{1,2})$", ra_str)
    if not m:
        return None
    h, mm, ss = (int(x) for x in m.groups())
    if h < 0 or h >= 24 or mm < 0 or mm >= 60 or ss < 0 or ss >= 60:
        return None
    return h + mm / 60.0 + ss / 3600.0


def _parse_dec(dec_str: str):
    # Accept formats: +DD*MM:SS or +DD:MM:SS
    m = re.match(r"^([+-])(\d{1,2})[\*:,](\d{1,2})[:](\d{1,2})$", dec_str)
    if not m:
        return None
    sign, dd, mm, ss = m.groups()
    dd_i = int(dd)
    mm_i = int(mm)
    ss_i = int(ss)
    if dd_i < 0 or dd_i > 90 or mm_i < 0 or mm_i >= 60 or ss_i < 0 or ss_i >= 60:
        return None
    val = dd_i + mm_i / 60.0 + ss_i / 3600.0
    return val if sign == '+' else -val


class StellariumSerialBridge(Node):
    def __init__(self):
        super().__init__('stellarium_serial_bridge')

        self.declare_parameter('serial_port', '/dev/ttyS0')
        self.declare_parameter('baud_rate', 9600)
        self.declare_parameter('use_pseudo_tty', True)
        self.declare_parameter('stellarium.coordinate_system', 'J2000')
        self.declare_parameter('connection_delay', 0.5)
        self.declare_parameter('mirror_state_to_target', False)

        self.serial_port = self.get_parameter('serial_port').value
        self.baud_rate = self.get_parameter('baud_rate').value
        self.use_pseudo_tty = self.get_parameter('use_pseudo_tty').value
        self.coord_system = self.get_parameter('stellarium.coordinate_system').value
        self.connection_delay = self.get_parameter('connection_delay').value
        self.mirror_state_to_target = bool(self.get_parameter('mirror_state_to_target').value)

        self.current_ra = 0.0
        self.current_dec = 0.0
        self.pending_ra = None
        self.pending_dec = None

        self.target_pub = self.create_publisher(Vector3, '/stellarium/target', 10)
        self.telescope_state_pub = self.create_publisher(Vector3, '/telescope/state', 10)
        self.raw_rx_pub = self.create_publisher(String, '/stellarium/raw_rx', 10)
        self.raw_tx_pub = self.create_publisher(String, '/stellarium/raw_tx', 10)
        self.create_subscription(Vector3, '/telescope/state', self._telescope_state_cb, 10)
        self.create_subscription(JointState, '/joint_states_merged', self._joint_states_merged_cb, 10)

        self._is_running = True
        self._serial_lock = threading.Lock()

        self.serial_handle = None
        self.serial_fd = None
        self.serial_connected = False

        self._setup_serial()

        if self.serial_handle is None:
            self.get_logger().error('Stellarium serial bridge failed to start.')
            return

        self._reader_thread = threading.Thread(target=self._read_loop, daemon=True)
        self._reader_thread.start()

        self.get_logger().info(f'Stellarium serial bridge active (coord system={self.coord_system}).')

    def _setup_serial(self):
        if self.use_pseudo_tty:
            self.get_logger().info('Creating pseudo-TTY pair for Stellarium.')
            try:
                master_fd, slave_fd = os.openpty()
                slave_name = os.ttyname(slave_fd)
                try:
                    os.chmod(slave_name, 0o666)
                    self.get_logger().debug(f'Set permissions for pseudo-TTY {slave_name} to 666')
                except Exception as e:
                    self.get_logger().warning(f'Failed to chmod pseudo-TTY {slave_name}: {e}')
                os.close(slave_fd)
                self.serial_port = slave_name
                self.get_logger().info(f'Pseudo-TTY endpoint: {slave_name} (use this in Stellarium)')
                self.serial_handle = os.fdopen(master_fd, 'r+b', buffering=0)
                self.serial_fd = master_fd
                return
            except Exception as e:
                self.get_logger().error(f'Failed to create pty pair: {e}')

        if not os.path.exists(self.serial_port):
            self.get_logger().warn(
                f"Configured serial port '{self.serial_port}' does not exist. Fall back to pseudo-TTY mode."
            )
            try:
                master_fd, slave_fd = os.openpty()
                slave_name = os.ttyname(slave_fd)
                os.close(slave_fd)
                self.serial_port = slave_name
                self.get_logger().info(f'Fallback pseudo-TTY endpoint: {slave_name}')
                self.serial_handle = os.fdopen(master_fd, 'r+b', buffering=0)
                self.serial_fd = master_fd
                return
            except Exception as e:
                self.get_logger().error(f'Fallback pty creation failed: {e}')
                self.serial_handle = None
                return

        if serial is None:
            self.get_logger().error('pyserial is not installed; cannot open real serial port.')
            self.serial_handle = None
            return

        try:
            ser = serial.Serial(self.serial_port, self.baud_rate, timeout=0.1)
            self.serial_handle = ser
            self.serial_fd = ser.fileno()
            self.get_logger().info(f'Opened serial port {self.serial_port} @ {self.baud_rate}')
        except SerialException as e:
            self.get_logger().error(f'Failed to open serial port {self.serial_port}: {e}')
            self.serial_handle = None

    def _telescope_state_cb(self, msg: Vector3):
        self.current_ra = float(msg.x)
        self.current_dec = float(msg.y)

        if self.mirror_state_to_target:
            # Optional debug mode only: mirroring can create target feedback loops.
            target_msg = Vector3(x=self.current_ra, y=self.current_dec, z=0.0)
            self.target_pub.publish(target_msg)
            self.get_logger().debug(
                f'Mirror /telescope/state into /stellarium/target RA={self.current_ra:.6f} DEC={self.current_dec:.6f}'
            )

    def _joint_states_merged_cb(self, msg: JointState):
        # If joint-state values available, mirror to telescope state for Stellarium
        if 'ra_joint' in msg.name and 'dec_joint' in msg.name:
            try:
                ra_joint = msg.position[msg.name.index('ra_joint')]
                dec_joint = msg.position[msg.name.index('dec_joint')]
                ra_deg = math.degrees(ra_joint) % 360.0
                dec_deg = math.degrees(dec_joint)
                self.current_ra = ra_deg
                self.current_dec = dec_deg
                state_msg = Vector3(x=ra_deg, y=dec_deg, z=0.0)
                self.telescope_state_pub.publish(state_msg)
                self.get_logger().debug(f'Joint merged -> telescope/state RA={ra_deg:.4f}° DEC={dec_deg:.4f}°')
            except Exception as e:
                self.get_logger().warn(f'Failed convert merged joint state: {e}')

    def _publish_target(self, ra_deg, dec_deg):
        self.current_ra = ra_deg
        self.current_dec = dec_deg

        target_msg = Vector3()
        target_msg.x = float(ra_deg)
        target_msg.y = float(dec_deg)
        target_msg.z = 0.0

        self.target_pub.publish(target_msg)
        self.telescope_state_pub.publish(target_msg)

        self.get_logger().info(f'Published target RA={ra_deg:.6f}° DEC={dec_deg:.6f}°')

    def _build_stellarium_packet(self, ra_deg, dec_deg):
        # Stellarium protocol expects RA in hours in packet framing
        ra_hours = (ra_deg % 360.0) / 15.0
        msize = 0x1800
        mtype = 0x0000
        mtime = int(time.time())
        ra_proto = int(ra_hours * 2147483648.0 / 12.0) & 0xFFFFFFFF
        dec_proto = int(dec_deg * 1073741824.0 / 90.0)
        return struct.pack('<HHQIi', msize, mtype, mtime, ra_proto, dec_proto)

    def _parse_stellarium_packet(self, packet: bytes):
        if len(packet) < 20:
            return None
        try:
            ra_proto = int.from_bytes(packet[12:16], byteorder='little', signed=False)
            dec_proto = int.from_bytes(packet[16:20], byteorder='little', signed=True)
            ra_hours = float(ra_proto) * 12.0 / 2147483648.0
            dec_deg = float(dec_proto) * 90.0 / 1073741824.0
            ra_deg = (ra_hours * 15.0) % 360.0
            return ra_deg, dec_deg
        except Exception as e:
            self.get_logger().debug(f'Failed to parse stellarium packet: {e}')
            return None

    def _read_loop(self):
        buffer = bytearray()
        while rclpy.ok() and self._is_running:
            try:
                if self.serial_handle is None:
                    time.sleep(0.1)
                    continue

                rlist, _, _ = select.select([self.serial_fd], [], [], 0.1)
                if not rlist:
                    continue

                if isinstance(self.serial_handle, serial.Serial):
                    data = self.serial_handle.read(256)
                else:
                    data = os.read(self.serial_fd, 256)

                if not data:
                    continue

                raw = data.decode('ascii', errors='replace') if isinstance(data, (bytes, bytearray)) else str(data)
                self.raw_rx_pub.publish(String(data=raw))
                self.get_logger().info(f'Serial RX: {raw}')

                if not self.serial_connected:
                    self.serial_connected = True
                    self.get_logger().info(f"Serial endpoint opened by peer: {self.serial_port}")

                buffer.extend(data)

                # Handle ASCII command bytes (LX200 style)
                while b'#' in buffer:
                    idx = buffer.index(b'#')
                    packet = buffer[: idx + 1]
                    del buffer[: idx + 1]
                    text = packet.decode('ascii', errors='ignore').strip()
                    if not text:
                        continue
                    # Normalize received protocol in case there are extra # tokens
                    text = text.strip('#')
                    if not text.startswith(':'):
                        text = ':' + text
                    if not text.endswith('#'):
                        text = text + '#'
                    self.get_logger().debug(f'Received serial command: raw={packet!r} norm={text}')
                    self._handle_command(text)

                # Handle binary Stellarium telescope protocol data (20-byte frames)
                while len(buffer) >= 20 and not buffer.startswith(b':'):
                    packet = bytes(buffer[:20])
                    del buffer[:20]
                    parsed = self._parse_stellarium_packet(packet)
                    if parsed is not None:
                        ra_deg, dec_deg = parsed
                        self.get_logger().info(f'Received Stellarium binary target RA={ra_deg:.4f}° DEC={dec_deg:.4f}°')
                        self._publish_target(ra_deg, dec_deg)
                        # echo back current position
                        try:
                            resp = self._build_stellarium_packet(self.current_ra, self.current_dec)
                            self._write_response_bytes(resp)
                        except Exception as e:
                            self.get_logger().warn(f'Error writing response packet: {e}')

                # Some non-command bytes may remain; keep them until more data arrives

            except OSError as e:
                # EIO or ENXIO happens when PTY slave is not yet opened by Stellarium client
                if getattr(e, 'errno', None) in (5, 6):
                    self.get_logger().debug(f'Serial temporary state: {e}')
                else:
                    self.get_logger().warn(f'Serial read error: {e}')
                time.sleep(0.2)
            except Exception as e:
                self.get_logger().warn(f'Serial read error: {e}')
                time.sleep(0.2)

    def _handle_command(self, cmd: str):
        c = cmd.upper().strip()
        # Accept forms like GR#, :GR#, #:GR##
        c = c.strip('#')
        if not c.startswith(':'):
            c = ':' + c
        c = c + '#' if not c.endswith('#') else c

        # Set RA and DEC from Stellarium:
        if c.startswith(':SR') and len(c) > 3 and c.endswith('#'):
            ra_text = c[3:-1]
            ra = _parse_ra(ra_text)
            if ra is not None:
                self.pending_ra = ra * 15.0
                self.get_logger().info(f'Pending target RA set {ra_text} ({self.pending_ra:.6f}°)')
                self._write_response('1#')
            else:
                self.get_logger().warn(f"Unparseable RA set '{ra_text}'")
                self._write_response('0#')
            return

        elif c.startswith(':SD') and len(c) > 3 and c.endswith('#'):
            dec_text = c[3:-1]
            dec = _parse_dec(dec_text)
            if dec is not None:
                self.pending_dec = dec
                self.get_logger().info(f'Pending target DEC set {dec_text} ({dec:.6f}°)')
                self._write_response('1#')
            else:
                self.get_logger().warn(f"Unparseable DEC set '{dec_text}'")
                self._write_response('0#')
            return

        elif c in (':MS#', ':MG#'):
            if self.pending_ra is not None and self.pending_dec is not None:
                self._publish_target(self.pending_ra, self.pending_dec)
                self._write_response('1#')
            else:
                self.get_logger().warn('Goto command received but target RA/DEC not set')
                self._write_response('0#')
            return

        elif c == ':GR#':
            ra_hours = (self.current_ra % 360.0) / 15.0
            response = _ra_hours_to_lx200(ra_hours)
            self.get_logger().info(f'Responding to GR with RA current={self.current_ra:.6f}° ({ra_hours:.6f}h) -> {response}')
            self._write_response(response)
            return

        if c == ':GD#':
            response = _dec_deg_to_lx200(self.current_dec)
            self.get_logger().info(f'Responding to GD with DEC current={self.current_dec:.6f}° -> {response}')
            self._write_response(response)
            return

        if c in (':GQ#', ':Q#'):
            self._write_response('0#')
            return

        if c == ':ME#':
            self._write_response('1#')
            return

        # Respond to unknown to keep Stellarium happy
        self.get_logger().debug(f'Unknown serial command: {cmd}')
        self._write_response('0#')

    def _write_response(self, data: str):
        if not data.endswith('#'):
            data = data + '#'
        self.get_logger().debug(f'Serial response: {data}')
        with self._serial_lock:
            try:
                self.raw_tx_pub.publish(String(data=data))
                if isinstance(self.serial_handle, serial.Serial):
                    self.serial_handle.write(data.encode('ascii'))
                else:
                    os.write(self.serial_fd, data.encode('ascii'))
            except Exception as e:
                self.get_logger().warn(f'Error writing serial response: {e}')

    def _write_response_bytes(self, data: bytes):
        with self._serial_lock:
            try:
                self.raw_tx_pub.publish(String(data=data.hex() if isinstance(data, (bytes, bytearray)) else str(data)))
                if self.serial_handle is None:
                    return
                if isinstance(self.serial_handle, serial.Serial):
                    self.serial_handle.write(data)
                else:
                    os.write(self.serial_fd, data)
            except Exception as e:
                self.get_logger().warn(f'Error writing serial bytes: {e}')

    def destroy_node(self):
        self._is_running = False
        if self.serial_handle is not None:
            try:
                if isinstance(self.serial_handle, serial.Serial):
                    self.serial_handle.close()
                else:
                    self.serial_handle.close()
            except Exception:
                pass
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = StellariumSerialBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
