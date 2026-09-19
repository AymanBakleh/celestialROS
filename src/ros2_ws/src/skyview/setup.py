from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'skyview'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=[
        'setuptools',
    ],
    zip_safe=True,
    maintainer='user',
    maintainer_email='user@todo.todo',
    description='Skyview target routing node for telescope control',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'skyview_node = skyview.skyview_node:main',
        ],
    },
)
