#!/usr/bin/env python3
"""
Hardware launch without auto-starting any driver node or RViz.

- Uses hardware joint states directly for coordinate transforms.
- Starts Stellarium serial endpoints for hardware and fake RA/DEC streams.
- telescope_driver or fake_mount_simulator can be started manually by the user.

This launch intentionally does not start telescope_driver.
"""
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import ExecuteProcess
from launch.actions import TimerAction
from launch.substitutions import Command
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    telescope_desc_pkg = get_package_share_directory('telescope_description')
    goto_telescope_pkg = get_package_share_directory('goto_telescope')
    goto_params = os.path.join(goto_telescope_pkg, 'config', 'telescope_params.yaml')
    urdf_path = os.path.join(telescope_desc_pkg, 'urdf', 'telescope_description.urdf.xacro')
    robot_description = ParameterValue(Command(['xacro ', urdf_path]), value_type=str)

    return LaunchDescription([
        # Start the serial receiver first so /tmp/stellarium_pty exists before Stellarium opens it.

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
            executable='coordinate_transformer',
            name='coordinate_transformer',
            output='screen',
            parameters=[goto_params, {
                'joint_states_topic': '/telescope/hardware_joint_states',
                'state_topic': '/telescope/state_from_joints'
            }],
        ),

        Node(
            package='goto_telescope',
            executable='stellarium_serial_bridge_receiver_node',
            name='stellarium_serial_bridge_receiver',
            output='screen',
            parameters=[{
                'serial_port': '/tmp/stellarium_pty',
                'baud_rate': 9600,
                'use_pseudo_tty': True,
                'stellarium.coordinate_system': 'J2000',
                'joint_state_topic': '/telescope/hardware_joint_states',
                'state_topic': '/telescope/state_from_joints',
            }],
        ),

        TimerAction(
            period=2.0,
            actions=[ExecuteProcess(cmd=['stellarium'], output='screen', shell=True)],
        ),

        Node(
            package='goto_telescope',
            executable='stellarium_serial_bridge_receiver_node',
            name='stellarium_serial_bridge_fake',
            output='screen',
            parameters=[{
                'serial_port': '/tmp/fake_stellarium_pty',
                'baud_rate': 9600,
                'use_pseudo_tty': True,
                'stellarium.coordinate_system': 'J2000',
                'state_topic': '/telescope/fake_RaDec',
                'joint_state_topic': '',
            }],
        ),
    ])
