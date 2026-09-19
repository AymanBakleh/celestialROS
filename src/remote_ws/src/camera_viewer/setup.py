from setuptools import find_packages, setup

package_name = 'camera_viewer'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools', 'opencv-python'],
    zip_safe=True,
    maintainer='user',
    maintainer_email='user@todo.todo',
    description='Remote camera viewer',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'viewer_node = camera_viewer.viewer_node:main'
        ],
    },
)
