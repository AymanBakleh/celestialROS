"""RGB histogram widget drawn with QPainter."""
from __future__ import annotations

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QColor, QPainter
from PyQt5.QtWidgets import QWidget

_CHANNEL_COLORS = {
    "red": QColor(248, 113, 113, 150),
    "green": QColor(74, 222, 128, 150),
    "blue": QColor(96, 165, 250, 150),
}


class HistogramWidget(QWidget):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setMinimumHeight(120)
        self._hist: dict[str, list[int]] | None = None

    def update_histogram(self, hist: dict[str, list[int]] | None) -> None:
        self._hist = hist
        self.update()

    def paintEvent(self, event) -> None:  # noqa: N802
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor("#0c1120"))

        if not self._hist:
            painter.setPen(QColor("#5a6b88"))
            painter.drawText(self.rect(), Qt.AlignCenter, "no data")
            return

        width = self.width()
        height = self.height()
        bins = 256
        bin_w = width / bins

        peak = 1
        for channel in self._hist.values():
            if channel:
                peak = max(peak, max(channel))

        painter.setPen(Qt.NoPen)
        for name, color in _CHANNEL_COLORS.items():
            data = self._hist.get(name)
            if not data:
                continue
            painter.setBrush(color)
            for i, count in enumerate(data):
                if count <= 0:
                    continue
                bar_h = (count / peak) * (height - 2)
                x = i * bin_w
                painter.drawRect(
                    int(x), int(height - bar_h), max(1, int(bin_w) + 1), int(bar_h)
                )
