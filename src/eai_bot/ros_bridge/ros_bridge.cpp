// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 KY (kyshipit)

/*
 * ros_bridge/ros_bridge.cpp
 *
 * RosBridge 实现：持有 ROS2 Node 和 Publisher，在 PublishTask() 中
 * 转换 InferenceTask → DetectionResult 消息并发布。
 */

// rclcpp 必须在 ros_bridge.h 之前 include，因为 ros_bridge.h 中有
// rclcpp::Publisher 的 forward declare，而 create_publisher 需要完整定义。
#include "rclcpp/rclcpp.hpp"

#include "ros_bridge/ros_bridge.h"

#include <memory>
#include <string>

#include "std_msgs/msg/string.hpp"

#include "eai_bot/msg/detection_result.hpp"

#include "ros_bridge/detection_msg_builder.h"
#include "platform/logging.h"

namespace eai {
namespace ros_bridge {

class RosBridge::Impl {
public:
    Impl(int argc, char** argv, const std::string& node_name) {
        rclcpp::init(argc, argv);
        node_ = std::make_shared<rclcpp::Node>(node_name);

        // 为每个已知槽位创建独立的发布者。
        yolo_pub_ = node_->create_publisher<eai_bot::msg::DetectionResult>(
            "/eai/detections/yolo", 10);
        scrfd_pub_ = node_->create_publisher<eai_bot::msg::DetectionResult>(
            "/eai/detections/scrfd", 10);

        // 输入订阅：外部节点通过 /eai/user_prompt 推送文本交予 LLM。
        prompt_sub_ = node_->create_subscription<std_msgs::msg::String>(
            "/eai/user_prompt", 10,
            [this](const std_msgs::msg::String::SharedPtr msg) {
                if (!input_handler_) {
                    return;
                }
                const std::string text = msg->data;
                if (text.empty()) {
                    return;
                }
                LogDebug("RosBridge: /eai/user_prompt received (%zu chars)", text.size());
                input_handler_(text);
            });

        LogInfo("RosBridge: node '%s' started, pubs /eai/detections/{yolo,scrfd} sub /eai/user_prompt",
                node_name.c_str());
    }

    ~Impl() {
        if (rclcpp::ok()) {
            LogInfo("RosBridge: shutting down");
            rclcpp::shutdown();
        }
    }

    void PublishTask(const InferenceTask& task) {
        if (task.frame_id == -1) {
            return;
        }

        for (const auto& slot_result : task.slot_results) {
            auto msg = BuildDetectionResult(
                task.frame_id,
                slot_result.slot,
                slot_result.result_json,
                task.merged_signals.person_present,
                task.merged_signals.face_detected);

            if (slot_result.slot == "yolo") {
                yolo_pub_->publish(std::move(msg));
            } else if (slot_result.slot == "scrfd") {
                scrfd_pub_->publish(std::move(msg));
            }
        }
    }

    void Install(Pipeline& pipeline) {
        pipeline.SetPostTaskHandler([this](InferenceTask& task) -> bool {
            if (task.frame_id == -1) {
                return false;
            }
            PublishTask(task);
            return true;
        });

        LogInfo("RosBridge: installed into pipeline");
    }

    void SetInputHandler(std::function<bool(const std::string&)> handler) {
        input_handler_ = std::move(handler);
    }

private:
    std::shared_ptr<rclcpp::Node> node_;
    std::shared_ptr<rclcpp::Publisher<eai_bot::msg::DetectionResult>> yolo_pub_;
    std::shared_ptr<rclcpp::Publisher<eai_bot::msg::DetectionResult>> scrfd_pub_;
    std::shared_ptr<rclcpp::Subscription<std_msgs::msg::String>> prompt_sub_;
    std::function<bool(const std::string&)> input_handler_;
};

// ---------------------------------------------------------------------------
// 公共接口：转发到 Impl。
// ---------------------------------------------------------------------------

RosBridge::RosBridge(int argc, char** argv, const std::string& node_name)
    : impl_(std::make_unique<Impl>(argc, argv, node_name)) {}

RosBridge::~RosBridge() = default;

void RosBridge::PublishTask(const InferenceTask& task) {
    impl_->PublishTask(task);
}

void RosBridge::Install(Pipeline& pipeline) {
    impl_->Install(pipeline);
}

void RosBridge::SetInputHandler(std::function<bool(const std::string&)> handler) {
    impl_->SetInputHandler(std::move(handler));
}

}  // namespace ros_bridge
}  // namespace eai