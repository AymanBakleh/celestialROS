from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'goto_telescope'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.launch.py')),
        (os.path.join('share', package_name, 'config'), glob('config/*')),
        (os.path.join('share', package_name, 'msg'), glob('goto_telescope/msg/*.msg')),
    ],
    install_requires=['setuptools', 'requests'],
    zip_safe=True,
    maintainer='Ayman Bakleh',
    maintainer_email='ayman.bakleh98@gmail.com',
    description='Telescope control system with Stellarium integration',
    license='Apache-2.0',
    extras_require={
        'test': ['pytest'],
    },
    entry_points={
        'console_scripts': [
            'stellarium_bridge = goto_telescope.nodes.stellarium_bridge:main',
            'stellarium_serial_bridge = goto_telescope.nodes.stellarium_serial_bridge:main',
            'coordinate_transformer = goto_telescope.nodes.coordinate_transformer:main',
            'telescope_joint_bridge = goto_telescope.nodes.telescope_joint_bridge:main',
            'joint_state_merger = goto_telescope.nodes.joint_state_merger:main',
            'stellarium_initialization = goto_telescope.nodes.stellarium_initialization:main',
            'telescope_speed_gui = goto_telescope.nodes.telescope_speed_gui:main',
            'stellarium_serial_bridge_sender_node = goto_telescope.nodes.stellarium_serial_bridge_sender_node:main',
            'stellarium_serial_bridge_receiver_node = goto_telescope.nodes.stellarium_serial_bridge_receiver_node:main',
        ],
    },
)