# Stellarium Integration Setup

This document explains how to configure Stellarium to work with the telescope control system.

## Stellarium Configuration

### Required Add-ons

1. **Remote Control Add-on** (port 8090)
   - Enable "Server enabled" on startup
   - Listening on: `localhost` (or `127.0.0.1`)
   - Port: `8090` (default)

2. **Telescope Control Add-on** (port 10001)
   - Enable "External software or remote computer"
   - Coordinate system: `J2000` (default) or `JNow` (equinox of date)
   - Connection delay: `0.5s`
   - Host: `localhost`
   - TCP Port: `10001`
   - Field of view indicator: Enabled (set to match your telescope's field of view)
   - Optical Tube Dimensions: Set to your telescope size (e.g., 57.5 x 16.0 cm)

### Configuration File

The configuration is in `config/telescope_params.yaml`:

```yaml
stellarium:
  remote_control:
    host: "localhost"  # or "127.0.0.1" if needed
    port: 8090
  telescope_control:
    host: "localhost"   # or "127.0.1.1" if that's what Stellarium shows
    port: 10001
  coordinate_system: "J2000"  # Must match Stellarium config
  connection_delay: 0.5
```

**Important:** If Stellarium shows IP `127.0.1.1`, you can set:
```yaml
  telescope_control:
    host: "127.0.1.1"
```

## How It Works

### Remote Control (Port 8090)
- **Purpose**: Receive targets/commands from Stellarium
- **Protocol**: JSON over TCP
- **Usage**: When you select a target in Stellarium, it sends RA/DEC coordinates
- **Direction**: Stellarium → ROS

### Telescope Control (Port 10001)
- **Purpose**: Send telescope position to Stellarium
- **Protocol**: Binary (RA and DEC as doubles in radians)
- **Usage**: Makes Stellarium's view follow the telescope
- **Direction**: ROS → Stellarium
- **Format**: 16 bytes total (8 bytes RA + 8 bytes DEC, network byte order)

## Data Flow

### Manual Mode
1. User moves telescope via `joint_state_publisher_gui`
2. `coordinate_transformer` converts joint states to RA/DEC
3. `stellarium_bridge` sends position to Stellarium (port 10001)
4. Stellarium's view follows the telescope

### Tracking Mode
1. User selects target in Stellarium
2. Stellarium sends target via Remote Control (port 8090)
3. `stellarium_bridge` receives target and publishes to `/stellarium/target`
4. `telescope_joint_bridge` converts RA/DEC to joint positions
5. Telescope moves in RViz
6. `coordinate_transformer` converts new position to RA/DEC
7. `stellarium_bridge` sends position back to Stellarium (port 10001)
8. Stellarium's view follows the telescope

## Troubleshooting

### Stellarium Not Moving with Telescope

1. **Check Telescope Control Add-on is enabled**
   - Go to Stellarium → Configuration → Plugins → Telescope Control
   - Ensure "Load at startup" is checked
   - Ensure "External software or remote computer" is selected

2. **Verify Port Configuration**
   - Check Stellarium shows port 10001
   - Verify `telescope_params.yaml` has matching port
   - If Stellarium shows `127.0.1.1`, update config to use that IP

3. **Check Connection**
   ```bash
   # Check if bridge is connecting
   ros2 topic echo /telescope/state
   
   # Check logs
   ros2 run goto_telescope stellarium_bridge
   ```

4. **Verify Coordinate System**
   - Stellarium config must match `telescope_params.yaml`
   - Both should be `J2000` or both `JNow`

5. **Test Connection**
   ```bash
   # Test if port is accessible
   telnet localhost 10001
   # or
   nc -zv localhost 10001
   ```

### Field of View Indicator

The field of view indicator (circle) shows where the telescope is pointing in Stellarium. 
- Size should match your telescope's field of view
- For 57.5 x 16.0 cm telescope, a 16 arcminute circle is appropriate
- The indicator will move as the telescope moves

## Coordinate Systems

- **J2000**: Equatorial coordinates at epoch J2000.0 (January 1, 2000, 12:00 TT)
- **JNow**: Equatorial coordinates at current date/time (accounts for precession)

For most purposes, J2000 is recommended as it's a fixed reference frame.

## Notes

- The Telescope Control protocol uses binary format, not JSON
- Coordinates are sent in radians (not degrees or hours)
- Network byte order (big-endian) is used
- Position updates are sent at ~10 Hz to avoid overwhelming Stellarium
- Connection delay of 0.5s is applied after connecting to match Stellarium's configuration