"""Live preview widget: scaled image + focus overlay + floating capture button."""
from __future__ import annotations

import numpy as np
from PyQt5.QtCore import Qt, pyqtSignal
from PyQt5.QtGui import QColor, QFont, QImage, QPainter, QPixmap
from PyQt5.QtWidgets import QPushButton, QWidget

from .theme import CAPTURE_SIZE


class PreviewWidget(QWidget):
    capture_requested = pyqtSignal()

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setMinimumSize(320, 240)

        self._buf: np.ndarray | None = None
        self._frame: np.ndarray | None = None
        self._pixmap: QPixmap | None = None
        self._focus: float | None = None
        self._night = False
        self._flash = 0

        self._capture_btn = QPushButton("◉", self)
        self._capture_btn.setObjectName("CaptureButton")
        self._capture_btn.setToolTip("Capture frame (save PNG)")
        self._capture_btn.clicked.connect(self.capture_requested)

    def update_frame(self, frame: np.ndarray) -> None:
        self._frame = frame
        self._buf = np.ascontiguousarray(frame, dtype=np.uint8)
        h, w = self._buf.shape[:2]
        image = QImage(self._buf.data, w, h, 3 * w, QImage.Format_RGB888)
        self._pixmap = QPixmap.fromImage(image)
        self.update()

    def set_focus_score(self, score: float | None) -> None:
        self._focus = score
        self.update()

    def set_night(self, night: bool) -> None:
        self._night = night
        self._layout_capture_button()
        self.update()

    def current_frame(self) -> np.ndarray | None:
        return self._frame

    def flash_confirmation(self) -> None:
        self._flash = 8
        self.update()

    def resizeEvent(self, event) -> None:  # noqa: N802
        self._layout_capture_button()
        super().resizeEvent(event)

    def _layout_capture_button(self) -> None:
        size = CAPTURE_SIZE
        margin = 18
        self._capture_btn.setFixedSize(size, size)
        self._capture_btn.move(
            (self.width() - size) // 2, self.height() - size - margin
        )

    def paintEvent(self, event) -> None:  # noqa: N802
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor("#000000"))

        if self._pixmap is not None:
            scaled = self._pixmap.scaled(
                self.size(), Qt.KeepAspectRatio, Qt.FastTransformation
            )
            x = (self.width() - scaled.width()) // 2
            y = (self.height() - scaled.height()) // 2
            painter.drawPixmap(x, y, scaled)

            if self._night:
                painter.setCompositionMode(QPainter.CompositionMode_Multiply)
                painter.fillRect(x, y, scaled.width(), scaled.height(),
                                 QColor(255, 40, 40))
                painter.setCompositionMode(QPainter.CompositionMode_SourceOver)
        else:
            painter.setPen(QColor("#5a6b88"))
            painter.setFont(QFont("DejaVu Sans", 16))
            painter.drawText(self.rect(), Qt.AlignCenter,
                             "Waiting for ROS2 camera frames...")

        if self._focus is not None:
            accent = QColor("#ff3b30") if self._night else QColor("#38bdf8")
            painter.setFont(QFont("DejaVu Sans", 14, QFont.Bold))
            painter.setPen(QColor(0, 0, 0, 160))
            painter.drawText(13, 27, f"Focus: {self._focus:.1f}")
            painter.setPen(accent)
            painter.drawText(12, 26, f"Focus: {self._focus:.1f}")

        if self._flash > 0:
            alpha = int(180 * self._flash / 8)
            pen_color = QColor(255, 255, 255, alpha)
            painter.setPen(pen_color)
            for inset in (1, 2, 3):
                painter.drawRect(inset, inset,
                                 self.width() - 2 * inset, self.height() - 2 * inset)
            self._flash -= 1
            if self._flash > 0:
                self.update()
