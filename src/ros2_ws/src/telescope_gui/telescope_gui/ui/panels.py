"""The three contextual panels shown in the right-hand QStackedWidget."""
from __future__ import annotations

from PyQt5.QtCore import Qt, QTimer, pyqtSignal
from PyQt5.QtWidgets import (
    QCheckBox,
    QGridLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QSlider,
    QVBoxLayout,
    QWidget,
)

from .histogram_widget import HistogramWidget
from ..ros_node import DEFAULT_CAMERA_NODE, DEFAULT_TOPIC

EXPOSURE_MIN = 10
EXPOSURE_MAX = 100000
EXPOSURE_DEFAULT = 1000


def _section_title(text: str) -> QLabel:
    label = QLabel(text)
    label.setObjectName("PanelTitle")
    return label


class CameraPanel(QWidget):
    settings_changed = pyqtSignal(dict)
    enhance_toggled = pyqtSignal(bool)

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(8)

        layout.addWidget(_section_title("CAMERA"))

        self._exposure_label = QLabel()
        self._exposure = self._make_slider(EXPOSURE_MIN, EXPOSURE_MAX, EXPOSURE_DEFAULT)
        layout.addWidget(self._exposure_label)
        layout.addWidget(self._exposure)

        self._gain_label = QLabel()
        self._gain = self._make_slider(0, 500, 120)
        layout.addWidget(self._gain_label)
        layout.addWidget(self._gain)

        self._offset_label = QLabel()
        self._offset = self._make_slider(0, 255, 10)
        layout.addWidget(self._offset_label)
        layout.addWidget(self._offset)

        self._enhance = QCheckBox("Auto Stretch")
        self._enhance.setChecked(True)
        self._enhance.toggled.connect(self.enhance_toggled)
        layout.addWidget(self._enhance)

        layout.addStretch(1)

        self._debounce = QTimer(self)
        self._debounce.setSingleShot(True)
        self._debounce.setInterval(250)
        self._debounce.timeout.connect(self._emit_settings)

        for slider in (self._exposure, self._gain, self._offset):
            slider.valueChanged.connect(self._on_slider_changed)

        self._update_labels()

    def _make_slider(self, lo: int, hi: int, value: int) -> QSlider:
        slider = QSlider(Qt.Horizontal)
        slider.setRange(lo, hi)
        slider.setValue(value)
        slider.setMinimumHeight(40)
        return slider

    def _on_slider_changed(self, _value: int) -> None:
        self._update_labels()
        self._debounce.start()

    def _update_labels(self) -> None:
        self._exposure_label.setText(f"Exposure: {self._exposure.value() / 100:.2f} ms")
        self._gain_label.setText(f"Gain: {self._gain.value()}")
        self._offset_label.setText(f"Offset: {self._offset.value()}")

    def _emit_settings(self) -> None:
        self.settings_changed.emit({
            "exposure_us": self._exposure.value() / 100.0 * 1000.0,
            "gain": self._gain.value(),
            "offset": self._offset.value(),
        })

    def enhance_enabled(self) -> bool:
        return self._enhance.isChecked()

    def values(self) -> dict:
        return {
            "exposure": self._exposure.value(),
            "gain": self._gain.value(),
            "offset": self._offset.value(),
            "enhance": self._enhance.isChecked(),
        }

    def restore(self, values: dict) -> None:
        if not values:
            return
        for slider, key in ((self._exposure, "exposure"),
                            (self._gain, "gain"), (self._offset, "offset")):
            if key in values:
                slider.blockSignals(True)
                slider.setValue(int(values[key]))
                slider.blockSignals(False)
        if "enhance" in values:
            self._enhance.setChecked(bool(values["enhance"]))
        self._update_labels()


class AnalysisPanel(QWidget):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(8)

        layout.addWidget(_section_title("ANALYSIS"))

        self.histogram = HistogramWidget()
        layout.addWidget(self.histogram)

        grid = QGridLayout()
        grid.setHorizontalSpacing(10)
        grid.setVerticalSpacing(6)
        self._values: dict[str, QLabel] = {}
        for row, key in enumerate(("Mean", "Median", "Max", "Min", "Focus")):
            name = QLabel(key)
            name.setObjectName("StatLabel")
            value = QLabel("-")
            value.setObjectName("StatValue")
            value.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
            grid.addWidget(name, row, 0)
            grid.addWidget(value, row, 1)
            self._values[key] = value
        layout.addLayout(grid)
        layout.addStretch(1)

    def update_analysis(self, summary: dict, histogram: dict) -> None:
        self.histogram.update_histogram(histogram)
        self._values["Mean"].setText(f"{summary['mean']:.1f}")
        self._values["Median"].setText(f"{summary['median']:.0f}")
        self._values["Max"].setText(f"{summary['max']}")
        self._values["Min"].setText(f"{summary['min']}")
        self._values["Focus"].setText(f"{summary['focus_score']:.1f}")


class ConnectPanel(QWidget):
    connection_changed = pyqtSignal(str, str)
    mode_changed = pyqtSignal(str)

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(8)

        layout.addWidget(_section_title("CONNECTION"))

        layout.addWidget(QLabel("Image topic"))
        self._topic = QLineEdit(DEFAULT_TOPIC)
        layout.addWidget(self._topic)

        layout.addWidget(QLabel("Camera node"))
        self._node = QLineEdit(DEFAULT_CAMERA_NODE)
        layout.addWidget(self._node)

        apply_btn = QPushButton("Apply Connection")
        apply_btn.clicked.connect(self._apply)
        layout.addWidget(apply_btn)

        layout.addStretch(1)

    def _apply(self) -> None:
        self.connection_changed.emit(self._topic.text().strip(),
                                     self._node.text().strip())

    def set_fields(self, topic: str, node: str) -> None:
        if topic:
            self._topic.setText(topic)
        if node:
            self._node.setText(node)

    def topic(self) -> str:
        return self._topic.text().strip()

    def node(self) -> str:
        return self._node.text().strip()
