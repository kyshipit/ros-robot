# ros-robot：基于 ROS2 的插件化边缘 AI 机器人平台

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
![License: MIT](https://img.shields.io/badge/ROS2-0091BD?style=flat-square&logo=ros&logoColor=white)
![OpenCV](https://img.shields.io/badge/OpenCV-green?logo=opencv)
![Platform](https://img.shields.io/badge/Platform-RK3588-red)
![YOLO](https://img.shields.io/badge/YOLOv5-111F68?logo=yolo&logoColor=white)
![TTS](https://img.shields.io/badge/TTS-MeloTTS-blue)
![SCRFD](https://img.shields.io/badge/SCRFD-FF6F00)
![RKNN](https://img.shields.io/badge/RKNN-orange)
![RKLLM](https://img.shields.io/badge/RKLLM-brightgreen)

ros-robot 是为 Rockchip RK3588 设计、基于 ROS2（Jazzy）的可扩展边缘 AI 机器人平台。通过一份 YAML 配置文件驱动，结合多线程视频流水线与插件化架构，内置 YOLO、SCRFD 模型，并支持本地 RKLLM 对话及语音合成（TTS）。检测结果通过自定义 ROS2 消息实时发布，默认应用展示了人脸门控、AI 问候与对话等功能。

> 本仓库是标准 colcon 工作空间：核心逻辑封装为 ROS2 包 `eai_bot`，通过 `ros_bridge` 层将推理结果发布为 ROS2 话题，业务与 ROS 解耦。


---
## 📋 目录

- [✨ 核心特性](#-核心特性)
- [🏗️ 架构设计](#️-架构设计)
- [🚀 快速开始](#-快速开始)
- [🔌 ROS2 集成](#-ros2-集成)
- [⚙️ 配置说明](#️-配置说明)
- [📖 文档与代码入口](#-文档与代码入口)
- [🔧 本地检查工具](#-本地检查工具)
- [📄 License](#-license)
---

## ✨ 核心特性

- **插件化架构**：YOLO、SCRFD、LLM、TTS 均以插件形式实现，按需加载。

- **配置驱动**：所有功能（检测、对话、语音）通过 `default.yaml` 灵活开关。

- **ROS2 集成**：以标准 `eai_bot` 包构建，检测结果实时发布为 ROS2 话题，可被其他 ROS 节点订阅。

- **低延迟流水线**：视频帧处理与 LLM/TTS 逻辑分离，保证实时推理性能。

- **开箱即用的人脸门控场景**：人员检测 → 人脸识别 → 自动问候 → 语音对话，完整端到端流程。

- **支持本地大模型**：集成 RKLLM，无需联网即可进行对话推理。

- **跨平台编译**：基于 colcon + 交叉工具链，一键编译并部署到正点原子 RK3588 开发板。


## 🏗️ 架构设计

| 层       | 目录                                     | 职责                                      |
| -------- | ---------------------------------------- | ----------------------------------------- |
| 入口     | `src/eai_bot/app/`                       | 读取 YAML，装配 Pipeline、ModelCoordinator 与 RosBridge |
| 采集/显示 | `src/eai_bot/capture/`, `src/eai_bot/display/` | 采集帧、旋转、画框、OpenCV 预览    |
| 引擎     | `src/eai_bot/engine/`                    | 预处理 → 推理 → 后处理回调               |
| 策略     | `src/eai_bot/platform/`                  | 场景切换、人脸门控、自动问候逻辑          |
| 模型插件 | `src/eai_bot/adapters/`                  | yolo / scrfd / llm / tts 插件，按配置启停 |
| ROS2 桥接 | `src/eai_bot/ros_bridge/`               | 发布 ROS2 话题，业务与 ROS 解耦          |

更详细的启动顺序、线程模型与设计决策请参阅：[docs/architecture-and-runtime.md](docs/architecture-and-runtime.md)。

## 🚀 快速开始

> 环境依赖：需要正点原子 RK3588 交叉工具链（`/opt/atk-dlrk3588-toolchain`）、板端 ROS2 sysroot（默认 `~/software/rk_sysroot`，可用 `SYSROOT_ROS` 环境变量覆盖），以及宿主机安装的 colcon 工具。

```bash
# 1. 在仓库根目录执行交叉编译脚本（colcon build）
./build-linux.sh

# 2. 将编译产物（install 目录）同步到开发板（替换 <board_ip> 为板端地址）
rsync -avz install/ root@<board_ip>:/path/to/workspace/install/

# 3. 在板端工作空间目录下，source 后运行
#    source /opt/ros/jazzy/setup.bash
#    source install/setup.bash
ros2 run eai_bot eai_bot_app
```

## 🔌 ROS2 集成

- **包名**：`eai_bot`（`ament_cmake` 包，`package.xml`）
- **节点**：`eai_detector`（默认，见 `src/eai_bot/ros_bridge/ros_bridge.h`）
- **发布话题**：

| 话题                      | 消息类型              | 说明                         |
| ------------------------- | --------------------- | ---------------------------- |
| `/eai/detections/yolo`    | `eai_bot/DetectionResult` | YOLO 检测结果             |
| `/eai/detections/scrfd`   | `eai_bot/DetectionResult` | SCRFD 人脸检测结果        |

- **订阅话题**：

| 话题                      | 消息类型              | 说明                         |
| ------------------------- | --------------------- | ---------------------------- |
| `/eai/user_prompt`        | `std_msgs/String`     | 外部节点向 LLM 提交对话文本（需人脸门控开启才被接受） |

- **自定义消息**（`src/eai_bot/msg/`）：

| 消息                 | 说明                                         |
| -------------------- | -------------------------------------------- |
| `Point2D`            | 2D 像素坐标                                  |
| `Box`                | 检测框（label、坐标、score、可选 5 个人脸关键点） |
| `DetectionResult`    | 每帧每槽位一条：frame_id、slot、是否有人/人脸、所有框 |

桥接层通过 `ros_bridge/ros_bridge.cpp` 将推理结果转换为消息发布，并订阅 `/eai/user_prompt` 接收外部对话输入；engine/、platform/、adapters/ 等业务代码不感知 ROS。订阅示例：

```bash
# 在板端另开终端，source 工作空间后订阅
ros2 topic echo /eai/detections/yolo eai_bot/msg/DetectionResult

# 向 LLM 提交对话文本（需已检测到稳定人脸、门控开启）
ros2 topic pub /eai/user_prompt std_msgs/msg/String "{data: '你好'}" --once
```

除 ROS 话题外，应用也支持**终端键盘输入**：直接在前台终端打字并回车即可提交对话（等价于发布 `/eai/user_prompt`），提交时会回显 `YOU>` 行。两种输入方式均复用同一条入口、统一经过人脸门控判断。

## ⚙️ 配置说明

关键配置项（位于 `src/eai_bot/config/default.yaml`）：

| 配置项 | 说明 |
| ------ | ---- |
| `model.llm.enabled` | 是否启用 LLM 对话链路；若 `false` 或模型缺失，仅运行视觉检测 |
| `model.tts.enabled` | 是否启用 TTS 语音播报（需 `model.llm.enabled` 为 `true`） |
| `model.tts.skip_static_greeting` | `true` 时，人脸稳定后不播报静态问候语（只播报后续对话） |
| `model.yolo.path` | YOLO 模型文件路径（相对于可执行文件） |
| `model.scrfd.path` | SCRFD 人脸检测模型路径 |
| `model.llm.path` | RKLLM 模型文件路径 |
| `model.tts.encoder_path` / `decoder_path` | TTS 编码器/解码器 RKNN 路径 |
| `model.tts.lexicon_path` / `tokens_path` | TTS 词表文件路径 |
| `capture.device` | 摄像头设备节点（如 `/dev/video0`） |
| `display.window_name` | 预览窗口标题 |

完整字段及注释请参考 `src/eai_bot/config/default.yaml`。

---

## 📖 文档与代码入口

| 文档/用途 | 说明 |
| --------- | ---- |
| [docs/architecture-and-runtime.md](docs/architecture-and-runtime.md) | 启动顺序、流水线、槽位与平台设计详情 |
| [docs/tts-melotts.md](docs/tts-melotts.md) | TTS 实现细节与验收指南 |
| [docs/llm-model-coordinator.md](docs/llm-model-coordinator.md) | RKLLM 协调、门控逻辑与终端交互 UX |
| [docs/troubleshooting.md](docs/troubleshooting.md) | 常见问题排查（无框、路径错误、退出崩溃等） |
| [docs/adapters.md](docs/adapters.md) | 各插件模块（yolo/scrfd/llm/tts）的文件职责说明 |

**代码入口**：

| 用途 | 路径 |
| ---- | ---- |
| 主程序入口 | `src/eai_bot/app/main_ros.cc` |
| 每帧处理流水线 | `src/eai_bot/engine/pipeline.cpp` |
| 场景协调器 | `src/eai_bot/platform/model_coordinator.cpp` |
| 人脸门控与问候逻辑 | `src/eai_bot/platform/llm_greeting.cpp` |
| ROS2 桥接层 | `src/eai_bot/ros_bridge/ros_bridge.cpp` |
| RKLLM 插件 | `src/eai_bot/adapters/llm/` |
| TTS 实现 | `src/eai_bot/voice/` + `src/eai_bot/adapters/melotts/` |
| 默认配置文件 | `src/eai_bot/config/default.yaml` |

---

## 🔧 本地检查工具

在推送至板端前，可在仓库根目录运行以下脚本进行预检：

```bash
# 检查 default.yaml 字段类型与范围（不检查文件存在性）
python3 tools/check_config.py

# 检查 model/ 下各模型文件是否存在（若缺少 .rkllm 仅给出警告）
./tools/check_models.sh
```

## 仓库结构

```text
ros-robot/                      # colcon 工作空间根目录
├── build-linux.sh              # colcon 交叉编译脚本（RK3588 / aarch64 / ROS2 Jazzy）
├── toolchain_rk3588.cmake      # 交叉编译工具链配置
├── model/                      # yolov5.rknn、scrfd.rknn、.rkllm、TTS encoder/decoder RKNN、lexicon.txt、tokens.txt
├── docs/                       # 平台文档
├── tools/                      # 开发/集成辅助（配置校验、模型文件检查等），不参与板端运行
└── src/
    └── eai_bot/                # ROS2 包（ament_cmake）
        ├── package.xml
        ├── CMakeLists.txt
        ├── msg/                # 自定义消息：Point2D、Box、DetectionResult
        ├── app/                # main_ros.cc 入口（装载 config 与 ROS 装配）
        ├── ros_bridge/         # ROS2 桥接层（发布检测话题）
        ├── engine/ platform/ capture/ display/ voice/
        ├── adapters/           # yolo / scrfd / llm / melotts 插件
        ├── 3rdparty/           # rknpu2 / rkllm / onnx 等依赖
        ├── utils/
        └── config/default.yaml
```

## 📄 License

MIT License
