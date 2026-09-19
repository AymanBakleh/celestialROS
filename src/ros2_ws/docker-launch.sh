#!/bin/bash
set -e

# Telescope GUI Docker Launch Script
# This script sets up and runs the ROS2 telescope GUI in Docker with X11 forwarding and camera access

# Default Docker images to try, in priority order.
# You can override this by exporting DOCKER_IMAGE before running the script.
DEFAULT_DOCKER_IMAGE="ros2_jazzy_image_with_gui:latest"
FALLBACK_DOCKER_IMAGE="ros:jazzy-ros-base"
DOCKER_IMAGE="${DOCKER_IMAGE:-$DEFAULT_DOCKER_IMAGE}"
CONTAINER_NAME="ros2_jazzy"
WORKSPACE_DIR="${WORKSPACE_DIR:-$PWD}"
ZWO_HOST_LIB="/usr/lib/aarch64-linux-gnu/libASICamera2.so"
DISPLAY_NUM="${DISPLAY:=:0}"
ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-0}"
ROS_LOCALHOST_ONLY="${ROS_LOCALHOST_ONLY:-0}"


echo "=========================================="
echo "Telescope GUI Docker Launch Script"
echo "=========================================="
echo "ROS_DOMAIN_ID: $ROS_DOMAIN_ID"

# Check if DISPLAY is set
if [ -z "$DISPLAY" ]; then
    echo "WARNING: DISPLAY is not set. X11 forwarding may not work."
    echo "Setting DISPLAY to :0"
    export DISPLAY=:0
fi

echo "Workspace: $WORKSPACE_DIR"
echo "Display: $DISPLAY"

# Choose Docker image
if ! docker image inspect "$DOCKER_IMAGE" >/dev/null 2>&1; then
    echo "NOTE: configured image '$DOCKER_IMAGE' not found locally. Trying available local Jazzy images..."
    if docker image inspect "ros2_jazzy_image_with_gui:latest" >/dev/null 2>&1; then
        DOCKER_IMAGE="ros2_jazzy_image_with_gui:latest"
    elif docker image inspect "ros:jazzy-ros-base" >/dev/null 2>&1; then
        DOCKER_IMAGE="ros:jazzy-ros-base"
    elif docker image inspect "ros:jazzy-desktop" >/dev/null 2>&1; then
        DOCKER_IMAGE="ros:jazzy-desktop"
    elif docker image inspect "osrf/ros:jazzy-desktop" >/dev/null 2>&1; then
        DOCKER_IMAGE="osrf/ros:jazzy-desktop"
    else
        echo ""
        echo "ERROR: No local ROS2 Jazzy Docker image found."
        echo "Please set DOCKER_IMAGE to one of your available images, for example:"
        echo "  export DOCKER_IMAGE=ros2_jazzy_image_with_gui:latest"
        echo "  ./docker-launch.sh"
        exit 1
    fi
    echo "Using Docker image: $DOCKER_IMAGE"
fi

# Find available video devices
echo "Searching for camera devices..."
CAMERA_DEVICE=""
for device in /dev/video{0,19,20,21,22,23,24,25}; do
    if [ -e "$device" ]; then
        CAMERA_DEVICE="$device"
        echo "Found camera device: $CAMERA_DEVICE"
        break
    fi
done

if [ -z "$CAMERA_DEVICE" ]; then
    echo ""
    echo "WARNING: No camera device found at /dev/video0 or /dev/video19-25"
    echo ""
    echo "To fix this, install ZWO drivers on the host:"
    echo "  1. Download the ZWO ASI SDK for Linux ARM64"
    echo "  2. Extract and install: sudo make -C libusbwrapper install"
    echo "  3. Install libusb: sudo apt install libusb-1.0-0"
    echo "  4. Reboot or reload the drivers"
    echo ""
    echo "After installing drivers, run this script again."
    exit 1
fi

# Find available serial devices so the container can talk to the telescope mount.
echo "Searching for serial devices..."
SERIAL_DEVICE=""
for device in /dev/ttyUSB* /dev/ttyACM* /dev/serial/by-id/* /dev/serial/by-path/*; do
    if [ -e "$device" ]; then
        SERIAL_DEVICE="$device"
        echo "Found serial device: $SERIAL_DEVICE"
        break
    fi
done

if [ -z "$SERIAL_DEVICE" ]; then
    echo "WARNING: No serial device found. The telescope mount will not be reachable from inside Docker."
    echo "Expected a device such as /dev/ttyUSB0 or /dev/ttyACM0 on the host."
fi

# Allow X11 access from localhost
echo "Configuring X11 access..."
xhost +local:root 2>/dev/null || echo "Note: xhost may not be available, X11 may still work"

# Prepare X11 socket and authority
X11_SOCKET="/tmp/.X11-unix"
X11_AUTH="$HOME/.Xauthority"

if [ ! -e "$X11_SOCKET" ]; then
    echo "WARNING: X11 socket not found at $X11_SOCKET"
fi

echo ""
echo "=========================================="
echo "Starting Docker container..."
echo "Camera device: $CAMERA_DEVICE"
echo "=========================================="

# Reuse a named container so installs and work persist between runs.
if docker ps -a --format '{{.Names}}' | grep -Fxq "$CONTAINER_NAME"; then
    CURRENT_NETMODE=$(docker inspect "$CONTAINER_NAME" --format '{{.HostConfig.NetworkMode}}' 2>/dev/null || true)
    HAS_WORKSPACE_MOUNT=$(docker inspect "$CONTAINER_NAME" --format '{{json .Mounts}}' 2>/dev/null | grep -q '"Destination":"/root/ros2_ws"' && echo yes || echo no)

    if [ "$CURRENT_NETMODE" = "host" ] && [ "$HAS_WORKSPACE_MOUNT" = "yes" ]; then
        echo "Reusing existing host-networked container: $CONTAINER_NAME"
        docker start "$CONTAINER_NAME" >/dev/null 2>&1 || true
    else
        echo "Existing container '$CONTAINER_NAME' is missing the expected host networking or workspace mount. Recreating..."
        docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
    fi
fi

if ! docker ps -a --format '{{.Names}}' | grep -Fxq "$CONTAINER_NAME"; then
    echo "Creating new container: $CONTAINER_NAME"
    docker run -d \
        --name "$CONTAINER_NAME" \
        --privileged \
        --network host \
        -e DISPLAY="$DISPLAY_NUM" \
        -e QT_X11_NO_MITSHM=1 \
        -e VIDEO_DEVICE="$CAMERA_DEVICE" \
        -e SERIAL_DEVICE="$SERIAL_DEVICE" \
        -e ZWO_LIB_PATH="/usr/lib/aarch64-linux-gnu/libASICamera2.so" \
        -e ROS_DOMAIN_ID="$ROS_DOMAIN_ID" \
        -e ROS_LOCALHOST_ONLY="$ROS_LOCALHOST_ONLY" \
        -e RMW_IMPLEMENTATION="rmw_cyclonedds_cpp" \
        -v "$X11_SOCKET:$X11_SOCKET" \
        -v "$X11_AUTH:/root/.Xauthority:rw" \
        -v "$WORKSPACE_DIR:/root/ros2_ws:rw" \
        -v /usr/local/lib:/usr/local/lib:ro \
        $(if [ -f "$ZWO_HOST_LIB" ]; then echo "-v $ZWO_HOST_LIB:$ZWO_HOST_LIB:ro"; fi) \
        -w /root/ros2_ws \
        --device "$CAMERA_DEVICE" \
        $(if [ -n "$SERIAL_DEVICE" ]; then echo "--device $SERIAL_DEVICE"; fi) \
        --device /dev/dri \
        --device /dev/bus/usb \
        "$DOCKER_IMAGE" \
        bash -lc 'sleep infinity' >/dev/null
fi

# Make sure the container is actually running before entering it.
for retry in 1 2 3; do
    if docker ps --format '{{.Names}}' | grep -Fxq "$CONTAINER_NAME"; then
        break
    fi
    echo "Starting container: $CONTAINER_NAME"
    docker start "$CONTAINER_NAME" >/dev/null 2>&1 || true
    sleep 2
 done

if ! docker ps --format '{{.Names}}' | grep -Fxq "$CONTAINER_NAME"; then
    echo "ERROR: container $CONTAINER_NAME is still not running after startup attempts."
    echo "Try: docker ps -a | grep $CONTAINER_NAME"
    exit 1
fi

# Initialize the persistent container and drop into an interactive shell.
docker exec -it "$CONTAINER_NAME" bash -lc "
    set -e

    echo '=========================================='
    echo 'Inside Docker Container'
    echo '=========================================='

    if ! python3 -m pip --version >/dev/null 2>&1; then
        echo 'pip not found, installing python3-pip...'
        apt-get update -qq
        apt-get install -y python3-pip
    fi

    echo 'Checking ASI SDK availability...'
    python3 - <<'PY' || {
import sys
try:
    import zwoasi
    try:
        zwoasi.init('/usr/lib/aarch64-linux-gnu/libASICamera2.so')
        print('ASI SDK available')
    except Exception as e:
        print('ASI SDK init failed:', e)
        sys.exit(2)
except Exception as e:
    print('zwoasi import failed:', e)
    sys.exit(3)
PY
        if [ -f /usr/lib/aarch64-linux-gnu/libASICamera2.so ]; then
            echo 'ASI SDK shared library already present; skipping apt install.'
        else
            echo 'ASI SDK not available in container; installing libasi...'
            apt-get update -qq
            apt-get install -y libasi || true
        fi
        # update linker cache and re-check
        ldconfig || true
    }

    echo 'Installing build dependencies for camera-zwo-asi...'
    apt-get install -y libusb-1.0-0-dev libusb-dev pkg-config cmake ninja-build || true

    echo 'Checking Python dependencies...'
    python3 - <<'PY'
import importlib.util
import subprocess
import sys

requirements = [
    ('numpy', 'numpy'),
    ('zwoasi', 'zwoasi'),
    ('onnxruntime', 'onnxruntime'),
    ('opencv-python', 'cv2'),
    ('PyQt5', 'PyQt5'),
    ('skyfield', 'skyfield'),
    ('requests', 'requests'),
    ('pyserial', 'serial'),
]
missing = [pkg for pkg, module in requirements if importlib.util.find_spec(module) is None]
if missing:
    subprocess.check_call([sys.executable, '-m', 'pip', 'install', '--quiet', '--break-system-packages', *missing])
PY

    echo 'Installing ROS build tools if needed...'
    command -v colcon >/dev/null 2>&1 || apt-get install -y python3-colcon-common-extensions

    echo 'Installing CycloneDDS RMW runtime for ROS 2...'
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y ros-jazzy-rmw-cyclonedds-cpp ros-jazzy-cyclonedds
    ldconfig || true

    cd /root/ros2_ws || exit 1
    echo 'Sourcing ROS2 environment...'
    source /opt/ros/*/setup.bash || source /ros_entrypoint.sh

    if [ -d /root/ros2_ws/src/telescope_gui ]; then
        echo 'Building telescope_gui package if needed...'
        colcon build --packages-select telescope_gui >/tmp/telescope_gui_build.log 2>&1 || (cat /tmp/telescope_gui_build.log; exit 1)
        source install/setup.bash
    fi

    export DISPLAY=\"$DISPLAY_NUM\"
    export XAUTHORITY=/root/.Xauthority

    echo ''
    echo '=========================================='
    echo 'Container ready.'
    echo 'Run your ROS launch commands here.'
    echo 'Example: source install/setup.bash && ros2 launch telescope_gui gui.launch.py'
    echo '=========================================='
    echo ''

    exec bash
"

echo ""
echo "=========================================="
echo "Docker container stopped."
echo "=========================================="
