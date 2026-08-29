// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 KY (kyshipit)

/*
 * ros_bridge/detection_msg_builder.h
 *
 * 纯函数：解析各适配器 Postprocess 输出的行文本格式，填充 ROS2 消息。
 *
 * 输入格式（与 display/result_overlay.cpp 解析逻辑一致）：
 *   YOLO:  "label x1 y1 x2 y2 score\n"
 *   SCRFD: "face x1 y1 x2 y2 score kp1x kp1y kp2x kp2y kp3x kp3y kp4x kp4y kp5x kp5y\n"
 *
 * 不依赖 rclcpp（仅依赖生成的消息头），可独立单测。
 */
#pragma once

#include <sstream>
#include <string>

#include "eai_bot/msg/box.hpp"
#include "eai_bot/msg/detection_result.hpp"
#include "eai_bot/msg/point2_d.hpp"

namespace eai {
namespace ros_bridge {

inline eai_bot::msg::Box BuildBoxFromLine(const std::string& line) {
    eai_bot::msg::Box box;
    std::istringstream ss(line);
    std::string label;
    int x1, y1, x2, y2;
    float score;

    if (!(ss >> label >> x1 >> y1 >> x2 >> y2 >> score)) {
        // 解析失败，返回空 box（调用方可用 label 为空判断）。
        return box;
    }

    box.label = label;
    box.x1 = x1;
    box.y1 = y1;
    box.x2 = x2;
    box.y2 = y2;
    box.score = score;

    // 尝试读取 5 个关键点（SCRFD 格式）。
    for (int k = 0; k < 5; ++k) {
        int kx = 0, ky = 0;
        if (ss >> kx >> ky) {
            eai_bot::msg::Point2D pt;
            pt.x = static_cast<float>(kx);
            pt.y = static_cast<float>(ky);
            box.keypoints.push_back(pt);
        } else {
            break;
        }
    }

    return box;
}

inline eai_bot::msg::DetectionResult BuildDetectionResult(
    int frame_id,
    const std::string& slot,
    const std::string& result_json,
    bool person_present,
    bool face_detected) {

    eai_bot::msg::DetectionResult msg;
    msg.frame_id = frame_id;
    msg.slot = slot;
    msg.person_present = person_present;
    msg.face_detected = face_detected;

    if (result_json.empty()) {
        return msg;
    }

    std::istringstream ss(result_json);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) {
            continue;
        }
        auto box = BuildBoxFromLine(line);
        if (!box.label.empty()) {
            msg.boxes.push_back(std::move(box));
        }
    }

    return msg;
}

}  // namespace ros_bridge
}  // namespace eai