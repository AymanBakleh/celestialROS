"""MainWindow: top bar, icon rail, live preview, and a collapsible panel stack."""
from __future__ import annotations

from datetime import datetime
from pathlib import Path

import numpy as np
from PyQt5.QtCore import QElapsedTimer, QEvent, QSettings, Qt
from PyQt5.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPushButton,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)

from ..cv.processing import frame_summary, histogram_rgb, stretch_image
from ..ros_node import (
    DEFAULT_CAMERA_NODE,
    DEFAULT_TOPIC,
    CameraParamClient,
    RosBridge,
    RosWorker,
)
from .panels import AnalysisPanel, CameraPanel, ConnectPanel
from .preview_widget import PreviewWidget
from .theme import PANEL_WIDTH, RAIL_WIDTH, TOPBAR_HEIGHT, apply_theme

ANALYSIS_INTERVAL_MS = 250
ANALYSIS_DOWNSAMPLE = 2

IMAGES_DIR = Path(__file__).resolve().parents[2] / "images"
DESIGN_SIZE = (1024, 600)
MINIMUM_SIZE = (800, 480)


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("AstroCam")
        self.resize(*DESIGN_SIZE)
        self.setMinimumSize(*MINIMUM_SIZE)

        self._settings = QSettings("astrocam", "viewer")
        self._night = self._settings.value("night", False, type=bool)
        self._panel_collapsed = False
        self._active_tool = "camera"
        self._client_cache: CameraParamClient | None = None

        self._bridge = RosBridge()
        topic = self._settings.value("topic", DEFAULT_TOPIC, type=str)
        self._worker = RosWorker(self._bridge, topic)

        self._analysis_clock = QElapsedTimer()
        self._analysis_clock.start()
        self._last_analysis = -ANALYSIS_INTERVAL_MS
        self._fps_clock = QElapsedTimer()
        self._fps_clock.start()
        self._frame_count = 0

        self._build_ui()
        self._wire_signals()
        self._restore_state()

        self._preview.set_night(self._night)

        self._worker.start()

    def _build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        outer = QVBoxLayout(central)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        outer.addWidget(self._build_topbar())

        body = QWidget()
        body_layout = QHBoxLayout(body)
        body_layout.setContentsMargins(0, 0, 0, 0)
        body_layout.setSpacing(0)

        body_layout.addWidget(self._build_rail())

        self._preview = PreviewWidget()
        body_layout.addWidget(self._preview, 1)

        self._panel_stack = self._build_panel_stack()
        body_layout.addWidget(self._panel_stack)

        outer.addWidget(body, 1)

    def _build_topbar(self) -> QFrame:
        bar = QFrame()
        bar.setObjectName("TopBar")
        bar.setFixedHeight(TOPBAR_HEIGHT)
        layout = QHBoxLayout(bar)
        layout.setContentsMargins(12, 4, 12, 4)
        layout.setSpacing(14)

        brand = QLabel("AstroCam")
        brand.setObjectName("Brand")
        layout.addWidget(brand)

        self._status_label = QLabel("ROS: disconnected")
        self._status_label.setObjectName("StatLabel")
        layout.addWidget(self._status_label)

        self._temp_label = QLabel("T: -")
        self._temp_label.setObjectName("StatLabel")
        layout.addWidget(self._temp_label)

        self._fps_label = QLabel("0 fps")
        self._fps_label.setObjectName("StatLabel")
        layout.addWidget(self._fps_label)

        layout.addStretch(1)

        self._mode_btn = QPushButton("Standalone (Pi)")
        self._mode_btn.setObjectName("ToggleButton")
        self._mode_btn.setCheckable(True)
        self._mode_btn.clicked.connect(self._toggle_mode)
        layout.addWidget(self._mode_btn)

        self._night_btn = QPushButton("Night")
        self._night_btn.setObjectName("ToggleButton")
        self._night_btn.setCheckable(True)
        self._night_btn.setChecked(self._night)
        self._night_btn.clicked.connect(self._toggle_night)
        layout.addWidget(self._night_btn)

        self._fullscreen_btn = QPushButton("Fullscreen")
        self._fullscreen_btn.setObjectName("ToggleButton")
        self._fullscreen_btn.setCheckable(True)
        self._fullscreen_btn.clicked.connect(self._toggle_fullscreen)
        layout.addWidget(self._fullscreen_btn)

        return bar

    def _build_rail(self) -> QFrame:
        rail = QFrame()
        rail.setObjectName("IconRail")
        rail.setFixedWidth(RAIL_WIDTH)
        layout = QVBoxLayout(rail)
        layout.setContentsMargins(0, 6, 0, 6)
        layout.setSpacing(2)

        self._rail_buttons: dict[str, QPushButton] = {}
        for tool, label in (
            ("camera", "Camera"),
            ("analysis", "Analyze"),
            ("connect", "Connect"),
        ):
            btn = self._make_rail_button(label)
            btn.setCheckable(True)
            btn.clicked.connect(lambda _checked, t=tool: self._select_tool(t))
            layout.addWidget(btn)
            self._rail_buttons[tool] = btn

        layout.addStretch(1)

        self._moon_btn = self._make_rail_button("Moon")
        self._moon_btn.setEnabled(False)
        self._moon_btn.setToolTip("Go to Moon - coming soon")
        layout.addWidget(self._moon_btn)

        self._yolo_btn = self._make_rail_button("YOLO")
        self._yolo_btn.setEnabled(False)
        self._yolo_btn.setToolTip("Enable YOLO inference - coming soon")
        layout.addWidget(self._yolo_btn)

        self._rail_buttons["camera"].setChecked(True)
        return rail

    def _make_rail_button(self, text: str) -> QPushButton:
        btn = QPushButton(text)
        btn.setObjectName("RailButton")
        btn.setFixedWidth(RAIL_WIDTH)
        return btn

    def _build_panel_stack(self) -> QStackedWidget:
        stack = QStackedWidget()
        stack.setObjectName("PanelStack")
        stack.setFixedWidth(PANEL_WIDTH)

        self._camera_panel = CameraPanel()
        self._analysis_panel = AnalysisPanel()
        self._connect_panel = ConnectPanel()

        self._panel_index = {
            "camera": stack.addWidget(self._camera_panel),
            "analysis": stack.addWidget(self._analysis_panel),
            "connect": stack.addWidget(self._connect_panel),
        }
        return stack

    def _wire_signals(self) -> None:
        self._bridge.frame_ready.connect(self._on_frame, Qt.QueuedConnection)
        self._bridge.status.connect(self._on_status, Qt.QueuedConnection)
        self._bridge.error.connect(self._on_error, Qt.QueuedConnection)

        self._preview.capture_requested.connect(self._capture)
        self._camera_panel.settings_changed.connect(self._on_settings_changed)
        self._connect_panel.connection_changed.connect(self._on_connection_changed)

    def _restore_state(self) -> None:
        cam = self._settings.value("camera_values", None)
        if isinstance(cam, dict):
            self._camera_panel.restore(cam)
        topic = self._settings.value("topic", DEFAULT_TOPIC, type=str)
        node = self._settings.value("camera_node", DEFAULT_CAMERA_NODE, type=str)
        self._connect_panel.set_fields(topic, node)
        remote = self._settings.value("remote", False, type=bool)
        self._mode_btn.setChecked(remote)
        self._update_mode_label(remote)

    def _on_frame(self, frame: np.ndarray) -> None:
        display = frame
        if self._camera_panel.enhance_enabled():
            display = stretch_image(frame)
        self._preview.update_frame(display)
        self._tick_fps()
        self._maybe_run_analysis(display)

    def _tick_fps(self) -> None:
        self._frame_count += 1
        elapsed = self._fps_clock.elapsed()
        if elapsed >= 1000:
            fps = self._frame_count * 1000.0 / elapsed
            self._fps_label.setText(f"{fps:.0f} fps")
            self._frame_count = 0
            self._fps_clock.restart()

    def _maybe_run_analysis(self, frame: np.ndarray) -> None:
        now = self._analysis_clock.elapsed()
        if now - self._last_analysis < ANALYSIS_INTERVAL_MS:
            return
        self._last_analysis = now
        small = frame[::ANALYSIS_DOWNSAMPLE, ::ANALYSIS_DOWNSAMPLE]
        summary = frame_summary(small)
        histogram = histogram_rgb(small)
        self._preview.set_focus_score(summary["focus_score"])
        self._analysis_panel.update_analysis(summary, histogram)

    def _on_status(self, status: dict) -> None:
        connected = status.get("connected", False)
        color = "#4ade80" if connected else "#f87171"
        text = "connected" if connected else "waiting..."
        self._status_label.setText(f'<span style="color:{color}">o</span> ROS: {text}')
        temp = status.get("temperature_c")
        self._temp_label.setText(f"T: {temp:.1f} C" if temp is not None else "T: -")

    def _on_error(self, message: str) -> None:
        self._status_label.setText(f'<span style="color:#f87171">o</span> {message}')

    def _select_tool(self, tool: str) -> None:
        if tool == self._active_tool and not self._panel_collapsed:
            self._set_panel_collapsed(True)
            self._rail_buttons[tool].setChecked(False)
            return
        self._active_tool = tool
        self._set_panel_collapsed(False)
        self._panel_stack.setCurrentIndex(self._panel_index[tool])
        for name, btn in self._rail_buttons.items():
            btn.setChecked(name == tool)

    def _set_panel_collapsed(self, collapsed: bool) -> None:
        self._panel_collapsed = collapsed
        self._panel_stack.setVisible(not collapsed)

    def _toggle_fullscreen(self, fullscreen: bool | None = None) -> None:
        if fullscreen is None:
            fullscreen = not self.isFullScreen()
        if fullscreen:
            self.showFullScreen()
        else:
            self.showNormal()
        self._fullscreen_btn.setChecked(fullscreen)

    def keyPressEvent(self, event) -> None:  # noqa: N802
        if event.key() == Qt.Key_F11:
            self._toggle_fullscreen()
            return
        if event.key() == Qt.Key_Escape and self.isFullScreen():
            self._toggle_fullscreen(False)
            return
        super().keyPressEvent(event)

    def changeEvent(self, event) -> None:  # noqa: N802
        if event.type() == QEvent.WindowStateChange:
            self._fullscreen_btn.setChecked(self.isFullScreen())
        super().changeEvent(event)

    def _toggle_night(self) -> None:
        self._night = self._night_btn.isChecked()
        apply_theme(self._app(), self._night)
        self._preview.set_night(self._night)
        self._settings.setValue("night", self._night)

    def _toggle_mode(self) -> None:
        remote = self._mode_btn.isChecked()
        self._update_mode_label(remote)
        self._settings.setValue("remote", remote)

    def _update_mode_label(self, remote: bool) -> None:
        self._mode_btn.setText("Remote (Laptop)" if remote else "Standalone (Pi)")

    def _on_settings_changed(self, settings: dict) -> None:
        client = self._param_client()
        if client is not None:
            client.send(
                exposure_us=settings.get("exposure_us"),
                gain=settings.get("gain"),
                offset=settings.get("offset"),
            )
        self._settings.setValue("camera_values", self._camera_panel.values())

    def _on_connection_changed(self, topic: str, node: str) -> None:
        if topic and self._worker.node is not None:
            self._worker.node.change_topic(topic)
        client = self._param_client()
        if client is not None and node:
            client.set_camera_node(node)
        self._settings.setValue("topic", topic)
        self._settings.setValue("camera_node", node)

    def _capture(self) -> None:
        frame = self._preview.current_frame()
        if frame is None:
            return
        try:
            from PIL import Image

            IMAGES_DIR.mkdir(parents=True, exist_ok=True)
            stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
            path = IMAGES_DIR / f"capture_{stamp}.png"
            Image.fromarray(np.asarray(frame, dtype=np.uint8), mode="RGB").save(path)
            self._preview.flash_confirmation()
            self._status_label.setText(
                f'<span style="color:#4ade80">o</span> saved {path.name}'
            )
        except Exception as exc:
            self._on_error(f"capture failed: {exc}")

    def _param_client(self) -> CameraParamClient | None:
        if self._worker.node is None:
            return None
        if self._client_cache is None:
            node_name = self._connect_panel.node() or DEFAULT_CAMERA_NODE
            self._client_cache = CameraParamClient(self._worker.node, node_name)
        return self._client_cache

    def _app(self):
        from PyQt5.QtWidgets import QApplication

        return QApplication.instance()

    def shutdown_ros(self) -> None:
        try:
            self._worker.stop()
        except Exception:
            pass
        try:
            import rclpy

            if rclpy.ok():
                rclpy.shutdown()
        except Exception:
            pass

    def closeEvent(self, event) -> None:  # noqa: N802
        self.shutdown_ros()
        super().closeEvent(event)
