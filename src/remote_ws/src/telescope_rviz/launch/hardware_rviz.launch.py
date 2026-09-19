import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch_ros.actions import Node
from launch.substitutions import Command
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    telescope_desc_pkg = get_package_share_directory('telescope_description')
    urdf_path = os.path.join(telescope_desc_pkg, 'urdf', 'telescope_description.urdf.xacro')
    rviz_config_path = os.path.join(telescope_desc_pkg, 'config', 'telescope_description.rviz')

    robot_description = ParameterValue(
        Command(['xacro ', urdf_path]),
        value_type=str,
    )

    telescope_rviz_pkg = get_package_share_directory('telescope_rviz')
    fixer_executable = os.path.join(
        os.path.dirname(os.path.dirname(telescope_rviz_pkg)),
        'bin',
        'joint_state_fixer',
    )

    joint_state_fixer_node = ExecuteProcess(
        cmd=[
            fixer_executable,
            '--ros-args',
            '-p', 'input_topic:=/telescope/hardware_joint_states',
            '-p', 'output_topic:=/telescope/hardware_joint_states_fixed',
            '-p', 'default_polar_align:=0.0',
        ],
        output='screen',
    )

    robot_state_publisher_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description}],
        remappings=[('/joint_states', '/telescope/hardware_joint_states_fixed')],
    )

    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        output='screen',
        arguments=['-d', rviz_config_path],
    )

    return LaunchDescription([
        joint_state_fixer_node,
        robot_state_publisher_node,
        rviz_node,
    ])
