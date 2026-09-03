cmake_minimum_required(VERSION 3.8)
project(mbot_gazebo)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)

# 安装 URDF/xacro、launch、config、world 资源。
install(DIRECTORY urdf launch config world
  DESTINATION share/${PROJECT_NAME}
)

ament_package()