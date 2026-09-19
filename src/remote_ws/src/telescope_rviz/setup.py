from setuptools import setup
import os

package_name = 'telescope_rviz'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), ['launch/hardware_rviz.launch.py']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='ayman',
    maintainer_email='ayman.bakleh98@gmail.com',
    description='ROS2 package for visualizing telescope hardware joint states in RViz',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'joint_state_fixer = telescope_rviz.joint_state_fixer:main',
        ],
    },
)
