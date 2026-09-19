import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

def generate_launch_description():
    pkg_goto_telescope = get_package_share_directory('goto_telescope')
    
    # We include the standard tracking launch which brings up RViz, fake mount, and the Stellarium bridges.
    # It bridges Stellarium on port 10001 to fake mount state.
    # We can also start a second bridge or just rely on standard ROS DDS routing for /telescope/state vs /telescope/fake_state.
    
    return LaunchDescription([
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(pkg_goto_telescope, 'launch', 'telescope_tracking.launch.py'))
        ),
        
        # Start the camera viewer specifically for remote mode
        Node(
            package='camera_viewer',
            executable='viewer_node',
            name='remote_camera_viewer',
            output='screen'
        )
    ])
