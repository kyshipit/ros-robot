import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument,IncludeLaunchDescription
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
from launch.launch_description_sources import PythonLaunchDescriptionSource


def generate_launch_description():

    use_sim_time = LaunchConfiguration('use_sim_time')
    params_file = LaunchConfiguration('params_file')
    octomap_params_file = LaunchConfiguration('octomap_params_file')
    voc_file = LaunchConfiguration('voc_file')

    remappings = [
        ('/camera/rgb/image_raw', '/camera/color/image_raw'),
        ('/camera/depth_registered/image_raw', '/camera/depth/image_raw'),
        ('/camera/camera_info', '/camera/color/camera_info'),
    ]
    bringup_dir = get_package_share_directory('turn_on_mbot_robot')
    launch_dir = os.path.join(bringup_dir, 'launch')

    mbot_camera = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(bringup_dir,'launch', 'mbot_camera.launch.py')),
    )
    mbot_robot = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(launch_dir, 'turn_on_mbot_robot.launch.py')),
    )

    return LaunchDescription([
    	mbot_camera,mbot_robot,
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='false',
            description='Use simulation (Gazebo) clock if true'),

        DeclareLaunchArgument(
            'params_file',
            default_value=os.path.join(
                get_package_share_directory("orb_slam2_ros"),
                'ros', 'config', 'params_Astra_rgbd.yaml'),
            description='Full path to the ROS2 parameters file to use for all launched nodes'),
        DeclareLaunchArgument(
            'octomap_params_file',
            default_value=os.path.join(
                get_package_share_directory("orb_slam2_ros"),
                'ros', 'config', 'octomap_server.yaml'),
            description='Full path to the octomap_server parameters file to use'),

        DeclareLaunchArgument(
            'voc_file',
            default_value=os.path.join(
                get_package_share_directory("orb_slam2_ros"),
                'orb_slam2', 'Vocabulary', 'ORBvoc.txt'),
            description='Full path to vocabulary file to use'),

        Node(
            parameters=[
                params_file,
                {"voc_file": voc_file,
                 "use_sim_time": use_sim_time},
            ],
            package='orb_slam2_ros',
            executable='orb_slam2_ros_rgbd',
            name='orb_slam2_rgbd',
            output='screen',
            remappings=remappings
        ),
        
        Node(
            package='tf2_ros', 
            executable='static_transform_publisher', 
            name='map_to_cloud',
            arguments=['0', '0', '0','-1.57', '0','-1.57','map','cloudpoint'],
        ),
        
        Node(
            package='octomap_server',
            executable='octomap_server_node',
            name='octomap_server',
            output='screen',
            parameters=[
            	octomap_params_file,
            ],
            remappings=[('cloud_in', '/RGBD/cloud_points'),],
        )
    ])
