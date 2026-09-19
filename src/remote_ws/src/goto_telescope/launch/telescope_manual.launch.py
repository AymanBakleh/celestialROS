#!/usr/bin/env python3
"""Manual mode: User controls telescope via joint_state_publisher_gui
- No automatic tracking
- Optional Stellarium integration via bridge nodes (no app auto-spawn)
- User manually moves telescope and sees it in RViz
"""
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import Command
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    telescope_desc_pkg = get_package_share_directory('telescope_description')
    rviz_config_file = os.path.join(telescope_desc_pkg, 'config', 'telescope_description.rviz')
    urdf_path = os.path.join(telescope_desc_pkg, 'urdf', 'telescope_description.urdf.xacro')

    robot_description = ParameterValue(Command(['xacro ', urdf_path]), value_type=str)

    return LaunchDescription([
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            remappings=[('/joint_states', '/joint_states')],
            parameters=[{'robot_description': robot_description, 'use_sim_time': False}],
        ),

        # Manual mode: GUI joint control
        Node(
            package='joint_state_publisher_gui',
            executable='joint_state_publisher_gui',
            name='joint_state_publisher_gui',
            output='screen',
        ),

        Node(
            package='goto_telescope',
            executable='coordinate_transformer',
            name='coordinate_transformer',
            output='screen',
            parameters=[{
                'location.lat': 33.5138,
                'location.lon': 36.2765,
                'joint_states_topic': '/joint_states',
            }],
        ),

        Node(
            package='goto_telescope',
            executable='stellarium_bridge',
            name='stellarium_bridge',
            output='screen',
            parameters=[{
                'mode': 'simulation',
                'stellarium.remote_control.host': 'localhost',
                'stellarium.remote_control.port': 8090,
                'stellarium.use_telescope_control': False,
                'stellarium.coordinate_system': 'J2000',
                'stellarium.connection_delay': 0.5,
            }],
        ),

        Node(
            package='goto_telescope',
            executable='stellarium_initialization',
            name='stellarium_initialization',
            output='screen',
            parameters=[{
                'stellarium.remote_control.host': 'localhost',
                'stellarium.remote_control.port': 8090,
                'location.lat': 33.5138,
                'location.lon': 36.2765,
                'stellarium.start_target': 'polaris',
            }],
        ),

        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            output='screen',
            arguments=['-d', rviz_config_file],
        ),
    ])
