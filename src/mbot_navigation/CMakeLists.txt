cmake_minimum_required(VERSION 3.8)
project(mbot_navigation)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)

# 安装配置、launch、地图与 rviz 资源。
install(DIRECTORY config launch map rviz
  DESTINATION share/${PROJECT_NAME}
)

ament_package()