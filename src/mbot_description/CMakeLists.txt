cmake_minimum_required(VERSION 3.8)
project(mbot_description)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

# fake_joint_states：阶段 1 静态 joint 状态源（RViz 显示用）。
add_executable(fake_joint_states src/fake_joint_states.cpp)
ament_target_dependencies(fake_joint_states rclcpp sensor_msgs)
install(TARGETS fake_joint_states DESTINATION lib/${PROJECT_NAME})

# 安装 URDF/xacro 与 launch/rviz 等资源，供其他包与运行时读取。
install(DIRECTORY urdf launch rviz
  DESTINATION share/${PROJECT_NAME}
)

ament_package()