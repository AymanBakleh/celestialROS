#!/usr/bin/env python3
"""
Simulation launch (RViz + fake mount) without spawning a Stellarium process.

Use this launch when you want simulation logic and visualization only.
It intentionally does not call `stellarium` so no extra window is opened.
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
            parameters=[{'robot_description': robot_description, 'use_sim_time': False}],
            remappings=[('/joint_states', '/telescope/fake_joint_states')],
        ),

        Node(
            package='goto_telescope',
            executable='fake_mount_simulator',
            name='fake_mount_simulator',
            output='screen',
            parameters=[{
                'apply_pier_side_ra180': True,
                'firmware_emulation': True,
                'visual_ra_inverted': True,
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
