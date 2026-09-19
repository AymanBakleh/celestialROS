# Telescope Mount Firmware (Jarspace)

ESP32 firmware for a 2-axis telescope mount with **two serial bus servos** (RA and DEC).

This repo contains:

- **V1** (`telescope_mount/`): OLED + WiFi web UI + richer command set.
- **V2** (`telescope_mount_v2/`): **Serial-only JSON** with the minimal `T1`/`T2` protocol (no web UI, no OLED).

---

## Hardware (V2 default)

- **Board:** ESP32-WROOM (or compatible).
- **Servos:** 2× Feetech serial bus servos (SMS/STS). **RA = ID 13, DEC = ID 14**.
- **Wiring:** Servo bus on `Serial1` (**TX=19, RX=18**), 1 Mbaud. USB `Serial` for host at 115200.

---

## Code layout

| File | Role |
|------|------|
| **config.h** | Pins, IDs, baud rate, deadband, WiFi AP name/password, timing. |
| **servo_control.h / .cpp** | Low-level servo bus: read position, write position, torque on/off, DEFA (torque/current limits), scan. |
| **mount_control.h / .cpp** | Mount state (zeros, targets, velocity), command handling, feedback/status JSON, position/velocity logic. |
| **oled_display.h / .cpp** | OLED init and update (4 lines). |
| **web_page.h** | Single HTML/CSS/JS page for the web UI (status + controls). |
| **web_server.h / .cpp** | WiFi AP (Jarspace / 12345678), HTTP server, `/` = page, `/api/status`, `/api/cmd`. |
| **telescope_mount.ino** | V1: setup/loop: init order, Serial + web command parsing, feedback, motors, OLED. |
| **telescope_mount_v2.ino** | V2: setup/loop: Serial JSON `T1`/`T2`, wheel-mode control, RA/DEC counters. |

---

## Startup behaviour

1. **Torque is OFF** at power-on (motors are not driven until you enable torque).
2. **Encoders are zeroed** at startup: current physical position is taken as (0, 0)° and step zero.
3. WiFi AP **Jarspace** (password **12345678**) is started; connect and open `http://192.168.4.1` for the web UI.

---

## Commands (V2 / Serial only)

All commands are **simple strings**, sent over **USB Serial**, newline-terminated.

### `T1` (relative move command)

`T1` is the desired relative move in **degrees** for RA/DEC. The mount converts degrees into ticks using:

- \(4096\) encoder ticks \(=\) \(2.5^\circ\) axis motion

Format:

- `T1 <ra_deg> <dec_deg> [spd]`

Examples:

- `T1 10 0 1500`
- `T1 -2.5 1.25`

Response (ack):

- `OK T1 <ra_deg> <dec_deg> <spd>`

### `T2` (read counters)

Request:

- `T2`

Response:

- `T2 ra_ticks dec_ticks ra_deg dec_deg ra_target_ticks dec_target_ticks ra_target_deg dec_target_deg`

### 1. Position (angle in degrees + separate RA/DEC speeds)

Set target position in **degrees**.  You may also supply a speed for each axis
(in the library's raw speed units).  Use the `ra_spd`/`dec_spd` fields.

- **Serial:** {"T":1,"ra_deg":<deg>,"dec_deg":<deg>,"ra_spd":<opt>,"dec_spd":<opt>,"ra_acc":<opt>,"dec_acc":<opt>}  
  (older clients may continue to send `"ra_spd"`, which is now the only valid speed field.)
- **Web:** GOTO position: enter RA/DEC in degrees and the desired speeds, then
  “GOTO position”.

Enables torque and leaves velocity mode. Uses deadband and rate-limited commands to reduce oscillation.

### 2. Position by encoder ticks (with separate RA/DEC speeds)

Move to a target given in **encoder steps** (multi-turn aware). You can set the speed in the library's raw units using `ra_spd`/`dec_spd`.

- **Serial:** `{"T":1,"ra_pos":<steps>,"dec_pos":<steps>,"ra_spd":<opt>,"dec_spd":<opt>}`
- **Web:** GOTO encoder ticks: enter RA/DEC ticks and the desired speeds, then
  “GOTO encoder ticks”.

### 3. Reset encoders (zero)

- **Zero now (current position = 0):**  
  `{"T":15,"zero_now":1}`  
  Sets current position as the new zero. Also done automatically at **startup**.
- **Set absolute zero values:**  
  `{"T":15,"zero_absolute":1,"ra_zero":<steps>,"dec_zero":<steps>}`  

- **Web:** “Zero encoders (current position = 0)” sends `T:15, zero_now:1`.

### 4. Constant speed (velocity mode)

Run the mount at a constant **speed** expressed in the library's raw units.
The JSON fields are `ra_spd` and `dec_spd`. For compatibility the old
`ra_deg_s`/`dec_deg_s` inputs are still supported; they are converted to the
native speed units by dividing by 6 and then scaling by `RPM_TO_SPEED`.

- **Serial:** `{"T":20,"ra_spd":<spd>,"dec_spd":<spd>}`
  
  Examples:
  - `{"T":20,"ra_spd":100}` → RA moves at speed unit 100, DEC holds.
  - `{"T":20,"dec_deg_s":5}` → DEC converted to raw speed from deg/s input.
- **Web:** The velocity field now accepts native speed units; keep “Set velocity”.

### Other commands

| T | Meaning |
|---|--------|
| **10** | Update params: `ra_ratio`, `dec_ratio`, `ra_spd`, `dec_spd`, `ra_acc`, `dec_acc`. |
| **11** | Torque: `{"T":11,"torque":1}` or `0`. |
| **12** | DEFA (torque/current limits): `defa`: 0=off, 1=on, 2=status, other=custom value. |
| **105** | Request one feedback packet (position, speed, mode) on Serial. |

---

## Feedback (status)

- **Serial:** Every 50 ms a JSON packet with `"T":2` is sent: **ra_deg**, **dec_deg**, ra_raw, dec_raw, ra_pos, dec_pos, **ra_spd**, **dec_spd**, mode, torque_enabled. If the servo cannot report its encoder position, the firmware will still estimate the axis by integrating the last commanded speed. In velocity mode: **ra_deg_s**, **dec_deg_s**.
- **Web:** GET `/api/status` returns the same status. The page polls about every 1.5 s.

---

## Reducing motor oscillation

- **Deadband:** In **config.h**, `DEADBAND_STEPS` (default **50**) — no move is sent when position error is within this. Increase further if the mount still hunts.
- **Rate limit:** `MOTOR_CMD_INTERVAL_MS` (default 40 ms) — minimum time between sending new position commands to the servos.
- **Speed scaling:** When error is small (< 100 steps), commanded servo speed/accel are scaled down so the mount doesn’t overshoot.

---

## Building and upload

- **PlatformIO:** From `telescope_firmware` (defaults to **V2** via `src_dir`):  
  `pio run -e esp32dev`  
  `pio run -e esp32dev -t upload`
- **Arduino IDE:** Open `telescope_mount_v2/telescope_mount_v2.ino`, select ESP32 board and port. Install: ArduinoJson + Feetech/SMS_STS (SCServo) library.

Pins (e.g. S_RXD, S_TXD, S_SDA, S_SCL) are in **config.h**; change them there if your board differs.

---

## WiFi and web UI

Only V1 has WiFi/web UI. V2 is Serial-only.

---

## Summary

- **Torque off** at power-on; encoders **zeroed** at startup.
- **1** = position in **degrees** (`ra_deg`/`dec_deg`) or encoder ticks
  (`ra_pos`/`dec_pos`), with separate **ra_spd** and **dec_spd`.
- **15** = reset encoders (`T:15`, zero_now or zero_absolute); also reset at startup.
- **4** = constant speed (`T:20` velocity mode).
- Control and display are split into **config**, **servo_control**, **mount_control**, **oled_display**, and **web_server**; **README** documents commands and layout.
