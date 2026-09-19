"""Application palettes: a standard dark theme and a night-vision red theme.

Night mode preserves dark adaptation at the telescope by rendering the whole UI in
red on black. ``apply_theme`` swaps the app-wide stylesheet; the preview widget
additionally red-tints the image (see ``preview_widget.set_night``).

All sizing is tuned for a 1024x600 7" touchscreen: large fonts, tall touch targets.
"""
from __future__ import annotations

# Shared sizing constants (also referenced by widgets).
RAIL_WIDTH = 76
PANEL_WIDTH = 280
TOPBAR_HEIGHT = 48
TOUCH_MIN = 44

# The floating capture button is a circle, so its diameter and corner radius must
# stay in lockstep. ``preview_widget`` imports CAPTURE_SIZE for setFixedSize and
# the stylesheet derives the radius from it — hardcoding both invites a mismatch
# that silently turns the circle into a rounded square.
CAPTURE_SIZE = 64

_BASE = """
* {{
    font-family: "DejaVu Sans", "Segoe UI", sans-serif;
    font-size: 15px;
    outline: none;
}}
QWidget {{
    background-color: {bg};
    color: {fg};
}}
QFrame#TopBar, QFrame#IconRail, QStackedWidget#PanelStack {{
    background-color: {panel};
}}
QLabel#Brand {{
    font-size: 18px;
    font-weight: bold;
    color: {accent};
}}
QLabel#PanelTitle {{
    font-size: 13px;
    font-weight: bold;
    color: {accent};
    padding: 6px 2px;
    border-bottom: 1px solid {border};
}}
QLabel#StatLabel {{
    font-size: 14px;
    color: {fg};
}}
QLabel#StatValue {{
    font-size: 16px;
    font-weight: bold;
    color: {accent};
}}
QPushButton {{
    background-color: {button};
    color: {fg};
    border: 1px solid {border};
    border-radius: 8px;
    padding: 8px;
    min-height: {touch}px;
}}
QPushButton:hover {{
    border-color: {accent};
}}
QPushButton:pressed {{
    background-color: {accent};
    color: {bg};
}}
QPushButton:disabled {{
    color: {muted};
    border-color: {border};
}}
QPushButton#RailButton {{
    border: none;
    border-radius: 0px;
    background-color: transparent;
    font-size: 12px;
    min-height: 68px;
}}
QPushButton#RailButton:hover {{
    background-color: {button};
}}
QPushButton#RailButton:checked {{
    background-color: {button};
    border-left: 4px solid {accent};
    color: {accent};
}}
/* Deliberately no min-width/min-height here. Qt adds padding and border to a
   stylesheet minimum and then lets that minimum override setFixedSize's maximum,
   so "min-width: 64px" plus the inherited 8px padding and 3px border pinned this
   button to 86px the moment a theme swap re-polished it - growing it and breaking
   the circle. Sizing is owned solely by setFixedSize(CAPTURE_SIZE) in
   preview_widget.resizeEvent. */
QPushButton#CaptureButton {{
    background-color: {accent};
    color: {bg};
    border: 3px solid {fg};
    border-radius: {capture_radius}px;
    padding: 0px;
    font-size: 24px;
    font-weight: bold;
}}
QPushButton#ToggleButton {{
    border-radius: 8px;
    padding: 6px 12px;
    min-height: 36px;
}}
QPushButton#ToggleButton:checked {{
    background-color: {accent};
    color: {bg};
}}
QCheckBox {{
    spacing: 10px;
    min-height: {touch}px;
}}
QCheckBox::indicator {{
    width: 26px;
    height: 26px;
    border: 2px solid {border};
    border-radius: 5px;
    background: {button};
}}
QCheckBox::indicator:checked {{
    background: {accent};
    border-color: {accent};
}}
QLineEdit {{
    background-color: {button};
    border: 1px solid {border};
    border-radius: 6px;
    padding: 8px;
    min-height: 36px;
    color: {fg};
}}
QLineEdit:focus {{
    border-color: {accent};
}}
QSlider::groove:horizontal {{
    height: 10px;
    background: {button};
    border-radius: 5px;
}}
QSlider::sub-page:horizontal {{
    background: {accent};
    border-radius: 5px;
}}
QSlider::handle:horizontal {{
    background: {fg};
    width: 30px;
    height: 30px;
    margin: -12px 0;
    border-radius: 15px;
}}
QScrollBar:vertical {{
    background: {panel};
    width: 12px;
}}
QScrollBar::handle:vertical {{
    background: {border};
    border-radius: 6px;
    min-height: 30px;
}}
"""

_DARK = {
    "bg": "#060a12",
    "panel": "#0c1120",
    "button": "#16203a",
    "border": "#27324f",
    "fg": "#e8eeff",
    "muted": "#5a6b88",
    "accent": "#38bdf8",
}

_NIGHT = {
    "bg": "#000000",
    "panel": "#0a0000",
    "button": "#240404",
    "border": "#511010",
    "fg": "#ff5b52",
    "muted": "#7a1f1f",
    "accent": "#ff3b30",
}

_SIZES = {"touch": TOUCH_MIN, "capture_radius": CAPTURE_SIZE // 2}

DARK_QSS = _BASE.format(**_SIZES, **_DARK)
NIGHT_QSS = _BASE.format(**_SIZES, **_NIGHT)


def apply_theme(app, night: bool) -> None:
    """Swap the application-wide stylesheet between dark and night-vision red."""
    app.setStyleSheet(NIGHT_QSS if night else DARK_QSS)
