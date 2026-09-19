import os

from launch import LaunchDescription
from launch.actions import SetEnvironmentVariable
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        SetEnvironmentVariable('QT_QPA_PLATFORM', os.environ.get('QT_QPA_PLATFORM', 'xcb')),
        SetEnvironmentVariable('DISPLAY', os.environ.get('DISPLAY', ':0')),
        Node(
            package='telescope_camera',
            executable='zwo_camera_node',
            name='zwo_camera',
            output='screen',
            parameters=[{
                'camera_type': 'zwo',
                'video_device': os.environ.get('VIDEO_DEVICE', '/dev/video0'),
                'framerate': 20.0,
                'image_width': 1280,
                'image_height': 720,
                'exposure_us': 500000,
                'gain': 100,
                'bandwidth': 50,
                'zwo_lib_path': os.environ.get('ZWO_LIB_PATH', '/usr/lib/aarch64-linux-gnu/libASICamera2.so'),
            }]
        ),
        Node(
            package='skyview',
            executable='skyview_node',
            name='skyview',
            output='screen',
        ),
        Node(
            package='telescope_driver',
            executable='telescope_driver',
            name='telescope_driver',
            output='screen',
            parameters=[{
                'connection_mode': 'serial',
                'serial_port': os.environ.get('MOUNT_SERIAL_PORT', '/dev/ttyUSB0'),
                'baud_rate': 115200,
                'debug_rate': 1.0,
            }]
        ),
        Node(
            package='telescope_gui',
            executable='gui_node',
            name='telescope_gui',
            output='screen'
        )
    ])
