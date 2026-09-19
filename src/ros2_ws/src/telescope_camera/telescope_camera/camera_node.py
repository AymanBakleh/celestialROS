#!/usr/bin/env python3
import glob
import os
import time

import cv2
import numpy as np
import rclpy
from cv_bridge import CvBridge
from rclpy.node import Node
from sensor_msgs.msg import Image

try:
    import zwoasi
except ImportError:
    zwoasi = None


class ZWOCameraNode(Node):
    def __init__(self):
        super().__init__('zwo_camera_node')

        self.declare_parameter('camera_type', 'zwo')
        self.declare_parameter('video_device', '/dev/video0')
        self.declare_parameter('framerate', 20.0)
        self.declare_parameter('image_width', 1280)
        self.declare_parameter('image_height', 720)
        self.declare_parameter('exposure_us', 500000)
        self.declare_parameter('gain', 100)
        self.declare_parameter('bandwidth', 50)
        self.declare_parameter('zwo_lib_path', '/usr/local/lib/libASICamera2.so')

        self.camera_type = self.get_parameter('camera_type').get_parameter_value().string_value
        self.video_device = os.environ.get('VIDEO_DEVICE') or self.get_parameter('video_device').get_parameter_value().string_value
        if not self.video_device or self.video_device == '/dev/video0' and not os.path.exists(self.video_device):
            candidates = [p for p in sorted(glob.glob('/dev/video*')) if os.path.exists(p)]
            if candidates:
                self.video_device = candidates[0]

        self.framerate = self.get_parameter('framerate').get_parameter_value().double_value
        self.image_width = self.get_parameter('image_width').get_parameter_value().integer_value
        self.image_height = self.get_parameter('image_height').get_parameter_value().integer_value
        self.exposure_us = self.get_parameter('exposure_us').get_parameter_value().integer_value
        self.gain = self.get_parameter('gain').get_parameter_value().integer_value
        self.bandwidth = self.get_parameter('bandwidth').get_parameter_value().integer_value
        self.zwo_lib_path = self.get_parameter('zwo_lib_path').get_parameter_value().string_value

        self.bridge = CvBridge()
        self.publisher = self.create_publisher(Image, '/camera/image_raw', 10)

        self.capture = None
        self.camera = None
        self.use_zwo = False

        if self.camera_type == 'zwo':
            self.use_zwo = self.init_zwo_camera()
        if not self.use_zwo:
            self.init_v4l2_camera()

        self.timer = self.create_timer(1.0 / max(self.framerate, 1.0), self.publish_frame)
        self.get_logger().info('Camera capture node started.')

    def init_zwo_camera(self):
        if zwoasi is None:
            self.get_logger().warning('zwoasi package not found; falling back to V4L2 capture.')
            return False

        if self.zwo_lib_path and os.path.exists(self.zwo_lib_path):
            self.get_logger().info(f'Initializing ZWO SDK from {self.zwo_lib_path}')
            zwo_lib_arg = self.zwo_lib_path
        else:
            self.get_logger().info('Initializing ZWO SDK from system library search path')
            zwo_lib_arg = None

        try:
            if zwo_lib_arg:
                zwoasi.init(zwo_lib_arg)
            else:
                zwoasi.init()
        except Exception as exc:
            self.get_logger().error(f'Failed to initialize ZWO SDK: {exc}')
            return False

        try:
            camera_count = zwoasi.get_num_cameras()
        except Exception as exc:
            self.get_logger().error(f'Failed to query ZWO cameras: {exc}')
            return False

        if camera_count <= 0:
            self.get_logger().error('No ZWO cameras detected. Falling back to V4L2 capture.')
            return False

        try:
            self.camera = zwoasi.Camera(0)
            self.camera_info = self.camera.get_camera_property()
            self.camera.set_control_value(zwoasi.ASI_BANDWIDTHOVERLOAD, int(self.bandwidth))
            self.camera.set_control_value(zwoasi.ASI_GAIN, int(self.gain))
            self.camera.set_control_value(zwoasi.ASI_EXPOSURE, int(self.exposure_us))
            self.camera.set_image_type(zwoasi.ASI_IMG_RGB24)
            self.camera.set_control_value(zwoasi.ASI_HIGH_SPEED_MODE, 0)
            time.sleep(0.5)
            self.get_logger().info('ZWO camera initialized successfully.')
            return True
        except Exception as exc:
            self.get_logger().error(f'Failed to initialize ZWO camera: {exc}')
            return False

    def init_v4l2_camera(self):
        self.capture = cv2.VideoCapture(self.video_device, cv2.CAP_V4L2)
        self.capture.set(cv2.CAP_PROP_FRAME_WIDTH, self.image_width)
        self.capture.set(cv2.CAP_PROP_FRAME_HEIGHT, self.image_height)
        self.capture.set(cv2.CAP_PROP_FPS, float(self.framerate))

        if not self.capture.isOpened():
            self.get_logger().error(f'Unable to open video device: {self.video_device}')
        else:
            self.get_logger().info(f'Opened fallback V4L2 device at {self.video_device}.')

    def capture_frame(self):
        if self.use_zwo and self.camera is not None:
            try:
                frame = self.camera.capture()
                if frame is None:
                    if self.capture is None:
                        self.get_logger().warning('ZWO capture returned no image, falling back to V4L2.')
                        self.use_zwo = False
                        self.camera = None
                        self.init_v4l2_camera()
                    return None
                if isinstance(frame, np.ndarray):
                    image = frame
                else:
                    image = np.frombuffer(frame, dtype=np.uint8)
                    expected_size = self.image_width * self.image_height * 3
                    if image.size != expected_size:
                        self.get_logger().warning(
                            f'Captured ZWO frame size mismatch: {image.size} != {expected_size}')
                        if self.capture is None:
                            self.get_logger().warning('Falling back to V4L2 due to ZWO frame mismatch.')
                            self.use_zwo = False
                            self.camera = None
                            self.init_v4l2_camera()
                        return None
                    image = image.reshape((self.image_height, self.image_width, 3))
                if image.ndim == 3:
                    return image
                if image.ndim == 2:
                    return cv2.cvtColor(image, cv2.COLOR_GRAY2RGB)
                self.get_logger().warning(f'Unexpected ZWO frame shape: {image.shape}')
                return None
            except Exception as exc:
                self.get_logger().error(f'Failed to capture ZWO frame: {exc}')
                if self.capture is None:
                    self.get_logger().warning('Falling back to V4L2 after ZWO capture error.')
                    self.use_zwo = False
                    self.camera = None
                    self.init_v4l2_camera()
                return None

        if self.capture is None:
            return None

        success, frame = self.capture.read()
        if not success or frame is None:
            self.get_logger().warning('Failed to read frame from fallback camera.')
            return None

        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        return rgb_frame

    def publish_frame(self):
        rgb_image = self.capture_frame()
        if rgb_image is None:
            return

        try:
            image_message = self.bridge.cv2_to_imgmsg(rgb_image, encoding='rgb8')
            image_message.header.stamp = self.get_clock().now().to_msg()
            self.publisher.publish(image_message)
            if not hasattr(self, '_first_frame_logged'):
                self.get_logger().info('Published first camera frame')
                self._first_frame_logged = True
        except Exception as exc:
            self.get_logger().error(f'Failed to publish camera frame: {exc}')


def main(args=None):
    rclpy.init(args=args)
    node = ZWOCameraNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
