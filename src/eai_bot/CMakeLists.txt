cmake_minimum_required(VERSION 3.15)
project(eai_bot)

# ==========================================================================
# 1. C++ 标准
# ==========================================================================
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED TARGET_SOC)
  set(TARGET_SOC "rk3588")
  message(STATUS "eai_bot: TARGET_SOC default rk3588")
endif()
message(STATUS "eai_bot: TARGET_SOC=${TARGET_SOC}")

# ==========================================================================
# 2. ROS2 消息生成
# ==========================================================================
find_package(ament_cmake REQUIRED)

set(CMAKE_DISABLE_FIND_PACKAGE_rosidl_generator_py TRUE)
find_package(rosidl_default_generators REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

set(msg_files
  "msg/Point2D.msg"
  "msg/Box.msg"
  "msg/DetectionResult.msg"
)

message(STATUS "=== msg_files content: ${msg_files} ===")
rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
)

# ==========================================================================
# 3. 3rdparty 和 utils
# ==========================================================================
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ 3rdparty.out)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/utils/ utils.out)

# ==========================================================================
# 4. 平台相关库
# ==========================================================================
set(RKNN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/rknpu2/Linux/aarch64)
set(LIBRKNNRT ${RKNN_API_DIR}/librknnrt.so)
set(LIBRKNNRT_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/rknpu2/include)

set(RKLLM_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/rkllm)
set(LIBRKLLMRT ${RKLLM_API_DIR}/lib/librkllmrt.so)
set(LIBGOMP ${RKLLM_API_DIR}/lib/libgomp/libgomp.so)

# ---------- OpenCV：使用工具链 sysroot 中的完整 OpenCV ----------
set(TOOLCHAIN_SYSROOT /opt/atk-dlrk3588-toolchain/aarch64-buildroot-linux-gnu/sysroot)
set(OPENCV_INCLUDE_DIRS ${TOOLCHAIN_SYSROOT}/usr/include/opencv4)
set(OPENCV_LIB_DIR ${TOOLCHAIN_SYSROOT}/usr/lib)
set(OPENCV_LIBS
  ${OPENCV_LIB_DIR}/libopencv_core.so
  ${OPENCV_LIB_DIR}/libopencv_flann.so
  ${OPENCV_LIB_DIR}/libopencv_videoio.so
  ${OPENCV_LIB_DIR}/libopencv_video.so
  ${OPENCV_LIB_DIR}/libopencv_highgui.so
  ${OPENCV_LIB_DIR}/libopencv_imgcodecs.so
  ${OPENCV_LIB_DIR}/libopencv_imgproc.so
  ${OPENCV_LIB_DIR}/libopencv_dnn.so
)

# ==========================================================================
# 5. 可执行文件 eai_bot_app
# ==========================================================================
set(EAI_BOT_SRC
  app/main_ros.cc
  app/console_ui/console_ui.cpp
  engine/pipeline.cpp
  engine/thread_pool.cpp
  platform/model_coordinator.cpp
  platform/llm_greeting.cpp
  capture/camera_source.cpp
  capture/frame_transform.cpp
  display/opencv_display_sink.cpp
  display/result_overlay.cpp
  adapters/yolo/yolo_adapter.cpp
  adapters/yolo/yolo_postprocess.cpp
  adapters/scrfd/scrfd_adapter.cpp
  adapters/scrfd/scrfd_postprocess.cpp
  adapters/llm/rkllm_session.cpp
  adapters/llm/llm_worker.cpp
  adapters/melotts/melotts_engine.cc
  adapters/melotts/melotts_process.cc
  adapters/melotts/melotts_session.cpp
  voice/tts_worker.cpp
  voice/audio_player.cpp
  voice/tts_text_sanitizer.cpp
  voice/tts_ingress.cpp
  voice/tts_planner.cpp
  voice/voice_reply_bridge.cpp
  ros_bridge/ros_bridge.cpp
)

add_executable(eai_bot_app ${EAI_BOT_SRC})
target_link_options(eai_bot_app PRIVATE -no-pie)

# 链接选项：覆盖 sysroot 为 ROS sysroot，保证 ROS 依赖的标准库（libm等）能找到
# 同时通过 rpath-link 让工具链 OpenCV 的依赖也能找到
if(DEFINED ROS_SYSROOT)
  target_link_options(eai_bot_app PRIVATE
    -Wl,--sysroot=${ROS_SYSROOT}
    -Wl,-rpath-link,${TOOLCHAIN_SYSROOT}/usr/lib
    -Wl,-rpath-link,${TOOLCHAIN_SYSROOT}/lib
    -Wl,-rpath-link,${ROS_SYSROOT}/lib/aarch64-linux-gnu
    -Wl,-rpath-link,${ROS_SYSROOT}/usr/lib/aarch64-linux-gnu
    -Wl,--dynamic-linker=/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1
  )
  target_link_directories(eai_bot_app PRIVATE
    ${TOOLCHAIN_SYSROOT}/usr/lib
    ${TOOLCHAIN_SYSROOT}/lib
    ${ROS_SYSROOT}/lib/aarch64-linux-gnu
    ${ROS_SYSROOT}/usr/lib/aarch64-linux-gnu
  )
endif()

target_include_directories(eai_bot_app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/adapters/yolo
  ${CMAKE_CURRENT_SOURCE_DIR}/adapters/scrfd
  ${CMAKE_CURRENT_SOURCE_DIR}/adapters/llm
  ${CMAKE_CURRENT_SOURCE_DIR}/adapters/melotts
  ${CMAKE_CURRENT_SOURCE_DIR}/voice
  ${CMAKE_CURRENT_SOURCE_DIR}/utils
  ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/timer
  ${RKLLM_API_DIR}/include
  ${LIBRKNNRT_INCLUDES}
  ${OPENCV_INCLUDE_DIRS}
)

find_package(ament_index_cpp REQUIRED)

target_link_libraries(eai_bot_app
  imageutils
  fileutils
  imagedrawing
  audioutils
  ${LIBRKNNRT}
  ${LIBRKLLMRT}
  ${LIBGOMP}
  ${OPENCV_LIBS}
  ament_index_cpp::ament_index_cpp
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(eai_bot_app Threads::Threads)

add_definitions(-DTRACETOOLS_DISABLED)
rosidl_get_typesupport_target(cpp_typesupport_target "${PROJECT_NAME}" "rosidl_typesupport_cpp")
target_link_libraries(eai_bot_app "${cpp_typesupport_target}")
target_link_libraries(eai_bot_app rclcpp::rclcpp)
target_link_libraries(eai_bot_app std_msgs::std_msgs)

install(TARGETS eai_bot_app DESTINATION lib/${PROJECT_NAME})
install(FILES ${LIBRKNNRT} DESTINATION lib)
install(FILES ${LIBRKLLMRT} ${LIBGOMP} DESTINATION lib)

install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../../model/coco_80_labels_list.txt
  DESTINATION share/${PROJECT_NAME}/model)
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../../model/yolov5.rknn
  ${CMAKE_CURRENT_SOURCE_DIR}/../../model/scrfd.rknn
  DESTINATION share/${PROJECT_NAME}/model OPTIONAL)
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../../model/encoder-ZH_MIX_EN.rknn
  ${CMAKE_CURRENT_SOURCE_DIR}/../../model/decoder-ZH_MIX_EN.rknn
  ${CMAKE_CURRENT_SOURCE_DIR}/../../model/lexicon.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/../../model/tokens.txt
  DESTINATION share/${PROJECT_NAME}/model OPTIONAL)
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/*.yaml")
install(FILES ${CONFIG_FILES} DESTINATION share/${PROJECT_NAME}/config)

ament_export_dependencies(rosidl_default_runtime)
ament_package()