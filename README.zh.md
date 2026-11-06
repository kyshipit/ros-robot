🌐 语言: **中文** | [English](README.md)

# mbot-sim：Gazebo 仿真导航验证

> **仓库状态（归档快照）**
>
> 本仓冻结为 Ubuntu 24.04 / ROS 2 Jazzy 上仿真栈的学习与验证记录。下文描述的四包仿真链路（`eai_bot` 感知 + `mbot_description` / `mbot_gazebo` / `mbot_navigation`）为仿真阶段结束时的状态。
>
> 仿真阶段之后的提交新增了面向 Ubuntu 22.04 实物机器人的硬件集成试验包——`mbot_lidar_ros2`、`mbot_robot_msg`、`mbot_robot_slam`、`mbot_robot_urdf`、`turn_on_mbot_robot`——仅作参考保留，**不在本仓维护**。
>
> 实物机器人开发主线已迁至独立仓库：kyshipit/dragonli-prowl。

本仓记录两轮差速机器人在 Gazebo Harmonic 中的仿真导航验证：模型与坐标系由 `mbot_description` 定义，差速驱动闭环由 `mbot_gazebo` 实现，导航与避障由 `mbot_navigation` 提供；感知侧 `eai_bot` 为运行在 RK3588 开发板上的视觉/对话/TTS 推理平台，与仿真链路通过 ROS2 话题在 TF 层面汇合。

> 本仓库是标准 colcon 工作空间，纯软件、与硬件解耦。

## 仿真阶段包

| 包                                                 | 子系统    | 职责                                                         | 依赖硬件      |
| :------------------------------------------------- | :-------- | :----------------------------------------------------------- | :------------ |
| [`eai_bot`](https://src/eai_bot)                   | 感知      | YOLO/SCRFD 检测、RKLLM 对话、TTS 语音，发布 `/eai/detections/*` | RK3588 开发板 |
| [`mbot_description`](https://src/mbot_description) | 仿真·模型 | 两轮差速底盘 URDF/xacro + TF 树，RViz 可视化                 | 无（纯几何）  |
| [`mbot_gazebo`](https://src/mbot_gazebo)           | 仿真·驱动 | Gazebo Harmonic 仿真，`/cmd_vel` → 差速 → `/odom` + TF + 激光/IMU | 无（纯仿真）  |
| [`mbot_navigation`](https://src/mbot_navigation)   | 仿真·导航 | Nav2 全栈（SLAM + AMCL + NavFn/MPPI + costmap），2D Goal Pose 避障 | 无（纯仿真）  |

其余 5 个硬件集成试验包见下文仓库结构，不在本仓维护。

## 架构

```text
感知（eai_bot）                        运动（mbot_*）
─────────────                         ─────────────
YOLO/SCRFD 检测 ──┐                   URDF（mbot_description）
LLM 对话          ├─ /eai/detections  ──> Gazebo 仿真（mbot_gazebo）
TTS 语音         ─┘                   ──> Nav2 导航（mbot_navigation）
```

仿真导航由三个包依次构成：`mbot_description` 定义机器人本体模型与坐标系（TF 树）；`mbot_gazebo` 让机器人在仿真里动起来（差速驱动闭环）；`mbot_navigation` 提供导航栈与避障（SLAM 建图 + 2D Goal Pose）。

## 快速开始

### 边缘 AI 感知（eai_bot，需 RK3588 交叉工具链）

```bash
# 交叉编译（正点原子 RK3588 工具链 + 板端 ROS2 sysroot）
./build-linux.sh
# 同步到开发板后运行
ros2 run eai_bot eai_bot_app
```

详细配置、消息话题、门控逻辑见 [`docs/`](https://docs/)（`architecture-and-runtime.md` 等）。

### 仿真导航（mbot_*，x86 + ROS2 Jazzy + Gazebo Harmonic）

```bash
# 环境依赖：ROS2 Jazzy + Gazebo Harmonic（gz 8）+ Nav2 + slam_toolbox

# 终端 1：起仿真（spawn 机器人 + 差速驱动 + 激光/IMU）
# world 参数用 $(find mbot_gazebo) 解析，默认 empty.world
ros2 launch mbot_gazebo spawn.launch.py \
  world:=$(find mbot_gazebo | head -1)/share/mbot_gazebo/world/obstacles.world

# 终端 2：起导航（SLAM 在线建图模式）
ros2 launch mbot_navigation navigation.launch.py

# 建图后用 RViz 的 "2D Goal Pose" 点目标点，机器人避障导航
# 已建图用定位模式：slam:=false map:=<map.yaml 路径>
```

手动建图（键盘遥控，需 `twist_to_stamped` relay 把 Twist 转 TwistStamped）：

```bash
ros2 run mbot_gazebo twist_to_stamped
ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r /cmd_vel:=/cmd_vel_raw
```

## ROS2 集成（仿真链路）

| 话题                                 | 类型          | 方向 | 说明                                                         |
| :----------------------------------- | :------------ | :--- | :----------------------------------------------------------- |
| `/cmd_vel`（`~/cmd_vel`）            | TwistStamped  | 订阅 | diff_drive_controller 输入（Nav2 controller_server 经 remap 发布） |
| `/odom`                              | Odometry      | 发布 | 里程计，`diff_drive_controller` 输出                         |
| `/scan`                              | LaserScan     | 发布 | 激光雷达（gpu_lidar，frame_id=`laser_link`）                 |
| `/imu`                               | Imu           | 发布 | 惯性测量单元                                                 |
| `/joint_states`                      | JointState    | 发布 | `joint_state_broadcaster`                                    |
| `/map`                               | OccupancyGrid | 发布 | SLAM 建图输出                                                |
| `/map` → `/odom` → `/base_footprint` | TF 树         | —    | 导航坐标链条                                                 |

感知侧话题（`/eai/detections/yolo`、`/eai/detections/scrfd`、`/eai/user_prompt`）见 `eai_bot` 的 docs。

## 仓库结构

```text
mbot-sim/                         # colcon 工作空间
├── src/
│   ├── eai_bot/                  # 感知：YOLO/SCRFD/LLM/TTS（RK3588）
│   ├── mbot_description/         # 仿真：URDF/xacro + TF 树
│   ├── mbot_gazebo/              # 仿真：Gazebo 仿真 + ros2_control 差速
│   ├── mbot_lidar_ros2/          # 硬件集成试验：激光雷达驱动（LD06/LD14/LSLidar）
│   ├── mbot_navigation/          # 仿真：Nav2 导航参数与 launch
│   ├── mbot_robot_msg/           # 硬件集成试验：自定义消息
│   ├── mbot_robot_slam/          # 硬件集成试验：SLAM（cartographer/slam_toolbox/orb-slam2）
│   ├── mbot_robot_urdf/          # 硬件集成试验：实物 URDF/meshes/rviz
│   └── turn_on_mbot_robot/       # 硬件集成试验：实物启动 launch
├── model/                        # RKNN/RKLLM 推理模型
├── docs/                         # eai_bot 与平台文档
├── tools/                        # 配置校验、模型检查脚本
├── build-linux.sh                # eai_bot 交叉编译脚本
└── toolchain_rk3588.cmake        # RK3588 交叉工具链
```

## License

MIT License