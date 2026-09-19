# Telescope Follow Mode - Complete Guide

## Overview

The telescope mount now supports **Follow Mode** for continuous RA tracking at a fixed speed, perfect for astrophotography and celestial object tracking.

## Features

- **Goto Mode**: Default mode for slewing to specific coordinates
- **Follow Mode**: Continuous RA tracking at configurable speed
- **OLED Display**: Shows current mode (GOTO/FOLLOW) and speed
- **ROS2 Control**: Full integration with ROS2 topics
- **Web GUI Control**: Enable/disable and set speed via web interface

## Hardware Changes

### OLED Display Updates
```
GOTO Mode:
JARSPACE MOUNT
GOTO Moving...
RA: 213.912 SPD:100
DEC: 019.165 SPD:100

Follow Mode:
JARSPACE MOUNT
FOLLOW SPD:1.5
RA: 213.912 SPD:050
DEC: 019.165 SPD:000
```

## ROS2 Usage

### 1. Start the Telescope Driver
```bash
ros2 run telescope_driver telescope_driver
```

### 2. Follow Mode Controller Node
```bash
# Enable follow mode at 1.0 deg/sec
ros2 run telescope_driver follow_mode --enable --speed 1.0

# Disable follow mode
ros2 run telescope_driver follow_mode --disable

# Enable follow mode at 2.5 deg/sec
ros2 run telescope_driver follow_mode --enable --speed 2.5
```

### 3. Manual Topic Control
```bash
# Enable torque first
ros2 topic pub /telescope/torque std_msgs/msg/Bool "{data: true}" --once

# Set follow speed to 1.5 deg/sec
ros2 topic pub /telescope/follow_speed std_msgs/msg/Float32 "{data: 1.5}" --once

# Enable follow mode
ros2 topic pub /telescope/follow_mode std_msgs/msg/Bool "{data: true}" --once

# Disable follow mode
ros2 topic pub /telescope/follow_mode std_msgs/msg/Bool "{data: false}" --once

# Send goto command (switches to goto mode automatically)
ros2 topic pub /stellarium/target geometry_msgs/msg/Vector3 "{x: 213.9125, y: 19.1647, z: 0.0}" --once
```

## ROS2 Topics

| Topic | Type | Description |
|-------|------|-------------|
| `/telescope/follow_mode` | `std_msgs/Bool` | Enable/disable follow mode |
| `/telescope/follow_speed` | `std_msgs/Float32` | Set follow speed (deg/sec) |
| `/telescope/torque` | `std_msgs/Bool` | Enable/disable motor torque |
| `/stellarium/target` | `geometry_msgs/Vector3` | Goto commands (switches to goto mode) |

## Web GUI Controls

The web interface will have:
- **Follow Mode Toggle**: Enable/disable follow mode
- **Speed Slider**: Set RA tracking speed (0.1 - 10 deg/sec)
- **Mode Indicator**: Shows current mode (GOTO/FOLLOW)

## Command Protocol

### T:4 - Follow Mode Control
```json
{
  "T": 4,
  "enable": true,
  "ra_speed": 1.5
}
```

### T:1 - Goto Command (automatically disables follow mode)
```json
{
  "T": 1,
  "ra_deg": 213.9125,
  "dec_deg": 19.1647,
  "ra_spd": 20.0,
  "dec_spd": 20.0
}
```

## Firmware Behavior

### Follow Mode
- Continuous RA tracking at fixed speed
- Small step movements for smooth tracking
- DEC motor remains stationary
- Speed displayed on OLED

### Goto Mode
- PID-controlled movements to target coordinates
- Speed scaling based on distance
- Both RA and DEC motors move
- Automatic switch from follow mode

## Speed Guidelines

| Purpose | Speed (deg/sec) | Use Case |
|---------|----------------|----------|
| Very Slow | 0.1 - 0.5 | High magnification astrophotography |
| Slow | 0.5 - 1.0 | General astrophotography |
| Medium | 1.0 - 2.0 | Visual observing |
| Fast | 2.0 - 5.0 | Wide field tracking |
| Very Fast | 5.0 - 10.0 | Testing/demo only |

## Test Script

Run the comprehensive test script:
```bash
cd /home/ayman/Jarspace
python3 test_follow_mode.py
```

This script demonstrates:
- Torque enable
- Goto commands
- Follow mode enable/disable
- Speed changes
- Mode switching

## Troubleshooting

### Motors not moving in follow mode:
1. Check torque is enabled: `ros2 topic echo /telescope/torque`
2. Verify follow mode is active: Check OLED display
3. Check serial monitor for "FOLLOW:" debug messages

### Mode not switching:
1. Send goto command to switch from follow to goto mode
2. Check for "FOLLOW MODE: DISABLED" message in serial monitor
3. Verify T:4 command is being sent

### Speed not changing:
1. Set speed before enabling follow mode
2. Check serial monitor for speed value
3. Verify speed is within 0.1-10.0 deg/sec range

## Expected Serial Monitor Output

### Follow Mode Enable:
```
FOLLOW MODE: ENABLED, RA speed=1.5
FOLLOW: RA dir=1, speed=15, steps=10
```

### Follow Mode Disable:
```
FOLLOW MODE: DISABLED
```

### Goto Command (auto-switch):
```
STELLARIUM: RA=213.912500° -> 4096 ticks, DEC=19.164700° -> 1638 ticks
MOTOR: ra_err=4096, dec_err=1638
```

## Integration Examples

### Astrophotography Sequence:
```bash
# 1. Start driver
ros2 run telescope_driver telescope_driver &

# 2. Enable torque
ros2 topic pub /telescope/torque std_msgs/msg/Bool "{data: true}" --once

# 3. Goto target object
ros2 topic pub /stellarium/target geometry_msgs/msg/Vector3 "{x: 45.0, y: 89.0, z: 0.0}" --once

# 4. Wait for goto to complete, then enable follow mode
sleep 10
ros2 run telescope_driver follow_mode --enable --speed 0.8

# 5. Start astrophotography
# (Camera control software here)

# 6. Stop tracking when done
ros2 run telescope_driver follow_mode --disable
```

Your telescope now has professional-grade follow mode tracking! 🌟🔭
