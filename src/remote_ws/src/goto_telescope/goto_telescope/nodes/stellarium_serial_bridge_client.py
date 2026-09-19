#!/usr/bin/env python3
"""Client script: send LX200 commands to Stellarium serial bridge and print output.

Run this in another terminal for manual command control.

Usage:
  python3 stellarium_serial_bridge_client.py --serial-port /dev/pts/X [--baud 9600]
"""

import argparse
import threading
import time

try:
    import serial
except ImportError:
    raise SystemExit('pyserial is required: pip install pyserial')


def main():
    parser = argparse.ArgumentParser(description='Stellarium serial bridge client.')
    parser.add_argument('--serial-port', type=str, required=True, help='Slave device from bridge (/dev/pts/X).')
    parser.add_argument('--baud', type=int, default=9600, help='Baud rate')
    args = parser.parse_args()

    ser = serial.Serial(args.serial_port, args.baud, timeout=0.1)
    print(f'Connected to {args.serial_port} @ {args.baud}')

    def read_loop():
        while True:
            try:
                data = ser.read(256)
                if data:
                    print('<- [bridge] ' + data.decode('ascii', errors='replace'))
                else:
                    time.sleep(0.05)
            except Exception as e:
                print('Read failed:', e)
                break

    state = {
        'last_cmd': None,
        'repeat_interval': 0.0,
    }
    repeat_event = threading.Event()

    def repeat_loop():
        while not repeat_event.is_set():
            interval = state.get('repeat_interval', 0.0)
            last_cmd = state.get('last_cmd')
            if last_cmd and interval > 0:
                try:
                    ser.write(last_cmd.encode('ascii'))
                    print('-> [hold] ' + last_cmd)
                except Exception as e:
                    print('Hold send failed:', e)
                    break
            step = 0.1
            if interval <= 0:
                time.sleep(step)
            else:
                waited = 0.0
                while waited < interval and not repeat_event.is_set():
                    time.sleep(step)
                    waited += step

    repeater_thread = threading.Thread(target=repeat_loop, daemon=True)
    repeater_thread.start()

    reader_thread = threading.Thread(target=read_loop, daemon=True)
    reader_thread.start()

    print('Type LX200 commands (GR, GD, SR..., SD..., MS, GQ, ME).')
    print('Special commands: hold <sec>, hold off, status, exit')

    try:
        while True:
            cmd = input('cmd> ').strip()
            if not cmd:
                continue
            low = cmd.lower()
            if low in ('exit', 'quit'):
                break
            if low.startswith('hold '):
                try:
                    interval = float(low.split()[1])
                    if interval <= 0:
                        raise ValueError
                    state['repeat_interval'] = interval
                    state['last_cmd'] = state['last_cmd'] or ''
                    print(f'Hold mode enabled: repeating last command every {interval} sec')
                except Exception:
                    print('Usage: hold <seconds> (example: hold 0.5)')
                continue
            if low == 'hold off':
                state['repeat_interval'] = 0.0
                print('Hold mode off')
                continue
            if low == 'status':
                print(f"last_cmd={state['last_cmd']} repeat_interval={state['repeat_interval']}")
                continue

            if not cmd.endswith('#'):
                cmd = cmd + '#'

            ser.write(cmd.encode('ascii'))
            print('-> [to bridge]', cmd)
            state['last_cmd'] = cmd
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
    finally:
        repeat_event.set()
        ser.close()
        print('Client stopped.')


if __name__ == '__main__':
    main()
