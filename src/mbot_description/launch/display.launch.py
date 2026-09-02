#!/usr/bin/env python3
# display.launch.py —— 仅显示模型与 TF 树，验证阶段 1 的坐标系。
#
# 依赖：robot_state_publisher + rviz2 + xacro（都已安装）。
# 无 joint_state_publisher：改用一个一次性发布静态 joint_states 的最小节点
# （左右轮置零），避免占 topic 与后续 ros2_control 的 joint_state_broadcaster 冲突。
#
# 用法：ros2 launch mbot_description display.launch.py

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, Command
from launch_ros.actions import Node


def generate_launch_description():
    pkg = get_package_share_directory('mbot_description')
    urdf_path = os.path.join(pkg, 'urdf', 'mbot.xacro')

    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        parameters=[{'robot_description': Command(['xacro ', urdf_path])}],
    )

    # 静态 joint_states：左右轮关节置零。real robot 阶段由 ros2_control 广播器接管。
    fake_joint_states = Node(
        package='mbot_description',
        executable='fake_joint_states',
        name='fake_joint_states',
    )

    rviz = Node(
        package='rviz2',
        executable='rviz2',
        arguments=['-d', os.path.join(pkg, 'rviz', 'mbot.rviz')],
    )

    return LaunchDescription([
        robot_state_publisher,
        fake_joint_states,
        rviz,
    ])