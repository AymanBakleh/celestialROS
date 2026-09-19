from setuptools import setup
import os

package_name = 'fake_mount'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), ['launch/fake_mount.launch.py']),
        (os.path.join('share', package_name, 'config'), ['config/fake_mount.rviz']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='ayman',
    maintainer_email='ayman.bakleh98@gmail.com',
    description='Fake mount simulator and RViz for isolated testing',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'fake_mount_simulator = fake_mount.fake_mount_simulator:main',
            'fake_stellarium_serial_bridge_receiver = fake_mount.fake_stellarium_serial_bridge_receiver:main',
        ],
    },
)
