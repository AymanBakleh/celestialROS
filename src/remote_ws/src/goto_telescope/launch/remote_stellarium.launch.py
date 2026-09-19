import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    pkg_goto_telescope = get_package_share_directory('goto_telescope')
    
    # Stellarium only mode means we don't start RViz, fake mount, or TF trees.
    # We ONLY start the Stellarium bridges so that Stellarium on the laptop can:
    # 1. Listen to the physical hardware state from Raspberry Pi (/telescope/state)
    # 2. Publish goto targets to physical hardware on Raspberry Pi (/stellarium/target)
    
    return LaunchDescription([
        Node(
            package='goto_telescope',
            executable='stellarium_bridge',
            name='stellarium_bridge',
            output='screen',
            parameters=[os.path.join(pkg_goto_telescope, 'config', 'telescope_params.yaml')]
        ),
        
        Node(
            package='camera_viewer',
            executable='viewer_node',
            name='remote_camera_viewer',
            output='screen'
        )
    ])
