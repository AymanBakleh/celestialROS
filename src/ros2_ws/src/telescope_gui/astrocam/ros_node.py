"""ROS2 integration for the AstroCam PyQt5 app.

The PyQt5 GUI never talks to the camera hardware directly. A separate ROS2 camera
node — maintained outside this repo — publishes ``sensor_msgs/msg/Image`` and
exposes ``Gain`` / ``Exposure`` / ``Offset`` parameters. This module:

* subscribes to the image topic and decodes frames into uint8 RGB NumPy arrays,
* bridges those frames onto the Qt main thread via a thread-safe queued signal,
* pushes camera parameter changes back to the camera node.

All ``rclpy`` imports are done lazily so the GUI modules can be imported (and the
CV unit tests can run) on a machine without ROS2 installed — e.g. a Windows dev box.
"""
from __future__ import annotations

import numpy as np
from PyQt5.QtCore import QObject, QThread, pyqtSignal

DEFAULT_TOPIC = "/camera/image_raw"
DEFAULT_CAMERA_NODE = "/zwo_camera"


class RosBridge(QObject):
    """Carries data from the ROS spin thread to the Qt main thread.

    Qt delivers signals emitted from a non-GUI thread to slots in the GUI thread
    using *queued connections*, which is the safe way to hand frames across the
    thread boundary. Emit on these signals only — never touch widgets from ROS code.
    """

    frame_ready = pyqtSignal(object)   # np.ndarray, HxWx3 uint8 RGB
    status = pyqtSignal(dict)          # {"connected": bool, "topic": str, "temperature_c": float|None}
    error = pyqtSignal(str)


def decode_image(msg) -> np.ndarray | None:
    """Convert a ``sensor_msgs/msg/Image`` into a contiguous uint8 RGB array.

    Handles the encodings a ZWO/USB camera node is likely to publish and respects
    the row ``step`` (which may include padding). Returns ``None`` for an encoding
    we do not understand rather than raising, so a stray frame can't kill the node.
    """
    height = msg.height
    width = msg.width
    step = msg.step or width
    encoding = (msg.encoding or "rgb8").lower()
    raw = np.frombuffer(bytes(msg.data), dtype=np.uint8)

    if encoding in ("mono8", "8uc1"):
        rows = raw.reshape(height, step)[:, :width]
        return np.repeat(rows[..., None], 3, axis=2).copy()

    if encoding in ("mono16", "16uc1"):
        # Reinterpret as 16-bit, scale down to 8-bit, then expand to 3 channels.
        rows16 = raw.view(np.uint16).reshape(height, step // 2)[:, :width]
        rows8 = (rows16 >> 8).astype(np.uint8)
        return np.repeat(rows8[..., None], 3, axis=2).copy()

    if encoding in ("rgb8", "bgr8", "rgba8", "bgra8"):
        channels = 4 if encoding in ("rgba8", "bgra8") else 3
        rows = raw.reshape(height, step)[:, : width * channels]
        frame = rows.reshape(height, width, channels)[..., :3]
        if encoding.startswith("bgr"):
            frame = frame[..., ::-1]
        return np.ascontiguousarray(frame)

    return None


def _make_camera_node(bridge: RosBridge, topic: str):
    """Build the rclpy Node subclass. Imported lazily so the GUI loads without ROS2."""
    import rclpy
    from rclpy.node import Node
    from rclpy.qos import qos_profile_sensor_data
    from sensor_msgs.msg import Image

    class CameraNode(Node):
        def __init__(self) -> None:
            super().__init__("astrocam_viewer")
            self._bridge = bridge
            self._topic = topic
            self._sub = None
            self._subscribe(topic)

        def _subscribe(self, topic: str) -> None:
            if self._sub is not None:
                self.destroy_subscription(self._sub)
            self._topic = topic
            self._sub = self.create_subscription(
                Image, topic, self._on_image, qos_profile_sensor_data
            )
            self._bridge.status.emit(
                {"connected": False, "topic": topic, "temperature_c": None}
            )

        def change_topic(self, topic: str) -> None:
            if topic and topic != self._topic:
                self._subscribe(topic)

        @property
        def topic(self) -> str:
            return self._topic

        def _on_image(self, msg) -> None:
            try:
                frame = decode_image(msg)
            except Exception as exc:  # never let a bad frame crash the spin loop
                self._bridge.error.emit(f"decode failed: {exc}")
                return
            if frame is None:
                self._bridge.error.emit(f"unsupported encoding: {msg.encoding}")
                return
            self._bridge.status.emit(
                {"connected": True, "topic": self._topic, "temperature_c": None}
            )
            self._bridge.frame_ready.emit(frame)

    return CameraNode()


class RosWorker(QThread):
    """Spins the ROS2 node in a background thread, polling so it can stop cleanly."""

    def __init__(self, bridge: RosBridge, topic: str = DEFAULT_TOPIC) -> None:
        super().__init__()
        self._bridge = bridge
        self._topic = topic
        self._running = False
        self.node = None
        self._rclpy = None

    def run(self) -> None:  # executes in the new thread
        try:
            import rclpy

            self._rclpy = rclpy
            if not rclpy.ok():
                rclpy.init()
            self.node = _make_camera_node(self._bridge, self._topic)
        except Exception as exc:
            self._bridge.error.emit(f"ROS2 init failed: {exc}")
            return

        self._running = True
        while self._running and self._rclpy.ok():
            try:
                self._rclpy.spin_once(self.node, timeout_sec=0.05)
            except Exception as exc:
                self._bridge.error.emit(f"spin error: {exc}")
                break

        try:
            if self.node is not None:
                self.node.destroy_node()
        except Exception:
            pass

    def stop(self) -> None:
        self._running = False
        self.wait(2000)


class CameraParamClient:
    """Pushes UI camera settings to the ROS2 camera node via ``set_parameters``.

    Targets a configurable node name (default ``/zwo_camera``). All calls are
    best-effort and fail quietly if the camera node is not present, so the GUI
    stays responsive when running against a simple publisher with no parameters.
    """

    def __init__(self, node, camera_node_name: str = DEFAULT_CAMERA_NODE) -> None:
        self._node = node
        self._client = None
        self.set_camera_node(camera_node_name)

    def set_camera_node(self, name: str) -> None:
        self._camera_node_name = name.lstrip("/")
        self._client = None  # recreated lazily on next send

    def _ensure_client(self) -> bool:
        if self._node is None:
            return False
        if self._client is not None:
            return True
        try:
            from rcl_interfaces.srv import SetParameters

            service = f"/{self._camera_node_name}/set_parameters"
            self._client = self._node.create_client(SetParameters, service)
            return True
        except Exception:
            return False

    def send(self, *, exposure_us: float | None = None,
             gain: int | None = None, offset: int | None = None) -> None:
        if self._node is None or not self._ensure_client():
            return
        try:
            from rcl_interfaces.msg import Parameter, ParameterType, ParameterValue
            from rcl_interfaces.srv import SetParameters

            params = []
            if exposure_us is not None:
                params.append(Parameter(
                    name="Exposure",
                    value=ParameterValue(type=ParameterType.PARAMETER_DOUBLE,
                                         double_value=float(exposure_us)),
                ))
            if gain is not None:
                params.append(Parameter(
                    name="Gain",
                    value=ParameterValue(type=ParameterType.PARAMETER_INTEGER,
                                         integer_value=int(gain)),
                ))
            if offset is not None:
                params.append(Parameter(
                    name="Offset",
                    value=ParameterValue(type=ParameterType.PARAMETER_INTEGER,
                                         integer_value=int(offset)),
                ))
            if not params:
                return
            if not self._client.service_is_ready():
                # Don't block the GUI; the camera node may not be up yet.
                self._client.wait_for_service(timeout_sec=0.0)
                if not self._client.service_is_ready():
                    return
            request = SetParameters.Request(parameters=params)
            self._client.call_async(request)  # fire-and-forget
        except Exception:
            # Best-effort: never propagate a parameter failure into the UI.
            return
