from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'telescope_gui'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.launch.py')),
    ],
    install_requires=[
        'setuptools',
        'requests',
        'opencv-python',
        'numpy',
        'PyQt5',
        'skyfield',
        'onnxruntime',
    ],
    zip_safe=True,
    maintainer='user',
    maintainer_email='user@todo.todo',
    description='Telescope control GUI with camera feed',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'gui_node = telescope_gui.gui_node:main',
        ],
    },
)
