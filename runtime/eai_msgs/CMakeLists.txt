cmake_minimum_required(VERSION 3.10)
project(eai_msgs)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

set(msg_files
  msg/Point2D.msg
  msg/Box.msg
  msg/DetectionResult.msg
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  DEPENDENCIES
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()