# Installation Guide for ros2_ws

## Required steps
1. Install ROS2 and source the ROS environment.
2. Install Python dependencies:
   ```bash
   cd /home/ayman/ros2_ws
   python3 -m pip install opencv-python numpy PyQt5 skyfield onnxruntime zwoasi
   ```
3. Install X11 helper tools for GUI forwarding:
   ```bash
   sudo apt update
   sudo apt install -y x11-xserver-utils
   ```
4. Install ZWO ASI SDK and driver:
   - Download the Linux ARM64 SDK from the ZWO website.
   - Copy `libASICamera2.so` to `/usr/local/lib`.
   - Run `sudo ldconfig`.

> If `zwoasi package not found` appears, install the Python package inside the container with `python3 -m pip install zwoasi`.
> If `/dev/video0` is not available, the camera driver is not installed or the camera device is not exposed into Docker.

## Build and run
```bash
cd /home/ayman/ros2_ws
colcon build --packages-select telescope_gui
source install/setup.bash
export DISPLAY=:0
xhost +local:root
./run_gui.sh
```

## Important: Install ZWO Drivers FIRST

Before running Docker or the GUI, install the ZWO ASI camera drivers on the host:

```bash
cd /home/ayman/ros2_ws
./install-zwo-drivers.sh
```

This script will:
- Download the ZWO ASI SDK
- Install the library and drivers
- Make the camera visible as `/dev/video*`

**After installation, reboot if the camera is not immediately visible:**

```bash
sudo reboot
```

Verify the camera is visible:

```bash
ls /dev/video*
```

---

## Docker setup (Recommended)

If you are running the workspace inside Docker, use the automated Docker launch script:

```bash
cd /home/ayman/ros2_ws
./docker-launch.sh
```

This script handles:
- X11 display forwarding for the GUI
- Camera device (`/dev/video0`) mounting
- Python dependency installation inside the container
- ROS2 environment setup
- Package build and GUI launch

### Requirements on the host:
- Docker installed and running
- Camera connected as `/dev/video0`
- X11 available (Linux desktop environments)

### Troubleshooting Docker launch:
- If `docker: invalid reference format` appears, ensure your Docker image name is correct in the script.
- If `/dev/video0` is not found, ensure the camera is connected and check `lsusb`.
- If X11 fails, ensure `xhost` is available or run the script with `DISPLAY=:0`.

---

## Manual Docker run (if script doesn't work)

Alternatively, run Docker manually:

```bash
docker run -it \
  --privileged \
  -e DISPLAY=$DISPLAY \
  -e QT_X11_NO_MITSHM=1 \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -v $HOME/.Xauthority:/root/.Xauthority:rw \
  --device /dev/video0 \
  --device /dev/dri \
  ros2_jazzy \
  bash
```

Then inside the container:

```bash
python3 -m pip install numpy zwoasi onnxruntime opencv-python PyQt5 skyfield
cd /root/ros2_ws
source /opt/ros/*/setup.bash
colcon build --packages-select telescope_gui
source install/setup.bash
export DISPLAY=:0
ros2 launch telescope_gui gui.launch.py
```

---

## Non-Docker setup

