// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 KY (kyshipit)

// fake_joint_states.cpp —— 阶段 1 的静态 joint 状态源。
//
// 用于 RViz 纯显示：持续发布 joint_states（左右轮置零），使
// robot_state_publisher 能持续构建 TF 树，尤其让 continuous 关节（左右轮）
// 的 TF 也能发布。真机/仿真阶段由 ros2_control 的 joint_state_broadcaster
// 接管同一 topic，本节点不参与运行。
//
// 要点：
//   - 用 reliable + transient_local QoS，保证 RSP 晚订阅也能收到最近一条；
//   - 启动后先短暂等待，再开始周期发布，避免与 RSP 订阅建立竞态。

#include <chrono>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = rclcpp::Node::make_shared("fake_joint_states");

  auto qos = rclcpp::QoS(10).reliable().transient_local();
  auto pub = node->create_publisher<sensor_msgs::msg::JointState>(
      "joint_states", qos);

  sensor_msgs::msg::JointState msg;
  msg.name = {"left_wheel_joint", "right_wheel_joint"};
  msg.position = {0.0, 0.0};
  msg.velocity = {0.0, 0.0};

  auto timer = node->create_wall_timer(
      std::chrono::milliseconds(100), [&pub, &msg, &node]() {
        msg.header.stamp = node->now();
        pub->publish(msg);
      });

  RCLCPP_INFO(node->get_logger(), "publishing static joint_states @10Hz");
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}