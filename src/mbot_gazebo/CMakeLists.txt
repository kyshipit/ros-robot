cmake_minimum_required(VERSION 3.8)
project(mbot_gazebo)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)

# twist_to_stamped：把 teleop 的 Twist 转成 TwistStamped，供键盘遥控建图。
add_executable(twist_to_stamped src/twist_to_stamped.cpp)
ament_target_dependencies(twist_to_stamped rclcpp geometry_msgs)
install(TARGETS twist_to_stamped DESTINATION lib/${PROJECT_NAME})

# 安装 URDF/xacro、launch、config、world 资源。
install(DIRECTORY urdf launch config world
  DESTINATION share/${PROJECT_NAME}
)

ament_package()