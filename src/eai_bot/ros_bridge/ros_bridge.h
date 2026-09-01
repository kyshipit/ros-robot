// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 KY (kyshipit)

/*
 * ros_bridge/ros_bridge.h
 *
 * ROS2 桥接层：在装配层创建 ROS 节点和发布者，提供 PublishTask() 供
 * Pipeline 的 PostTaskHandler 回调使用。
 *
 * 使用方式（在 main 中）：
 *   RosBridge bridge(argc, argv, "eai_detector");
 *   pipeline.SetPostTaskHandler([&](InferenceTask& task) {
 *       bridge.PublishTask(task);   // ROS 发布
 *       // ... 其他处理（显示、门控等）
 *       return true;
 *   });
 *   pipeline.Run();
 *
 * 不修改 engine/、platform/、adapters/ 任何代码。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "engine/pipeline.h"

// Forward declarations for ROS2 types (no rclcpp include in header).
namespace rclcpp {
class Node;
template <typename MessageT, typename AllocatorT>
class Publisher;
}  // namespace rclcpp

namespace eai {
namespace ros_bridge {

class RosBridge {
public:
    // 构造时初始化 ROS2 节点和发布者。
    // @param argc, argv  来自 main 的命令行参数。
    // @param node_name    ROS2 节点名，默认 "eai_detector"。
    RosBridge(int argc, char** argv, const std::string& node_name = "eai_detector");

    ~RosBridge();

    // 将 InferenceTask 中的每个 slot 结果转换为 ROS 消息并发布。
    // 在 PostTaskHandler 中每帧调用。
    // 发布 topic：
    //   /eai/detections/yolo  — YOLO 检测结果
    //   /eai/detections/scrfd — SCRFD 人脸检测结果
    void PublishTask(const InferenceTask& task);

    // 兼容旧接口：安装自身到 Pipeline（仅适用于纯无头模式，不需要其他 PostTaskHandler）。
    void Install(Pipeline& pipeline);

    // 绑定外部输入处理：订阅 /eai/user_prompt（std_msgs/String），
    // 收到文本后回调 handler（非 ROS 线程；仅转发到调用方注入的逻辑）。
    // handler 返回值决定是否被接受（与 SubmitUserPrompt 语义一致）。
    void SetInputHandler(std::function<bool(const std::string&)> handler);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ros_bridge
}  // namespace eai