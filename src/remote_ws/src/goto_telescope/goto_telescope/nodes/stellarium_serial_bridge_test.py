#!/usr/bin/env python3
"""Standalone Stellarium LX200-style serial bridge test script.

This script is a ROS2-free copy of the Stellarium serial bridge logic.
It supports:
 - creating a pseudo-TTY pair (master/slave)
 - accepting LX200 ASCII commands from user CLI (manual mode)
 - accepting commands from a Stellarium client connected to the pseudo-TTY slave
 - sending/receiving responses on the console

Usage:
  python3 stellarium_serial_bridge_test.py [--serial-port /dev/ttyS0] [--baud 9600]

If --serial-port is not available or omitted, it creates a pseudo-TTY and prints
slave path (point Stellarium to that path).

Manual commands you can type (works as if Stellarium sent them):
  GR, GD, SrHH:MM:SS, Sd+DD*MM:SS, Sd-DD*MM:SS, MS, Q, GQ, ME

"""

import argparse
import math
import os
import re
import select
import struct
import threading
import time

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


class StellariumSerialBridgeTest:
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
        self.serial_connected = False
        self._waiting_for_peer = False
        self._is_running = False

        self._setup_serial()

        if self.serial_handle is None:
            raise RuntimeError('Could not open serial endpoint.')

    def _setup_serial(self):
        if self.serial_port and os.path.exists(self.serial_port):
            if serial is None:
                print('pyserial is not installed; cannot open real serial.');
                self.serial_handle = None
                return
            try:
                ser = serial.Serial(self.serial_port, self.baud_rate, timeout=0.1)
                self.serial_handle = ser
                self.serial_fd = ser.fileno()
                print(f'Opened real serial port {self.serial_port} @ {self.baud_rate}')
                return
            except SerialException as e:
                print(f'Failed to open serial port {self.serial_port}: {e}')

        if self.use_pseudo_tty:
            print('Creating pseudo-TTY pair for Stellarium.')
            master_fd, slave_fd = os.openpty()
            slave_name = os.ttyname(slave_fd)
            os.chmod(slave_name, 0o666)
            os.close(slave_fd)
            self.serial_port = slave_name
            self.serial_handle = os.fdopen(master_fd, 'r+b', buffering=0)
            self.serial_fd = master_fd
            print(f'Pseudo-TTY endpoint created. Connect Stellarium to: {slave_name}')
            return

        print('Failed to create serial/pseudo-tty endpoint.')
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

    def _write_response(self, data: str):
        if not data.endswith('#'):
            data += '#'
        print(f'-> RESP: {data}')
        with self._serial_lock:
            try:
                if isinstance(self.serial_handle, (bytes, bytearray)):
                    pass
                if hasattr(self.serial_handle, 'write'):
                    self.serial_handle.write(data.encode('ascii'))
                elif self.serial_fd is not None:
                    os.write(self.serial_fd, data.encode('ascii'))
            except Exception as e:
                print(f'Error writing serial response: {e}')

    def _write_response_bytes(self, data: bytes):
        with self._serial_lock:
            try:
                print(f'-> RESP bytes: {data.hex()}')
                if hasattr(self.serial_handle, 'write'):
                    self.serial_handle.write(data)
                elif self.serial_fd is not None:
                    os.write(self.serial_fd, data)
            except Exception as e:
                print(f'Error writing serial bytes: {e}')

    def _publish_target(self, ra_hours, dec_deg):
        self.current_ra = float(ra_hours)
        self.current_dec = float(dec_deg)
        print(f'Goto target published: RA={self.current_ra:.6f}h DEC={self.current_dec:.6f}°')

    def _handle_command(self, input_cmd: str):
        cmd = input_cmd.strip().upper()
        if not cmd.startswith(':'):
            cmd = ':' + cmd
        if not cmd.endswith('#'):
            cmd = cmd + '#'

        if cmd.startswith(':SR') and len(cmd) > 3 and cmd.endswith('#'):
            ra_text = cmd[3:-1]
            ra = _parse_ra(ra_text)
            if ra is not None:
                self.pending_ra = ra
                print(f'Set pending RA = {ra_text} ({ra:.6f}h)')
                self._write_response('1#')
            else:
                print(f'Invalid RA format: {ra_text}')
                self._write_response('0#')
            return

        if cmd.startswith(':SD') and len(cmd) > 3 and cmd.endswith('#'):
            dec_text = cmd[3:-1]
            dec = _parse_dec(dec_text)
            if dec is not None:
                self.pending_dec = dec
                print(f'Set pending DEC = {dec_text} ({dec:.6f}°)')
                self._write_response('1#')
            else:
                print(f'Invalid DEC format: {dec_text}')
                self._write_response('0#')
            return

        if cmd in (':MS#', ':MG#'):
            if self.pending_ra is not None and self.pending_dec is not None:
                self._publish_target(self.pending_ra, self.pending_dec)
                self.pending_ra = None
                self.pending_dec = None
                self._write_response('1#')
            else:
                print('Goto command received but RA/DEC pending not set.')
                self._write_response('0#')
            return

        if cmd == ':GR#':
            response = _ra_hours_to_lx200(self.current_ra)
            print(f'Query GR -> {response}')
            self._write_response(response)
            return

        if cmd == ':GD#':
            response = _dec_deg_to_lx200(self.current_dec)
            print(f'Query GD -> {response}')
            self._write_response(response)
            return

        if cmd in (':GQ#', ':Q#'):
            self._write_response('0#')
            return

        if cmd == ':ME#':
            self._write_response('1#')
            return

        print(f'Unknown command: {input_cmd}')
        self._write_response('0#')

    def _read_loop(self):
        buffer = bytearray()
        while self._is_running:
            try:
                if self.serial_handle is None:
                    time.sleep(0.1)
                    continue

                rlist, _, _ = select.select([self.serial_fd], [], [], 0.1)
                if not rlist:
                    continue

                if serial is not None and isinstance(self.serial_handle, serial.Serial):
                    data = self.serial_handle.read(256)
                else:
                    data = os.read(self.serial_fd, 256)

                if not data:
                    continue

                text = data.decode('ascii', errors='replace')
                print(f'<- RX on serial: {text}')
                buffer.extend(data)

                while b'#' in buffer:
                    idx = buffer.index(b'#')
                    packet = buffer[: idx + 1]
                    del buffer[: idx + 1]
                    cmd = packet.decode('ascii', errors='ignore').strip().strip('#')
                    if not cmd:
                        continue
                    if not cmd.startswith(':'):
                        cmd = ':' + cmd
                    cmd = cmd + '#'
                    print(f'Parsed command from peer: {cmd}')
                    self._handle_command(cmd)

                while len(buffer) >= 20 and not buffer.startswith(b':'):
                    packet = bytes(buffer[:20])
                    del buffer[:20]
                    parsed = self._parse_stellarium_packet(packet)
                    if parsed is not None:
                        ra_hours, dec_deg = parsed
                        print(f'Received Stellarium binary target RA={ra_hours:.6f} DEC={dec_deg:.6f}')
                        self._publish_target(ra_hours, dec_deg)
                        resp = self._build_stellarium_packet(self.current_ra, self.current_dec)
                        self._write_response_bytes(resp)

            except OSError as e:
                if getattr(e, 'errno', None) in (5, 6):
                    if not self.serial_connected and not self._waiting_for_peer:
                        print('Waiting for Stellarium or serial client to open pseudo-TTY endpoint...')
                        self._waiting_for_peer = True
                    # suppress repeated messages while not connected
                else:
                    print('Serial read error:', e)
                time.sleep(0.1)
            except Exception as e:
                print('Readloop exception:', e)
                time.sleep(0.1)

    def start(self):
        self._is_running = True
        self._reader_thread = threading.Thread(target=self._read_loop, daemon=True)
        self._reader_thread.start()

    def stop(self):
        self._is_running = False
        if self.serial_handle is not None:
            try:
                self.serial_handle.close()
            except Exception:
                pass

    def _send_raw_to_peer(self, data: str):
        if not data.endswith('#'):
            data = data + '#'
        print(f'OUT -> {data}')
        with self._serial_lock:
            try:
                if hasattr(self.serial_handle, 'write'):
                    self.serial_handle.write(data.encode('ascii'))
                elif self.serial_fd is not None:
                    os.write(self.serial_fd, data.encode('ascii'))
            except Exception as e:
                print(f'Error writing raw to peer: {e}')

    def manual_console(self):
        print('Manual test console ready.')
        print('  in <cmd>   -> process as incoming Stellarium command (same as remote client).')
        print('  out <cmd>  -> send raw data to Stellarium endpoint (peer output).')
        print('  state      -> print current RA/DEC/pending state.')
        print('  help       -> this message.')
        print('  exit/quit  -> stop.')
        while True:
            try:
                raw = input('> ').strip()
            except EOFError:
                break
            if not raw:
                continue
            thing = raw.lower().split()[0]
            if thing in ('exit', 'quit'):
                break
            if thing == 'help':
                print('Commands: in <cmd>, out <cmd>, state, help, exit')
                continue
            if thing == 'state':
                print(f'Current RA {self.current_ra:.6f}h DEC {self.current_dec:.6f}°, pending RA {self.pending_ra}, pending DEC {self.pending_dec}')
                continue
            if raw.lower().startswith('in '):
                cmd = raw[3:].strip()
                print(f'INJECT <- {cmd}')
                self._handle_command(cmd)
                continue
            if raw.lower().startswith('out '):
                cmd = raw[4:].strip()
                self._send_raw_to_peer(cmd)
                continue
            # default treats as inbound Stellarium command
            self._handle_command(raw)


class StellariumSerialClient:
    def __init__(self, serial_port, baud_rate=9600):
        self.serial_port = serial_port
        self.baud_rate = baud_rate
        self._is_running = False
        self.serial_handle = None
        self.serial_fd = None

        if serial is None:
            raise RuntimeError('pyserial is required for client mode; please install pyserial.')

        if not os.path.exists(self.serial_port):
            raise FileNotFoundError(f'Serial port {self.serial_port} does not exist.')

        self.serial_handle = serial.Serial(self.serial_port, self.baud_rate, timeout=0.1)
        self.serial_fd = self.serial_handle.fileno()
        print(f'Client connected to {self.serial_port} @ {self.baud_rate}')

    def _read_loop(self):
        while self._is_running:
            try:
                data = self.serial_handle.read(256)
                if data:
                    text = data.decode('ascii', errors='replace')
                    print(f'<- [from bridge] {text}')
                else:
                    time.sleep(0.05)
            except Exception as e:
                print('Client read error:', e)
                time.sleep(0.2)

    def start(self):
        self._is_running = True
        self._reader_thread = threading.Thread(target=self._read_loop, daemon=True)
        self._reader_thread.start()

    def stop(self):
        self._is_running = False
        if self.serial_handle is not None:
            try:
                self.serial_handle.close()
            except Exception:
                pass

    def send(self, cmd: str):
        if not cmd.endswith('#'):
            cmd = cmd + '#'
        payload = cmd.encode('ascii')
        self.serial_handle.write(payload)
        print(f'-> [to bridge] {cmd}')

    def manual_console(self):
        print('Client manual console. Type LX200 commands to send to bridge.')
        print('Type exit/quit to stop.')
        while True:
            try:
                raw = input('client> ').strip()
            except EOFError:
                break
            if not raw:
                continue
            if raw.lower() in ('exit', 'quit'):
                break
            self.send(raw)


def main():
    parser = argparse.ArgumentParser(description='Stellarium serial bridge stand-alone test harness.')
    parser.add_argument('--mode', choices=['bridge', 'client'], default='bridge',
                        help='bridge: create pseudo-TTY and service commands; client: connect to existing slave and send commands')
    parser.add_argument('--serial-port', type=str, default=None,
                        help='Serial port (for client mode) or optional bridge target to use existing port')
    parser.add_argument('--baud', type=int, default=9600, help='Baud rate (default 9600).')
    parser.add_argument('--no-pty', dest='use_pty', action='store_false', help='Do not create pseudo-tty if no port exists.')
    args = parser.parse_args()

    if args.mode == 'bridge':
        bridge = StellariumSerialBridgeTest(serial_port=args.serial_port, baud_rate=args.baud, use_pseudo_tty=args.use_pty)
        bridge.start()
        try:
            bridge.manual_console()
        finally:
            bridge.stop()
            print('Bridge stopped.')
    else:
        if args.serial_port is None:
            raise ValueError('Client mode requires --serial-port <slave_path>')
        client = StellariumSerialClient(serial_port=args.serial_port, baud_rate=args.baud)
        client.start()
        try:
            client.manual_console()
        finally:
            client.stop()
            print('Client stopped.')


if __name__ == '__main__':
    main()
