from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import LaunchConfiguration
from launch.actions import DeclareLaunchArgument


def generate_launch_description():
    # Declare launch arguments
    connection_mode_arg = DeclareLaunchArgument(
        'connection_mode',
        default_value='serial',
        description='Connection mode: serial or wifi'
    )

    serial_port_arg = DeclareLaunchArgument(
        'serial_port',
        default_value='/dev/ttyUSB0',
        description='Serial port device'
    )

    baud_rate_arg = DeclareLaunchArgument(
        'baud_rate',
        default_value='115200',
        description='Serial baud rate'
    )

    wifi_ip_arg = DeclareLaunchArgument(
        'wifi_ip',
        default_value='192.168.4.1',
        description='WiFi IP address (ESP32 default AP IP)'
    )

    wifi_port_arg = DeclareLaunchArgument(
        'wifi_port',
        default_value='10001',
        description='WiFi TCP port'
    )

    debug_rate_arg = DeclareLaunchArgument(
        'debug_rate',
        default_value='1.0',
        description='Debug status print rate in Hz'
    )

    # Get launch configurations
    connection_mode = LaunchConfiguration('connection_mode')
    serial_port = LaunchConfiguration('serial_port')
    baud_rate = LaunchConfiguration('baud_rate')
    wifi_ip = LaunchConfiguration('wifi_ip')
    wifi_port = LaunchConfiguration('wifi_port')
    debug_rate = LaunchConfiguration('debug_rate')

    return LaunchDescription([
        connection_mode_arg,
        serial_port_arg,
        baud_rate_arg,
        wifi_ip_arg,
        wifi_port_arg,
        debug_rate_arg,

        Node(
            package='telescope_driver',
            executable='telescope_driver',
            name='telescope_driver',
            output='screen',
            parameters=[{
                'connection_mode': connection_mode,
                'serial_port': serial_port,
                'baud_rate': baud_rate,
                'wifi_ip': wifi_ip,
                'wifi_port': wifi_port,
                'debug_rate': debug_rate,
            }]
        )
    ])
