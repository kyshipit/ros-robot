#!/usr/bin/env python3
# navigation.launch.py —— mbot 的 Nav2 导航栈精简启动。
#
# 目标（阶段 3）：在 mbot_gazebo 仿真里跑通 "2D Goal Pose" 避障导航。
#
# 为什么自写精简 navigation（不复用官方 navigation_launch.py）：
#   官方 navigation_launch.py 硬编码了 velocity_smoother / collision_monitor /
#   route_server / docking_server 等节点，且把 controller 的 cmd_vel remap 成
#   cmd_vel_nav。这些节点我们没在 nav2_params.yaml 里配，会导致 lifecycle_manager
#   卡住。这里只启动我们 params 里配过的节点，controller 直接输出 cmd_vel，
#   顶层 remap 到 diff_drive_controller 的 ~/cmd_vel。
#
# 模式：
#   slam:=true  (默认) 在线建图（slam_toolbox，发布 map->odom）
#   slam:=false         已知地图定位（amcl + map_server，需提供 map 参数）
#
# 用法：
#   # 终端1：仿真
#   ros2 launch mbot_gazebo spawn.launch.py
#   # 终端2：导航（建图）
#   ros2 launch mbot_navigation navigation.launch.py
#   # 或定位模式
#   ros2 launch mbot_navigation navigation.launch.py slam:=false map:=/path/to/map.yaml

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition, UnlessCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    nav2_bringup = get_package_share_directory('nav2_bringup')
    mbot_nav = get_package_share_directory('mbot_navigation')

    use_sim_time = LaunchConfiguration('use_sim_time', default='true')
    autostart = LaunchConfiguration('autostart', default='true')
    slam = LaunchConfiguration('slam', default='true')
    map_yaml = LaunchConfiguration('map', default=os.path.join(mbot_nav, 'map', 'mbot_map.yaml'))
    params_file = os.path.join(mbot_nav, 'config', 'nav2_params.yaml')
    rviz_config = os.path.join(mbot_nav, 'rviz', 'mbot_navigation.rviz')

    # ---- SLAM（在线建图，slam:=true 时启动）----
    slam_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav2_bringup, 'launch', 'slam_launch.py')),
        condition=IfCondition(slam),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'params_file': os.path.join(mbot_nav, 'config', 'mapper_params_online_sync.yaml'),
            'autostart': autostart,
        }.items(),
    )

    # ---- 定位（已知地图，slam:=false 时启动）----
    localization_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav2_bringup, 'launch', 'localization_launch.py')),
        condition=UnlessCondition(slam),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'params_file': params_file,
            'map': map_yaml,
            'autostart': autostart,
        }.items(),
    )

    # ---- 精简导航栈节点 ----
    lifecycle_nodes = [
        'controller_server',
        'smoother_server',
        'planner_server',
        'behavior_server',
        'bt_navigator',
        'waypoint_follower',
    ]
    remappings = [('/tf', 'tf'), ('/tf_static', 'tf_static')]
    sim_time_param = {'use_sim_time': use_sim_time}

    controller_server = Node(
        package='nav2_controller',
        executable='controller_server',
        output='screen',
        parameters=[params_file, sim_time_param],
        # controller 输出 cmd_vel（TwistStamped，因 enable_stamped_cmd_vel=true），
        # 直接接到 diff_drive_controller 的 ~/cmd_vel（= /diff_drive_controller/cmd_vel）
        remappings=remappings + [('cmd_vel', '/diff_drive_controller/cmd_vel')],
    )
    smoother_server = Node(
        package='nav2_smoother',
        executable='smoother_server',
        output='screen',
        parameters=[params_file, sim_time_param],
        remappings=remappings,
    )
    planner_server = Node(
        package='nav2_planner',
        executable='planner_server',
        output='screen',
        parameters=[params_file, sim_time_param],
        remappings=remappings,
    )
    behavior_server = Node(
        package='nav2_behaviors',
        executable='behavior_server',
        output='screen',
        parameters=[params_file, sim_time_param],
        remappings=remappings + [('cmd_vel', '/diff_drive_controller/cmd_vel')],
    )
    bt_navigator = Node(
        package='nav2_bt_navigator',
        executable='bt_navigator',
        output='screen',
        parameters=[params_file, sim_time_param],
        remappings=remappings,
    )
    waypoint_follower = Node(
        package='nav2_waypoint_follower',
        executable='waypoint_follower',
        output='screen',
        parameters=[params_file, sim_time_param],
        remappings=remappings,
    )
    lifecycle_manager = Node(
        package='nav2_lifecycle_manager',
        executable='lifecycle_manager',
        name='lifecycle_manager_navigation',
        output='screen',
        parameters=[{'autostart': autostart}, {'node_names': lifecycle_nodes}, sim_time_param],
    )

    # ---- RViz ----
    rviz = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=['-d', rviz_config],
        parameters=[{'use_sim_time': use_sim_time}],
        output='screen',
    )

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='true'),
        DeclareLaunchArgument('autostart', default_value='true'),
        DeclareLaunchArgument('slam', default_value='true',
                              description='true=在线建图，false=已知地图定位(AMCL)'),
        DeclareLaunchArgument('map', default_value=os.path.join(mbot_nav, 'map', 'mbot_map.yaml'),
                              description='定位模式的地图 yaml 路径'),
        slam_launch,
        localization_launch,
        controller_server,
        smoother_server,
        planner_server,
        behavior_server,
        bt_navigator,
        waypoint_follower,
        lifecycle_manager,
        rviz,
    ])