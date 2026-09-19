# Workspace Summary

This workspace is a telescope-control project built around ROS 2 and a custom astronomy/firmware layer. In plain terms, it is trying to model, visualize, and control a telescope mount that can track celestial targets, work in simulation, and eventually interface with real hardware.

## 1. What this workspace contains

At the root, there are a few important areas:

- `src/`: the ROS 2 packages that define the system
- `build/`: generated build artifacts from `colcon`
- `install/`: installed package outputs and environment setup scripts
- `log/`: build/run logs from previous ROS builds
- `telescope_firmware/`: a separate firmware/tooling area for astronomy math, motion logic, and telescope control logic

This is not one app; it is a multi-package robotics project with both simulation and hardware-oriented components.

## 2. Main ROS packages

### `src/goto_telescope`
This is the main control stack.

It includes:

- launch files for different modes (`telescope_sim.launch.py`, `telescope_hardware.launch.py`, etc.)
- telescope parameters in `config/telescope_params.yaml`
- nodes that bridge with Stellarium and convert coordinates to joint motion
- simulation and tracking logic for telescope movement

Important concepts in this package:

- `coordinate_transformer`: converts joint states into telescope state such as RA/DEC
- `stellarium_serial_bridge_*`: supports communication with Stellarium via serial-like protocol
- `fake_mount_simulator`: simulates telescope motion for testing and visualization
- `telescope_joint_bridge`: likely turns RA/DEC target requests into mount joint commands

The project documents describe a few intended modes:

- manual/simulation mode
- autonomous tracking mode
- hardware mode
- hybrid mode

The docs show that simulation/manual tracking is the most mature path, while hardware and hybrid operation are planned or partially implemented.

### `src/telescope_description`
This package defines the telescope URDF/Xacro model and visualization assets.

It contains:

- the robot model in `urdf/telescope_description.urdf.xacro`
- launch files for display and RViz
- robot description configuration files

This is the visual representation of the mount in RViz and the source of the robot geometry used by `robot_state_publisher`.

### `src/fake_mount`
This package behaves like an isolated simulation/test harness.

It includes:

- a fake mount simulator
- prefixed robot frames so the fake mount does not conflict with the real hardware model
- a fake Stellarium bridge
- RViz setup to watch the simulated telescope

The launch file explicitly notes that it generates a prefixed robot description to avoid joint and frame name collisions, which is a common pattern when running a fake system alongside hardware/system components.

### `src/camera_viewer`
This package is a simpler camera display utility for the telescope system.

It depends on `rclpy`, `sensor_msgs`, and `cv_bridge`, which suggests it subscribes to camera topics and renders or processes image data for local monitoring.

### `src/telescope_rviz`
This package focuses on RViz-based visualization of joint states, especially for hardware-related telescope data.

It contains `joint_state_fixer.py`, which suggests some joint state normalization or cleanup is being done before the data is displayed or used.

## 3. What the project is trying to do

The overall behavior is a telescope automation stack that does roughly this:

1. The telescope model is described in URDF.
2. A simulation or hardware mount publishes joint states.
3. A transformer turns those joint states into telescope coordinates (for example RA/DEC or equivalent sky position).
4. Stellarium or another client provides target coordinates.
5. The system converts the target into mount movement commands.
6. RViz visualizes the motion and the telescope state.

This is a familiar robotics pattern: simulate or drive hardware, publish state, transform coordinates, and visualize the result.

## 4. Simulation and mode logic

The documentation in `src/goto_telescope/MODES.md` explains the system design:

- Manual/simulation mode: the user moves the telescope via a GUI or simulated joint states
- Autonomous tracking mode: Stellarium sends target RA/DEC and the system follows it
- Hardware mode: the real mount is controlled directly, without simulation
- Hybrid mode: both simulation and hardware operate together

The actual launch files reflect this fairly well:

- `telescope_sim.launch.py` starts the fake mount simulator and RViz only
- `telescope_hardware.launch.py` starts robot_state_publisher and serial bridge endpoints for hardware use and includes the Stellarium interface
- `fake_mount.launch.py` creates a fake, isolated setup with a prefixed robot model and RViz

## 5. The astronomy and firmware side

The `telescope_firmware/` directory is not a ROS package; it is a separate collection of scientific/firmware utility code and documentation.

It contains references to:

- Julian day calculations
- Greenwich Mean Sidereal Time (GMST)
- Local Sidereal Time (LST)
- Hour Angle (HA)
- RA/DEC processing
- meridian flip logic
- motor step calculations

The summary file there makes it clear that the project has invested a lot of effort into understanding how to compute sky coordinates and how to handle telescope axis motion, especially around meridian crossing and flip behavior.

This is the part that turns a mount model into actual astronomy logic instead of just a robot visualization.

## 6. Current state of the project

From the structure and docs, the project appears to be in a transitional state:

- the simulation and visualization parts are present and fairly organized
- the telescope package and astronomy utilities are substantial
- Stellarium integration is planned and partially implemented
- hardware mode and hybrid mode are explicitly described as TODO or partially implemented
- the build directories show this project has been built before and is not brand-new

In other words, this workspace is an active telescope automation and robotics project where the team has built the skeleton, simulation, coordinate logic, and astronomy model, but is still working through the real hardware integration layer.

## 7. Practical interpretation

If you were to run the code in this workspace, the likely flow is:

- start a simulation or telescope launch
- visualize the robot in RViz
- publish or emulate telescope joint states
- convert mount state into RA/DEC and related astronomy values
- optionally connect Stellarium to control targeting
- eventually replace or augment the simulation with real hardware motion commands

So the project is not just visualization: it is a real mount-control and astronomy pipeline with repeated attempts to move from simulation toward physical telescope operation.

## 8. Bottom line

This workspace is a ROS 2 telescope control project for a simulated and eventually hardware-driven astronomical mount. It mixes:

- robot modeling and visualization
- telescope kinematics and joint-state pipelines
- Stellarium-based targeting integration
- astronomy calculations for tracking and meridian behavior
- firmware-style logic for real mount motion

The system is most mature in simulation and coordinate/visualization pipelines, with the hardware path still being developed.
