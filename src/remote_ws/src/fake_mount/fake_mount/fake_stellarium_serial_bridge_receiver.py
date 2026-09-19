#!/usr/bin/env python3
"""Fake Stellarium serial bridge receiver for the fake mount package."""

import os
import re
import select
import struct
import threading
import time
import math

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Vector3
from sensor_msgs.msg import JointState
from std_msgs.msg import String


def _precess_equatorial_deg(ra_deg: float, dec_deg: float, t_centuries: float, inverse: bool = False):
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
        x, y, zc = rot_z(x, y, zc, zeta)
        x, y, zc = rot_y(x, y, zc, -theta)
        x, y, zc = rot_z(x, y, zc, z)
    else:
        x, y, zc = rot_z(x, y, zc, -z)
        x, y, zc = rot_y(x, y, zc, theta)
        x, y, zc = rot_z(x, y, zc, -zeta)

    out_ra = math.degrees(math.atan2(y, x)) % 360.0
    out_dec = math.degrees(math.asin(max(-1.0, min(1.0, zc))))
    return out_ra, out_dec


def _julian_centuries_since_j2000_now() -> float:
    jd = (time.time() / 86400.0) + 2440587.5
    return (jd - 2451545.0) / 36525.0


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
    m = re.match(r"^([+-])(\d{1,2})[\*:,°](\d{1,2})[:](\d{1,2})$", dec_str)
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


def _parse_stellarium_packet(packet: bytes):
    if len(packet) < 20:
        return None
    ra_proto = int.from_bytes(packet[12:16], byteorder='little', signed=False)
    dec_proto = int.from_bytes(packet[16:20], byteorder='little', signed=True)
    ra_hours = float(ra_proto) * 12.0 / 2147483648.0
    dec_deg = float(dec_proto) * 90.0 / 1073741824.0
    return ra_hours, dec_deg


def _build_stellarium_packet(ra_hours, dec_deg):
    msize = 0x1800
    mtype = 0x0000
    mtime = int(time.time())
    ra_proto = int(ra_hours * 2147483648.0 / 12.0) & 0xFFFFFFFF
    dec_proto = int(dec_deg * 1073741824.0 / 90.0)
    return struct.pack('<HHQIi', msize, mtype, mtime, ra_proto, dec_proto)


class FakeStellariumSerialBridgeReceiverNode(Node):
    def __init__(self):
        super().__init__('fake_stellarium_serial_bridge_receiver')
        self.declare_parameter('serial_port', '/tmp/fake_stellarium_pty')
        self.declare_parameter('baud_rate', 9600)
        self.declare_parameter('use_pseudo_tty', True)
        self.declare_parameter('stellarium.coordinate_system', 'J2000')
        self.declare_parameter('state_topic', '/fake_mount/fake_RaDec')
        self.declare_parameter('joint_state_topic', '')

        self.serial_port = self.get_parameter('serial_port').value
        self.baud_rate = self.get_parameter('baud_rate').value
        self.use_pseudo_tty = self.get_parameter('use_pseudo_tty').value
        self.coord_system = self.get_parameter('stellarium.coordinate_system').value
        self.state_topic = self.get_parameter('state_topic').value
        self.joint_state_topic = self.get_parameter('joint_state_topic').value
        self.coords_are_j2000 = str(self.coord_system).strip().lower() == 'j2000'

        self.current_ra_deg = 0.0
        self.current_dec_deg = 90.0
        self.pending_ra = None
        self.pending_dec = None

        self._serial_lock = threading.Lock()
        self._serial_handle = None
        self._serial_fd = None

        self._setup_serial()

        self.serial_rx_pub = self.create_publisher(String, '/stellarium/serial_rx', 50)
        self.serial_tx_pub = self.create_publisher(String, '/stellarium/serial_tx', 50)

        self.target_pub = self.create_publisher(Vector3, '/stellarium/target', 10)
        self.create_subscription(Vector3, self.state_topic, self._telescope_state_cb, 10)
        if str(self.joint_state_topic).strip():
            self.create_subscription(JointState, self.joint_state_topic, self._joint_states_cb, 10)

        self._is_running = True
        self._reader_thread = threading.Thread(target=self._read_loop, daemon=True)
        self._reader_thread.start()

        self.get_logger().info(f'Fake Stellarium receiver started on {self.serial_port}')
        self.get_logger().info(f'Feedback topics: state={self.state_topic}, joints={self.joint_state_topic}')
        coord_msg = 'J2000 <-> JNow precession enabled' if self.coords_are_j2000 else 'JNow passthrough'
        self.get_logger().info(f'Coordinate handling: {coord_msg}')

    def _stellarium_to_mount_deg(self, ra_deg: float, dec_deg: float):
        if not self.coords_are_j2000:
            return ra_deg % 360.0, dec_deg
        t = _julian_centuries_since_j2000_now()
        return _precess_equatorial_deg(ra_deg, dec_deg, t, inverse=False)

    def _mount_to_stellarium_deg(self, ra_deg: float, dec_deg: float):
        if not self.coords_are_j2000:
            return ra_deg % 360.0, dec_deg
        t = _julian_centuries_since_j2000_now()
        return _precess_equatorial_deg(ra_deg, dec_deg, t, inverse=True)

    def _setup_serial(self):
        if self.use_pseudo_tty:
            master_fd, slave_fd = os.openpty()
            slave_name = os.ttyname(slave_fd)
            os.chmod(slave_name, 0o666)
            static_path = self.serial_port
            try:
                if os.path.islink(static_path) or os.path.exists(static_path):
                    os.unlink(static_path)
                os.symlink(slave_name, static_path)
                self.get_logger().info(f'Created static symlink {static_path} -> {slave_name}')
            except Exception as e:
                self.get_logger().warn(f'Failed to create static symlink {static_path}: {e}')
            os.close(slave_fd)
            self._serial_handle = os.fdopen(master_fd, 'r+b', buffering=0)
            self._serial_fd = master_fd
            self.get_logger().info(f'Pseudo-TTY endpoint (slave): {slave_name} (use {static_path} in Stellarium)')
            return
        try:
            import serial
            self._serial_handle = serial.Serial(self.serial_port, self.baud_rate, timeout=0.1)
            self._serial_fd = self._serial_handle.fileno()
            self.get_logger().info(f'Opened real serial port {self.serial_port}')
        except Exception as e:
            self.get_logger().error(f'Failed to open serial port {self.serial_port}: {e}')
            self._serial_handle = None

    def _telescope_state_cb(self, msg: Vector3):
        self.current_ra_deg = float(msg.x) % 360.0
        self.current_dec_deg = float(msg.y)

    def _joint_states_cb(self, msg: JointState):
        try:
            if 'fake_ra_joint' in msg.name and 'fake_dec_joint' in msg.name:
                ra_joint = msg.position[msg.name.index('fake_ra_joint')]
                dec_joint = msg.position[msg.name.index('fake_dec_joint')]
                self.current_ra_deg = (math.degrees(ra_joint) % 360.0)
                self.current_dec_deg = math.degrees(dec_joint)
            elif 'ra_joint' in msg.name and 'dec_joint' in msg.name:
                ra_joint = msg.position[msg.name.index('ra_joint')]
                dec_joint = msg.position[msg.name.index('dec_joint')]
                self.current_ra_deg = (math.degrees(ra_joint) % 360.0)
                self.current_dec_deg = math.degrees(dec_joint)
        except Exception:
            pass

    def _publish_target(self, ra, dec):
        tgt = Vector3(x=float(ra), y=float(dec), z=0.0)
        self.target_pub.publish(tgt)
        self.get_logger().info(f'Published /stellarium/target RA={ra:.6f} DEC={dec:.6f}')

    def _write(self, data: bytes):
        with self._serial_lock:
            try:
                try:
                    self.serial_tx_pub.publish(String(data=data.decode('ascii', errors='replace')))
                except Exception:
                    pass
                if hasattr(self._serial_handle, 'write'):
                    self._serial_handle.write(data)
                elif self._serial_fd:
                    os.write(self._serial_fd, data)
            except Exception as e:
                self.get_logger().warn(f'Serial write error: {e}')

    def _write_response(self, text: str):
        if not text.endswith('#'):
            text += '#'
        self._write(text.encode('ascii'))

    def _handle_ascii_command(self, cmd: str):
        c = cmd.upper().strip()
        if not c.startswith(':'):
            c = ':' + c
        if not c.endswith('#'):
            c += '#'

        self.get_logger().info(f'LX200 RX: {c}')

        if c.startswith(':SR') and c.endswith('#'):
            ra = _parse_ra(c[3:-1])
            if ra is not None:
                self.pending_ra = ra
                self._write_response('1#')
                return
        if c.startswith(':SD') and c.endswith('#'):
            dec_text = c[3:-1]
            dec_text = dec_text.replace('°', '*').replace('�', '*')
            dec = _parse_dec(dec_text)
            if dec is not None:
                self.pending_dec = dec
                self._write_response('1#')
                return
        if c in (':MS#', ':MG#'):
            if self.pending_ra is not None and self.pending_dec is not None:
                in_ra_deg = (self.pending_ra * 15.0) % 360.0
                mount_ra_deg, mount_dec_deg = self._stellarium_to_mount_deg(in_ra_deg, self.pending_dec)
                self._publish_target(mount_ra_deg, mount_dec_deg)
                self.pending_ra = None
                self.pending_dec = None
                self._write_response('1#')
            else:
                self._write_response('0#')
            return
        if c == ':GR#':
            out_ra_deg, _ = self._mount_to_stellarium_deg(self.current_ra_deg, self.current_dec_deg)
            ra_hours = (out_ra_deg % 360.0) / 15.0
            self._write_response(_ra_hours_to_lx200(ra_hours))
            return
        if c == ':GD#':
            _, out_dec_deg = self._mount_to_stellarium_deg(self.current_ra_deg, self.current_dec_deg)
            self._write_response(_dec_deg_to_lx200(out_dec_deg))
            return
        if c in (':GQ#', ':Q#'):
            self._write_response('0#')
            return
        if c == ':ME#':
            self._write_response('1#')
            return

        self._write_response('0#')

    def _read_loop(self):
        buffer = bytearray()
        while self._is_running:
            try:
                rlist, _, _ = select.select([self._serial_fd], [], [], 0.1)
                if not rlist:
                    continue
                data = os.read(self._serial_fd, 256)
                if not data:
                    continue

                self.get_logger().debug(f'Received raw: {data!r}')
                try:
                    self.serial_rx_pub.publish(String(data=data.decode('ascii', errors='replace')))
                except Exception:
                    pass
                buffer.extend(data)

                while b'#' in buffer:
                    idx = buffer.index(b'#')
                    packet = buffer[: idx + 1]
                    del buffer[: idx + 1]
                    cmd = packet.decode('ascii', errors='replace').strip().strip('#')
                    if not cmd:
                        continue
                    self.get_logger().info(f'Parsed command from peer: {cmd}')
                    self._handle_ascii_command(cmd)

                while len(buffer) >= 20 and not buffer.startswith(b':'):
                    packet = bytes(buffer[:20])
                    del buffer[:20]
                    parsed = _parse_stellarium_packet(packet)
                    if parsed is not None:
                        ra_hours, dec_deg = parsed
                        in_ra_deg = (ra_hours * 15.0) % 360.0
                        mount_ra_deg, mount_dec_deg = self._stellarium_to_mount_deg(in_ra_deg, dec_deg)
                        self._publish_target(mount_ra_deg, mount_dec_deg)
                        out_ra_deg, out_dec_deg = self._mount_to_stellarium_deg(self.current_ra_deg, self.current_dec_deg)
                        cur_ra_hours = (out_ra_deg % 360.0) / 15.0
                        self._write(_build_stellarium_packet(cur_ra_hours, out_dec_deg))

            except OSError as e:
                if getattr(e, 'errno', None) in (5, 6, 11):
                    continue
                self.get_logger().warn(f'Serial read error: {e}')
                time.sleep(0.1)
            except Exception as e:
                self.get_logger().warn(f'Readloop exception: {e}')
                time.sleep(0.1)

    def destroy_node(self):
        self._is_running = False
        if self._serial_handle is not None:
            try:
                self._serial_handle.close()
            except Exception:
                pass
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = FakeStellariumSerialBridgeReceiverNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
