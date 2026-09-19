#!/usr/bin/env python3
import sys
import os
os.environ["LIBGL_ALWAYS_SOFTWARE"] = "1"
os.environ["QT_QPA_PLATFORM"] = os.environ.get("QT_QPA_PLATFORM", "xcb")
os.environ.setdefault("DISPLAY", ":0")

import math
from datetime import datetime
from zoneinfo import ZoneInfo

import numpy as np
import cv2
from skyfield.api import load, wgs84

# TODO: Change this block later if you move to a different city/country.
# Current observing site: Damascus, Syria
OBSERVATORY_LAT_DEG = 33.5138
OBSERVATORY_LON_DEG = 36.2765
OBSERVATORY_ELEV_M = 680.0
OBSERVATORY_TIMEZONE = "Asia/Damascus"  # Change to another IANA timezone if the location changes.

from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                             QHBoxLayout, QPushButton, QLabel, QRadioButton,
                             QMessageBox, QGroupBox, QCheckBox)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
from PyQt5.QtGui import QImage, QPixmap

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from geometry_msgs.msg import Vector3
from cv_bridge import CvBridge


class ROSThread(QThread):
    image_signal = pyqtSignal(QImage)

    def __init__(self, node):
        super().__init__()
        self.node = node

    def run(self):
        rclpy.spin(self.node)


class TelescopeGUINode(Node):
    def __init__(self, image_signal):
        super().__init__('telescope_gui_node')
        self.image_signal = image_signal

        self.bridge = CvBridge()
        self.image_sub = self.create_subscription(
            Image,
            '/camera/image_raw',
            self.image_callback,
            10)

        self.target_pub = self.create_publisher(Vector3, '/skyview/request', 10)

        self.inference_enabled = False
        self.model_loaded = False
        self.model = None
        self.model_input_name = None
        self.model_input_shape = None

        self.get_logger().info("GUI Node initialized.")

    def image_callback(self, msg):
        try:
            cv_image = self.bridge.imgmsg_to_cv2(msg, "rgb8")
            cv_image = self.process_frame(cv_image)
            h, w, ch = cv_image.shape
            bytes_per_line = ch * w
            qt_image = QImage(cv_image.data, w, h, bytes_per_line, QImage.Format_RGB888).copy()
            self.image_signal.emit(qt_image)
        except Exception as e:
            self.get_logger().error(f"Error converting image: {e}")

    def process_frame(self, cv_image):
        if self.inference_enabled:
            if not self.model_loaded:
                self.load_model()
            if self.model_loaded:
                detections = self.run_inference(cv_image)
                if detections:
                    cv_image = self.draw_detections(cv_image, detections)
        return cv_image

    def load_model(self, model_path=None):
        if self.model_loaded:
            return True

        if model_path is None:
            model_path = os.environ.get('YOLO_MODEL_PATH', os.path.join(os.getcwd(), 'yolov11n.onnx'))

        if not os.path.exists(model_path):
            self.get_logger().warning(
                f"YOLO model not found at {model_path}. Put your ONNX model there or set YOLO_MODEL_PATH.")
            return False

        try:
            import onnxruntime as ort
            self.model = ort.InferenceSession(model_path, providers=['CPUExecutionProvider'])
            self.model_input_name = self.model.get_inputs()[0].name
            self.model_input_shape = tuple(self.model.get_inputs()[0].shape)
            self.model_loaded = True
            self.get_logger().info(f"Loaded YOLO model from {model_path}")
            return True
        except Exception as e:
            self.get_logger().error(f"Failed to load YOLO model: {e}")
            self.model_loaded = False
            return False

    def run_inference(self, cv_image):
        if self.model is None:
            return []

        try:
            height, width = cv_image.shape[:2]
            input_shape = self.model_input_shape
            if len(input_shape) < 4 or input_shape[2] is None or input_shape[3] is None:
                return []

            target_w = int(input_shape[3])
            target_h = int(input_shape[2])
            resized = cv2.resize(cv_image, (target_w, target_h))
            model_input = resized.astype(np.float32) / 255.0
            model_input = np.transpose(model_input, (2, 0, 1))[None, ...]

            raw_outputs = self.model.run(None, {self.model_input_name: model_input})
            if not raw_outputs:
                return []

            output = raw_outputs[0]
            if output.ndim == 3 and output.shape[2] >= 5:
                detections = []
                for det in output[0]:
                    score = float(det[4])
                    if score < 0.35:
                        continue
                    cx, cy, w_box, h_box = det[0:4]
                    cls = int(det[5]) if det.shape[0] > 5 else 0
                    x1 = int((cx - w_box / 2) * width / target_w)
                    y1 = int((cy - h_box / 2) * height / target_h)
                    x2 = int((cx + w_box / 2) * width / target_w)
                    y2 = int((cy + h_box / 2) * height / target_h)
                    detections.append((x1, y1, x2, y2, score, cls))
                return detections

            return []
        except Exception as e:
            self.get_logger().error(f"Inference failed: {e}")
            return []

    def draw_detections(self, cv_image, detections):
        for x1, y1, x2, y2, score, cls in detections:
            label = f"{cls}:{score:.2f}"
            color = (0, 255, 0)
            cv2.rectangle(cv_image, (x1, y1), (x2, y2), color, 2)
            cv2.putText(cv_image, label, (x1, max(0, y1 - 8)), cv2.FONT_HERSHEY_SIMPLEX,
                        0.5, color, 1, cv2.LINE_AA)
        return cv_image

    def publish_target(self, ra, dec):
        msg = Vector3()
        msg.x = float(ra)
        msg.y = float(dec)
        msg.z = 0.0
        self.target_pub.publish(msg)
        self.get_logger().info(f"Published skyview request RA: {ra}, Dec: {dec}")


class MainWindow(QMainWindow):
    def __init__(self, ros_node):
        super().__init__()
        self.ros_node = ros_node
        self.setWindowTitle("Telescope Remote Control")

        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.layout = QVBoxLayout(self.central_widget)

        self.mode_group = QGroupBox("Mode Selection")
        self.mode_layout = QHBoxLayout()
        self.btn_remote = QRadioButton("Remote Control (Laptop)")
        self.btn_standalone = QRadioButton("Stand Alone (Raspberry Pi)")
        self.btn_standalone.setChecked(True)
        self.mode_layout.addWidget(self.btn_remote)
        self.mode_layout.addWidget(self.btn_standalone)
        self.mode_group.setLayout(self.mode_layout)
        self.layout.addWidget(self.mode_group)

        self.btn_remote.toggled.connect(self.on_mode_changed)

        self.action_layout = QHBoxLayout()
        self.btn_moon = QPushButton("Go To Moon")
        self.btn_moon.clicked.connect(self.go_to_moon)
        self.action_layout.addWidget(self.btn_moon)

        self.chk_inference = QCheckBox("Enable YOLO Inference")
        self.chk_inference.stateChanged.connect(self.on_inference_toggled)
        self.action_layout.addWidget(self.chk_inference)

        self.btn_load_model = QPushButton("Load Model")
        self.btn_load_model.clicked.connect(self.on_load_model)
        self.action_layout.addWidget(self.btn_load_model)

        self.layout.addLayout(self.action_layout)

        self.status_label = QLabel("Inference disabled. Load a model to enable detections.")
        self.layout.addWidget(self.status_label)

        self.camera_label = QLabel("Waiting for camera feed...")
        self.camera_label.setAlignment(Qt.AlignCenter)
        self.camera_label.setMinimumSize(640, 480)
        self.camera_label.setStyleSheet("border: 1px solid black;")
        self.layout.addWidget(self.camera_label)

        self.resize(800, 660)

    def update_image(self, qt_image):
        pixmap = QPixmap.fromImage(qt_image)
        self.camera_label.setPixmap(pixmap.scaled(self.camera_label.size(), Qt.KeepAspectRatio))

    def on_mode_changed(self):
        is_standalone = self.btn_standalone.isChecked()
        self.btn_moon.setEnabled(is_standalone)

    def on_inference_toggled(self, state):
        enabled = state == Qt.Checked
        self.ros_node.inference_enabled = enabled
        if enabled:
            if self.ros_node.model_loaded or self.ros_node.load_model():
                self.status_label.setText("Inference enabled. Detections will appear on the live feed.")
            else:
                self.status_label.setText("Inference enabled, but no model is loaded.")
        else:
            self.status_label.setText("Inference disabled.")

    def on_load_model(self):
        if self.ros_node.load_model():
            self.status_label.setText("Model loaded successfully. Enable inference to see detections.")
        else:
            self.status_label.setText("Could not load model. Place yolov11n.onnx in the current folder or set YOLO_MODEL_PATH.")

    def go_to_moon(self):
        try:
            # TODO: Change the observatory constants above if you later switch country/city.
            ts = load.timescale()
            local_dt = datetime.now(ZoneInfo(OBSERVATORY_TIMEZONE))
            t = ts.from_datetime(local_dt)
            eph = load('de421.bsp')
            earth, moon = eph['earth'], eph['moon']

            # Use the Damascus, Syria observer location for the moon target.
            observer = earth + wgs84.latlon(
                OBSERVATORY_LAT_DEG,
                OBSERVATORY_LON_DEG,
                OBSERVATORY_ELEV_M,
            )
            astrometric = observer.at(t).observe(moon)
            ra, dec, distance = astrometric.apparent().radec()
            ra_hours = ra.hours
            dec_deg = dec.degrees
            self.ros_node.publish_target(ra_hours, dec_deg)
            QMessageBox.information(self, "Success", f"Sent Moon target:\nRA: {ra_hours:.4f}h\nDec: {dec_deg:.4f}°")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to compute Moon coordinates.\n{e}")


def main(args=None):
    rclpy.init(args=args)

    app = QApplication(sys.argv)

    class SignalWrapper(QWidget):
        image_signal = pyqtSignal(QImage)

    signal_wrapper = SignalWrapper()

    node = TelescopeGUINode(signal_wrapper.image_signal)
    window = MainWindow(node)
    signal_wrapper.image_signal.connect(window.update_image)

    ros_thread = ROSThread(node)
    ros_thread.start()

    window.show()
    print("Window should be open now!", flush=True)
    exit_code = app.exec_()

    node.destroy_node()
    rclpy.shutdown()
    ros_thread.wait()
    sys.exit(exit_code)

if __name__ == '__main__':
    main()
