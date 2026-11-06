🌐 Language: **English** | [中文](README.zh.md)

# mbot-sim: Gazebo Simulation Navigation Verification

> **Repository status (archived snapshot)**
>
> This repository is frozen as a learning/verification record of the simulation stack on Ubuntu 24.04 / ROS 2 Jazzy. The four-package simulation chain described below (`eai_bot` perception + `mbot_description` / `mbot_gazebo` / `mbot_navigation`) reflects the state at the end of the simulation phase.
>
> Commits after the simulation phase added hardware-integration experiment packages — `mbot_lidar_ros2`, `mbot_robot_msg`, `mbot_robot_slam`, `mbot_robot_urdf`, `turn_on_mbot_robot` — targeting an Ubuntu 22.04 physical robot. They are kept for reference only and are **not** maintained in this repository.
>
> Active physical-robot development continues in a separate repository: kyshipit/dragonli-prowl.

This repository records the simulation navigation verification of a two-wheel differential-drive robot in Gazebo Harmonic: the model and coordinate frames are defined by `mbot_description`, the closed-loop differential drive is implemented by `mbot_gazebo`, and navigation and obstacle avoidance are provided by `mbot_navigation`. The perception side, `eai_bot`, is a vision/dialogue/TTS inference platform running on the RK3588 development board and meets the simulation chain at the TF level via ROS2 topics.

> This repository is a standard colcon workspace, purely software and decoupled from hardware.

## Simulation-phase Packages

| Package                                            | Subsystem               | Responsibility                                               | Hardware dependency      |
| :- | :---- | :----- | :----- |
| [`eai_bot`](https://src/eai_bot)                   | Perception              | YOLO/SCRFD detection, RKLLM dialogue, TTS speech; publishes `/eai/detections/*` | RK3588 development board |
| [`mbot_description`](https://src/mbot_description) | Simulation · Model      | Two-wheel differential-drive chassis URDF/xacro + TF tree; RViz visualization | None (pure geometry)     |
| [`mbot_gazebo`](https://src/mbot_gazebo)           | Simulation · Drive      | Gazebo Harmonic simulation; `/cmd_vel` → differential drive → `/odom` + TF + lidar/IMU | None (pure simulation)   |
| [`mbot_navigation`](https://src/mbot_navigation)   | Simulation · Navigation | Full Nav2 stack (SLAM + AMCL + NavFn/MPPI + costmap); obstacle-avoiding 2D Goal Pose navigation | None (pure simulation)   |

The remaining 5 hardware-integration experiment packages are listed in the repository structure below and are not maintained in this repository.

## Architecture

```text
Perception (eai_bot)                           Motion (mbot_*)
─────────────                                  ─────────────
YOLO/SCRFD detection ─┐                        URDF (mbot_description)
LLM dialogue          ├─ /eai/detections  ──>  Gazebo simulation (mbot_gazebo)
TTS speech           ─┘                   ──>  Nav2 navigation (mbot_navigation)
```

Simulation navigation is composed of three packages in sequence: `mbot_description` defines the robot body model and coordinate frames (TF tree); `mbot_gazebo` makes the robot move in simulation (closed-loop differential drive); `mbot_navigation` provides the navigation stack and obstacle avoidance (SLAM mapping + 2D Goal Pose).

## Quick Start

### Edge AI Perception (eai_bot, requires RK3588 cross toolchain)

```bash
# Cross-compile (ALIENTEK RK3588 toolchain + board-side ROS2 sysroot)
./build-linux.sh
# After syncing to the development board, run
ros2 run eai_bot eai_bot_app
```

For detailed configuration, message topics, and gating logic, see [`docs/`](https://docs/) (e.g., `architecture-and-runtime.md`).

### Simulation Navigation (mbot_*, x86 + ROS2 Jazzy + Gazebo Harmonic)

```bash
# Environment dependencies: ROS2 Jazzy + Gazebo Harmonic (gz 8) + Nav2 + slam_toolbox

# Terminal 1: start simulation (spawn robot + differential drive + lidar/IMU)
# The world parameter is resolved with $(find mbot_gazebo); default is empty.world
ros2 launch mbot_gazebo spawn.launch.py \
  world:=$(find mbot_gazebo | head -1)/share/mbot_gazebo/world/obstacles.world

# Terminal 2: start navigation (SLAM online mapping mode)
ros2 launch mbot_navigation navigation.launch.py

# After mapping, use RViz "2D Goal Pose" to click a goal; the robot navigates with obstacle avoidance
# Use localization mode with an existing map: slam:=false map:=<map.yaml path>
```

Manual mapping (keyboard teleop; requires a `twist_to_stamped` relay to convert Twist to TwistStamped):

```bash
ros2 run mbot_gazebo twist_to_stamped
ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r /cmd_vel:=/cmd_vel_raw
```

## ROS2 Integration (Simulation Chain)

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

## Repository Structure

```text
mbot-sim/                         # colcon workspace
├── src/
│   ├── eai_bot/                  # Perception: YOLO/SCRFD/LLM/TTS (RK3588)
│   ├── mbot_description/         # Simulation: URDF/xacro + TF tree
│   ├── mbot_gazebo/              # Simulation: Gazebo simulation + ros2_control differential drive
│   ├── mbot_lidar_ros2/          # Hardware integration experiment: lidar drivers (LD06/LD14/LSLidar)
│   ├── mbot_navigation/          # Simulation: Nav2 navigation parameters and launch
│   ├── mbot_robot_msg/           # Hardware integration experiment: custom messages
│   ├── mbot_robot_slam/          # Hardware integration experiment: SLAM (cartographer/slam_toolbox/orb-slam2)
│   ├── mbot_robot_urdf/          # Hardware integration experiment: physical robot URDF/meshes/rviz
│   └── turn_on_mbot_robot/       # Hardware integration experiment: physical robot launch
├── model/                        # RKNN/RKLLM inference models
├── docs/                         # eai_bot and platform docs
├── tools/                        # Configuration validation and model check scripts
├── build-linux.sh                # eai_bot cross-compilation script
└── toolchain_rk3588.cmake        # RK3588 cross toolchain
```

## License

MIT License