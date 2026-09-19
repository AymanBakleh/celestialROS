#!/usr/bin/env python3
"""Server script: pseudo-TTY serial bridge for Stellarium (read/process/write).

Run this in one terminal. It creates a pseudo-TTY and logs all incoming
LX200 commands. It responds with standard LX200 replies and supports
Stellarium binary frame handling too.

Usage:
  python3 stellarium_serial_bridge_server.py [--serial-port /dev/ttyS0] [--baud 9600]
"""

import argparse
import os
import re
import select
import struct
import threading
import time


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


class StellariumSerialBridgeServer:
    def __init__(self, serial_port=None, baud_rate=9600, use_pseudo_tty=True):
        self.serial_port = serial_port
        self.baud_rate = baud_rate
        self.use_pseudo_tty = use_pseudo_tty

        self.current_ra = 0.0
        self.current_dec = 0.0
        self.pending_ra = None
        self.pending_dec = None

        self._serial_lock = threading.Lock()
        self.serial_handle = None
        self.serial_fd = None
        self._is_running = False

        self._setup_serial()
        if self.serial_handle is None:
            raise RuntimeError('Could not open serial endpoint.')

    def _setup_serial(self):
        if self.serial_port and os.path.exists(self.serial_port):
            try:
                import serial
                self.serial_handle = serial.Serial(self.serial_port, self.baud_rate, timeout=0.1)
                self.serial_fd = self.serial_handle.fileno()
                print(f'Opened real serial port {self.serial_port} @ {self.baud_rate}')
                return
            except Exception as e:
                print(f'Failed to open serial port {self.serial_port}: {e}')

        if self.use_pseudo_tty:
            master_fd, slave_fd = os.openpty()
            slave_name = os.ttyname(slave_fd)
            os.chmod(slave_name, 0o666)
            os.close(slave_fd)
            self.serial_handle = os.fdopen(master_fd, 'r+b', buffering=0)
            self.serial_fd = master_fd
            self.serial_port = slave_name
            print(f'Pseudo-TTY endpoint created: {self.serial_port}')
            print('Open Stellarium client on this slave path')
            return

        self.serial_handle = None

    def _build_stellarium_packet(self, ra_hours, dec_deg):
        msize = 0x1800
        mtype = 0x0000
        mtime = int(time.time())
        ra_proto = int(ra_hours * 2147483648.0 / 12.0) & 0xFFFFFFFF
        dec_proto = int(dec_deg * 1073741824.0 / 90.0)
        return struct.pack('<HHQIi', msize, mtype, mtime, ra_proto, dec_proto)

    def _parse_stellarium_packet(self, packet: bytes):
        if len(packet) < 20:
            return None
        ra_proto = int.from_bytes(packet[12:16], byteorder='little', signed=False)
        dec_proto = int.from_bytes(packet[16:20], byteorder='little', signed=True)
        ra_hours = float(ra_proto) * 12.0 / 2147483648.0
        dec_deg = float(dec_proto) * 90.0 / 1073741824.0
        return ra_hours, dec_deg

    def _write(self, data: bytes):
        with self._serial_lock:
            try:
                if hasattr(self.serial_handle, 'write'):
                    self.serial_handle.write(data)
                elif self.serial_fd is not None:
                    os.write(self.serial_fd, data)
            except Exception as e:
                print('Write error:', e)

    def _write_response(self, text: str):
        if not text.endswith('#'):
            text += '#'
        print('->', text)
        self._write(text.encode('ascii'))

    def _publish_target(self, ra_hours, dec_deg):
        self.current_ra = ra_hours
        self.current_dec = dec_deg
        print(f'Goto target RA={ra_hours:.6f}h DEC={dec_deg:.6f}°')

    def _handle_command(self, cmd: str):
        c = cmd.upper().strip('#')
        if not c.startswith(':'):
            c = ':' + c
        c += '#'

        if c.startswith(':SR') and c.endswith('#'):
            ra = _parse_ra(c[3:-1])
            if ra is not None:
                self.pending_ra = ra
                self._write_response('1#')
            else:
                self._write_response('0#')
            return

        if c.startswith(':SD') and c.endswith('#'):
            dec = _parse_dec(c[3:-1])
            if dec is not None:
                self.pending_dec = dec
                self._write_response('1#')
            else:
                self._write_response('0#')
            return

        if c in (':MS#', ':MG#'):
            if self.pending_ra is not None and self.pending_dec is not None:
                self._publish_target(self.pending_ra, self.pending_dec)
                self.pending_ra = None
                self.pending_dec = None
                self._write_response('1#')
            else:
                self._write_response('0#')
            return

        if c == ':GR#':
            self._write_response(_ra_hours_to_lx200(self.current_ra))
            return

        if c == ':GD#':
            self._write_response(_dec_deg_to_lx200(self.current_dec))
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
                rlist, _, _ = select.select([self.serial_fd], [], [], 0.1)
                if not rlist:
                    continue
                data = os.read(self.serial_fd, 256)
                if not data:
                    continue
                text = data.decode('ascii', errors='replace')
                print('<-', text)
                buffer.extend(data)

                while b'#' in buffer:
                    idx = buffer.index(b'#')
                    packet = buffer[:idx + 1]
                    del buffer[:idx + 1]
                    cmd = packet.decode('ascii', errors='ignore').strip().strip('#')
                    if not cmd:
                        continue
                    cmd = ':' + cmd if not cmd.startswith(':') else ':' + cmd.lstrip(':')
                    cmd += '#'
                    print('Parsed command from peer:', cmd)
                    self._handle_command(cmd)

                while len(buffer) >= 20 and not buffer.startswith(b':'):
                    packet = bytes(buffer[:20])
                    del buffer[:20]
                    parsed = self._parse_stellarium_packet(packet)
                    if parsed is not None:
                        ra_hours, dec_deg = parsed
                        self._publish_target(ra_hours, dec_deg)
                        self._write(self._build_stellarium_packet(self.current_ra, self.current_dec))

            except OSError as e:
                if getattr(e, 'errno', None) in (5, 6):
                    continue
                print('Read error:', e)
                time.sleep(0.1)
            except Exception as e:
                print('Readloop exception:', e)
                time.sleep(0.1)

    def start(self):
        self._is_running = True
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    def stop(self):
        self._is_running = False
        if self.serial_handle is not None:
            try:
                self.serial_handle.close()
            except Exception:
                pass


def main():
    parser = argparse.ArgumentParser(description='Stellarium serial bridge server.')
    parser.add_argument('--serial-port', type=str, default=None, help='Optional actual serial port.')
    parser.add_argument('--baud', type=int, default=9600, help='Baud rate')
    parser.add_argument('--no-pty', dest='use_pty', action='store_false', help='Do not create pseudo-tty if no port exists.')
    args = parser.parse_args()

    server = StellariumSerialBridgeServer(serial_port=args.serial_port, baud_rate=args.baud, use_pseudo_tty=args.use_pty)
    server.start()
    try:
        print('Bridge server running. Ctrl+C to exit.')
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        server.stop()
        print('Bridge server stopped.')


if __name__ == '__main__':
    main()
