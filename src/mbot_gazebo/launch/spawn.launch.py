#!/usr/bin/env python3
# spawn.launch.py —— 起 Gazebo Harmonic + spawn mbot + 加载 ros2_control 控制器。
#
# 结构对齐官方 gz_ros2_control_demos/diff_drive_example.launch.py：
#   - controller_manager 由 gazebo 内 gz_ros2_control 插件自动创建，不单独起；
#   - 用 OnProcessExit 事件链：spawn entity → joint_state_broadcaster → diff_drive_controller；
#   - ros_gz_bridge 桥接 /clock（use_sim_time）、/scan、/imu。
#
# 闭环目标（阶段 2 验证）：
#   发 /cmd_vel → diff_drive_controller 解算 → 轮子转 → /odom + odom→base_footprint TF。
#
# 用法：ros2 launch mbot_gazebo spawn.launch.py

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, RegisterEventHandler
from launch.event_handlers import OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    pkg_gazebo = get_package_share_directory('mbot_gazebo')
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')

    use_sim_time = LaunchConfiguration('use_sim_time', default='true')

    # ---- robot_state_publisher：发布 robot_description（含 <ros2_control> 标签） ----
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': Command(
            ['xacro ', os.path.join(pkg_gazebo, 'urdf', 'mbot_gazebo.xacro')])}],
        # use_sim_time 由 /clock bridge 驱动
    )

    # ---- spawn 机器人到 gazebo ----
    gz_spawn_entity = Node(
        package='ros_gz_sim',
        executable='create',
        output='screen',
        arguments=['-topic', 'robot_description',
                   '-name', 'mbot',
                   '-x', '0.0', '-y', '0.0', '-z', '0.1',
                   '-allow_renaming', 'true'],
    )

    # ---- 控制器 spawner（参数从 yaml 读） ----
    robot_controllers = os.path.join(pkg_gazebo, 'config', 'ros2_control.yaml')

    joint_state_broadcaster_spawner = Node(
        package='controller_manager',
        executable='spawner',
        arguments=['joint_state_broadcaster', '--param-file', robot_controllers],
        output='screen',
    )

    diff_drive_controller_spawner = Node(
        package='controller_manager',
        executable='spawner',
        arguments=['diff_drive_controller', '--param-file', robot_controllers],
        output='screen',
    )

    # ---- ros_gz_bridge：clock（use_sim_time）、scan、imu ----
    clock_bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=['/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock'],
        output='screen',
    )
    laser_bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=['/scan@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan'],
        output='screen',
    )
    imu_bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=['/imu@sensor_msgs/msg/Imu[gz.msgs.IMU'],
        output='screen',
    )

    return LaunchDescription([
        # gazebo 环境
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')),
            launch_arguments=[('gz_args', [' -r -v 1 ', os.path.join(pkg_gazebo, 'world', 'empty.world')])],
        ),
        # spawn → joint_state_broadcaster → diff_drive_controller（事件链）
        RegisterEventHandler(
            event_handler=OnProcessExit(
                target_action=gz_spawn_entity,
                on_exit=[joint_state_broadcaster_spawner],
            )
        ),
        RegisterEventHandler(
            event_handler=OnProcessExit(
                target_action=joint_state_broadcaster_spawner,
                on_exit=[diff_drive_controller_spawner],
            )
        ),
        robot_state_publisher,
        gz_spawn_entity,
        clock_bridge,
        laser_bridge,
        imu_bridge,
        DeclareLaunchArgument('use_sim_time', default_value='true',
                              description='使用仿真时钟'),
    ])