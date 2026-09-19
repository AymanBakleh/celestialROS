#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_ROOT"

if [ ! -f install/setup.bash ]; then
  echo "Workspace has not been built yet. Run 'colcon build --packages-select telescope_gui' first."
  exit 1
fi

if [ -z "$DISPLAY" ]; then
  export DISPLAY=":0"
  echo "DISPLAY was not set. Defaulting to :0"
fi

if command -v xhost >/dev/null 2>&1; then
  echo "Allowing local root access to X server"
  xhost +local:root >/dev/null 2>&1 || true
fi

source install/setup.bash
ros2 launch telescope_gui gui.launch.py
