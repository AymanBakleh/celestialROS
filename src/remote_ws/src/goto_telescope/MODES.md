# Telescope Control Modes

This document describes the modes of operation for the telescope control system.

## Simulation Mode

**Status:** ✅ Implemented

Simulation mode has two sub-modes:

### 1. Manual Mode

**Status:** ✅ Implemented

Manual mode allows you to manually control the telescope:
- **telescope_description**: Visualizes the telescope in RViz using URDF
- **Stellarium**: Shows the sky view (for reference)
- **joint_state_publisher_gui**: Manual control of all joints (polar_align, ra, dec)
- **No automatic tracking**: You control everything manually

### How it works:
1. User moves telescope via `joint_state_publisher_gui`
2. `robot_state_publisher` publishes TF transforms for visualization in RViz
3. `coordinate_transformer` converts joint states to RA/DEC (for display)
4. Stellarium shows the sky view (not connected to telescope movement)

### Launch:
```bash
ros2 launch goto_telescope telescope_manual.launch.py
```

### 2. Autonomous Tracking Mode

**Status:** ✅ Implemented

Autonomous tracking mode provides full automatic control:
- **telescope_description**: Visualizes the telescope in RViz
- **Stellarium**: Controls the telescope automatically
- **Full integration**: Stellarium commands move the virtual telescope in RViz
- **Automatic tracking**: Telescope follows Stellarium targets

### How it works:
1. User selects a target in Stellarium
2. `stellarium_bridge` receives the target (RA/DEC) from Stellarium
3. `telescope_joint_bridge` converts RA/DEC to joint positions
4. `joint_state_publisher_wrapper` ensures all joints are published
5. `robot_state_publisher` publishes TF transforms for visualization
6. `coordinate_transformer` converts joint states back to RA/DEC for state tracking

### Launch:
```bash
ros2 launch goto_telescope telescope_tracking.launch.py
```

## Hardware Mode

**Status:** 🚧 TODO

In hardware mode, the system controls only the physical telescope:
- **telescope_driver**: Communicates with physical telescope hardware
- **Stellarium**: Provides target selection
- No simulation visualization

### Implementation Plan:
1. Initialize `telescope_driver` in `telescope_controller`
2. Convert Stellarium RA/DEC to hardware commands
3. Send commands via serial/USB to telescope mount
4. Read position feedback from hardware

### Launch (when implemented):
```bash
ros2 launch goto_telescope telescope_hardware.launch.py mode:=hardware
```

## Hybrid Mode

**Status:** 🚧 TODO

In hybrid mode, both simulation and hardware move together:
- **telescope_description**: Shows expected position in RViz
- **telescope_driver**: Moves physical telescope
- **Stellarium**: Provides target selection
- Both systems are synchronized

### Implementation Plan:
1. Execute commands in both simulation and hardware simultaneously
2. Monitor both for position feedback
3. Handle discrepancies (e.g., hardware limits, tracking errors)
4. Provide visual feedback of actual vs expected position

### Launch (when implemented):
```bash
ros2 launch goto_telescope telescope_hybrid.launch.py mode:=hybrid
```

## Architecture

### Manual Mode Architecture

```
┌──────────────────────┐
│joint_state_publisher │
│        _gui          │───► /joint_states
└──────┬───────────────┘
       │
       ▼
┌──────────────────────┐
│robot_state_publisher │───► RViz (TF visualization)
└──────┬───────────────┘
       │
       ▼
┌──────────────────────┐
│coordinate_transformer│───► /telescope/state (RA/DEC)
└──────────────────────┘
```

### Tracking Mode Architecture

```
┌─────────────┐
│  Stellarium │
└──────┬──────┘
       │ RA/DEC
       ▼
┌──────────────────┐
│ stellarium_bridge│
└──────┬───────────┘
       │ /stellarium/target
       ▼
┌──────────────────────┐
│telescope_joint_bridge│
└──────┬───────────────┘
       │ /telescope/joint_commands
       ▼
┌──────────────────────────────┐
│joint_state_publisher_wrapper │
└──────┬───────────────────────┘
       │ /joint_states
       ▼
┌──────────────────────┐
│robot_state_publisher │───► RViz (TF visualization)
└──────┬───────────────┘
       │
       ▼
┌──────────────────────┐
│coordinate_transformer│───► /telescope/state (RA/DEC)
└──────────────────────┘
```

## Configuration

Mode is set in `config/telescope_params.yaml`:
```yaml
mode: "simulation"  # or "hardware" or "hybrid"
```

Or via launch parameter:
```bash
ros2 launch goto_telescope telescope_sim.launch.py mode:=simulation
```

## Next Steps for Hardware/Hybrid Modes

1. **Implement hardware driver interface** in `telescope_controller.execute_goto_hardware()`
2. **Create hardware launch file** that includes `telescope_driver` package
3. **Add error handling** for hardware communication failures
4. **Implement synchronization** for hybrid mode
5. **Add safety checks** (e.g., hardware limits, collision detection)