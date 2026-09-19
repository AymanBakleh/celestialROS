import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
import subprocess
import re


def generate_launch_description():
    telescope_desc_pkg = get_package_share_directory('telescope_description')
    rviz_config_file = get_package_share_directory('fake_mount')
    rviz_config_file = os.path.join(rviz_config_file, 'config', 'fake_mount.rviz')
    urdf_path = os.path.join(telescope_desc_pkg, 'urdf', 'telescope_description.urdf.xacro')

    # Generate a prefixed robot_description so frames/joints do not conflict with hardware.
    # Run xacro here and apply safe regex replacements in Python to prefix names with 'fake_'.
    try:
        xacro_out = subprocess.run(['xacro', urdf_path], check=True, capture_output=True, text=True)
        urdf_text = xacro_out.stdout
        # prefix link names
        urdf_text = re.sub(r'(<link\s+name=")([^"]+)(")', r'\1fake_\2\3', urdf_text)
        # prefix joint names
        urdf_text = re.sub(r'(<joint\s+name=")([^"]+)(")', r'\1fake_\2\3', urdf_text)
        # prefix parent/child link references
        urdf_text = re.sub(r'(<parent\s+link=")([^"]+)(")', r'\1fake_\2\3', urdf_text)
        urdf_text = re.sub(r'(<child\s+link=")([^"]+)(")', r'\1fake_\2\3', urdf_text)
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to run xacro: {e.stderr}")

    robot_description = ParameterValue(urdf_text, value_type=str)

    # Robot state publisher listens to the fake joint states topic
    robot_state_publisher_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher_fake',
        output='screen',
        parameters=[{'robot_description': robot_description, 'use_sim_time': False}],
        remappings=[('/joint_states', '/fake_mount/fake_joint_states')],
    )

    fake_mount_pkg = get_package_share_directory('fake_mount')
    fake_exec = os.path.join(
        os.path.dirname(os.path.dirname(fake_mount_pkg)),
        'bin',
        'fake_mount_simulator',
    )

    fake_sim_node = ExecuteProcess(
        cmd=[fake_exec],
        output='screen',
    )

    fake_bridge_exec = os.path.join(
        os.path.dirname(os.path.dirname(fake_mount_pkg)),
        'bin',
        'fake_stellarium_serial_bridge_receiver',
    )
    fake_bridge_node = ExecuteProcess(
        cmd=[fake_bridge_exec],
        output='screen',
    )

    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2_fake',
        output='screen',
        arguments=['-d', rviz_config_file],
    )

    return LaunchDescription([
        fake_sim_node,
        fake_bridge_node,
        robot_state_publisher_node,
        rviz_node,
    ])
