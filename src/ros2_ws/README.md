# ros2_ws Telescope GUI

This workspace contains the `telescope_gui` ROS2 package updated to support ZWO ASI662MC cameras and YOLO inference-ready GUI overlays.

## Setup (IMPORTANT: Do this first)

Install the ZWO camera drivers on the host:

```bash
cd /home/ayman/ros2_ws
./install-zwo-drivers.sh
sudo reboot
```

After reboot, verify the camera is visible:

```bash
ls /dev/video*
```

## Quick start (Docker)
If you are running in Docker, use the automated script:

```bash
cd /home/ayman/ros2_ws
./docker-launch.sh
```

This handles X11 forwarding, camera device mounting, dependency installation, and GUI launch.

## Quick start (Bare metal / Raspberry Pi)
1. Install ROS2 and Python dependencies.
2. Build the package:
   ```bash
   cd /home/ayman/ros2_ws
   colcon build --packages-select telescope_gui
   ```
3. Source the workspace:
   ```bash
   source install/setup.bash
   ```
4. Start the GUI:
   ```bash
   export DISPLAY=:0
   xhost +local:root
   ./run_gui.sh
   ```

> If `xhost` is not found, install `x11-xserver-utils`.
> If `zwoasi package not found`, install `python3 -m pip install zwoasi`.
> If `/dev/video0` does not exist, ensure the camera driver is installed and device access is granted to Docker.

## Notes
- The launch file now starts `zwo_camera_node` instead of `usb_cam_node_exe`.
- If you see the old USB camera node, you are launching from the wrong workspace.
- Make sure ZWO drivers are installed and the camera device is accessible.
