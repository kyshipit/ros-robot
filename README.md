🌐 Language: **English** | [中文](README.zh.md)

# ros-robot: Edge AI Robot Platform (RK3588 Perception + Gazebo Simulation Navigation)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
![ROS2](https://img.shields.io/badge/ROS2-Jazzy-0091BD?style=flat-square&logo=ros&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-RK3588-red)
![Gazebo](https://img.shields.io/badge/Gazebo-Harmonic-8A2BE2)
![Nav2](https://img.shields.io/badge/Nav2-1.3-00B5AD)
![YOLO](https://img.shields.io/badge/YOLOv5-111F68)
![RKNN](https://img.shields.io/badge/RKNN-orange)
![RKLLM](https://img.shields.io/badge/RKLLM-brightgreen)

ros-robot is a **phased, full-stack robot platform** containing two mutually independent subsystems:

1. **Edge AI Perception** (`eai_bot`) — a vision/dialogue/TTS inference platform running on the RK3588 development board;
2. **Simulation Navigation** (`mbot_description` / `mbot_gazebo` / `mbot_navigation`) — a two-wheel differential-drive robot and Nav2 navigation stack in Gazebo Harmonic, purely software and decoupled from hardware.

> This repository is a standard colcon workspace. The two subsystems meet at the coordinate frame (TF) level via ROS2 topics, and perception results ultimately drive navigation decisions (Stage 4, real-robot integration).

------

## 📦 Four Packages

| Package                                            | Subsystem               | Responsibility                                               | Hardware dependency      |
| :------------------------------------------------- | :---------------------- | :----------------------------------------------------------- | :----------------------- |
| [`eai_bot`](https://src/eai_bot)                   | Perception              | YOLO/SCRFD detection, RKLLM dialogue, TTS speech; publishes `/eai/detections/*` | RK3588 development board |
| [`mbot_description`](https://src/mbot_description) | Simulation · Model      | Two-wheel differential-drive chassis URDF/xacro + TF tree; RViz visualization | None (pure geometry)     |
| [`mbot_gazebo`](https://src/mbot_gazebo)           | Simulation · Drive      | Gazebo Harmonic simulation; `/cmd_vel` → differential drive → `/odom` + TF + lidar/IMU | None (pure simulation)   |
| [`mbot_navigation`](https://src/mbot_navigation)   | Simulation · Navigation | Full Nav2 stack (SLAM + AMCL + NavFn/MPPI + costmap); obstacle-avoiding 2D Goal Pose navigation | None (pure simulation)   |

------

## 🏗️ Architecture and Three Stages

```
Perception (eai_bot)                                 Motion (mbot_*)
─────────────                                        ─────────────
YOLO/SCRFD detection ─┐                              URDF (mbot_description)
LLM dialogue          ├─     /eai/detections  ──>    Gazebo simulation (mbot_gazebo)
TTS speech           ─┘  (Stage 4 integration) ──>   Nav2 navigation (mbot_navigation)
```



Simulation navigation is built in three stages (all purely software; get them working in simulation before tuning navigation on the real robot):

1. **Stage 1 · `mbot_description`** — robot body model and coordinate frames (TF tree);
2. **Stage 2 · `mbot_gazebo`** — make the robot move in simulation (closed-loop differential drive);
3. **Stage 3 · `mbot_navigation`** — navigation stack and obstacle avoidance (SLAM mapping + 2D Goal Pose).

> Stage 4 (real-robot integration) requires hardware cooperation: replace the Gazebo hardware interface with a real motor driver, connect a real lidar/IMU, perform hand-eye calibration, and tune on the real robot. Real-robot integration is to be added.

------

## 🚀 Quick Start

### Edge AI Perception (eai_bot, requires RK3588 cross toolchain)

```
# Cross-compile (ALIENTEK RK3588 toolchain + board-side ROS2 sysroot)
./build-linux.sh
# After syncing to the development board, run
ros2 run eai_bot eai_bot_app
```



For detailed configuration, message topics, and gating logic, see [`docs/`](https://docs/) (e.g., `architecture-and-runtime.md`).

### Simulation Navigation (mbot_*, x86 + ROS2 Jazzy + Gazebo Harmonic)

```
# Environment dependencies: ROS2 Jazzy + Gazebo Harmonic (gz 8) + Nav2 + slam_toolbox

# Terminal 1: start simulation (spawn robot + differential drive + lidar/IMU)
#   The world parameter is resolved with $(find mbot_gazebo); default is empty.world
ros2 launch mbot_gazebo spawn.launch.py \
  world:=$(find mbot_gazebo | head -1)/share/mbot_gazebo/world/obstacles.world

# Terminal 2: start navigation (SLAM online mapping mode)
ros2 launch mbot_navigation navigation.launch.py

# After mapping, use RViz "2D Goal Pose" to click a goal; the robot navigates with obstacle avoidance
# Use localization mode with an existing map: slam:=false map:=<map.yaml path>
```



Manual mapping (keyboard teleop; requires a `twist_to_stamped` relay to convert Twist to TwistStamped):

bash

```
ros2 run mbot_gazebo twist_to_stamped
ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r /cmd_vel:=/cmd_vel_raw
```



------

## 🔌 ROS2 Integration (Simulation Chain)

| Topic                                | Type          | Direction | Description                                                  |
| :----------------------------------- | :------------ | :-------- | :----------------------------------------------------------- |
| `/cmd_vel` (`~/cmd_vel`)             | TwistStamped  | Subscribe | diff_drive_controller input (published by Nav2 controller_server via remap) |
| `/odom`                              | Odometry      | Publish   | Odometry, output by `diff_drive_controller`                  |
| `/scan`                              | LaserScan     | Publish   | Lidar (gpu_lidar, frame_id=`laser_link`)                     |
| `/imu`                               | Imu           | Publish   | Inertial measurement unit                                    |
| `/joint_states`                      | JointState    | Publish   | `joint_state_broadcaster`                                    |
| `/map`                               | OccupancyGrid | Publish   | SLAM mapping output                                          |
| `/map` → `/odom` → `/base_footprint` | TF tree       | —         | Navigation coordinate chain                                  |

For perception-side topics (`/eai/detections/yolo`, `/eai/detections/scrfd`, `/eai/user_prompt`), see the `eai_bot` docs.

------

## 📂 Repository Structure

text

```
ros-robot/                        # colcon workspace
├── src/
│   ├── eai_bot/                  # Perception: YOLO/SCRFD/LLM/TTS (RK3588)
│   ├── mbot_description/         # Stage 1: URDF/xacro + TF tree
│   ├── mbot_gazebo/              # Stage 2: Gazebo simulation + ros2_control differential drive
│   └── mbot_navigation/          # Stage 3: Nav2 navigation parameters and launch
├── model/                        # RKNN/RKLLM inference models
├── docs/                         # eai_bot and platform docs
├── tools/                        # Configuration validation and model check scripts
├── build-linux.sh                # eai_bot cross-compilation script
└── toolchain_rk3588.cmake        # RK3588 cross toolchain
```

------

## 📄 License

MIT License