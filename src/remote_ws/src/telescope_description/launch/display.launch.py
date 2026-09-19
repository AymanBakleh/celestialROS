import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import Command, LaunchConfiguration
from launch.actions import DeclareLaunchArgument
from launch_ros.parameter_descriptions import ParameterValue

def generate_launch_description():
    # Get package share directory
    package_name = 'telescope_description'
    pkg_share = get_package_share_directory(package_name)
    
    # URDF/Xacro file path
    urdf_file_name = 'telescope_description.urdf.xacro'
    urdf_path = os.path.join(pkg_share, 'urdf', urdf_file_name)
    
    # RViz config path
    rviz_config_path = os.path.join(pkg_share, 'config', 'telescope_description.rviz')
    
    # Launch configuration variables
    use_sim_time = LaunchConfiguration('use_sim_time', default='false')
    
    # Process the URDF file using xacro
    robot_description = ParameterValue(
        Command(['xacro ', urdf_path]),
        value_type=str
    )
    
    # Robot State Publisher Node
    robot_state_publisher_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{
            'use_sim_time': use_sim_time,
            'robot_description': robot_description
        }]
    )
    
    # Joint State Publisher GUI Node
    joint_state_publisher_gui_node = Node(
        package='joint_state_publisher_gui',
        executable='joint_state_publisher_gui',
        name='joint_state_publisher_gui',
        output='screen',
        parameters=[{
            'zeros': {
                'polar_align_joint': 0.584913,
                'ra_joint': 0.0,
                'dec_joint': 3.141592
            }
        }]
    )
    
    # RViz2 Node
    rviz2_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        output='screen',
        arguments=['-d', rviz_config_path]
    )
    
    # Create launch description
    ld = LaunchDescription()
    
    # Add actions
    ld.add_action(robot_state_publisher_node)
    ld.add_action(joint_state_publisher_gui_node)
    ld.add_action(rviz2_node)
    
    return ld