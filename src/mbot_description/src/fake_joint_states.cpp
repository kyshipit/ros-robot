// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 KY (kyshipit)

// fake_joint_states.cpp —— 阶段 1 的静态 joint 状态源。
//
// 仅用于 RViz 纯显示：发布一次固定的 joint_states（左右轮置零），使
// robot_state_publisher 能构建完整 TF 树。真机/仿真阶段由 ros2_control 的
// joint_state_broadcaster 接管同一 topic，本节点不参与运行。

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = rclcpp::Node::make_shared("fake_joint_states");
  auto pub = node->create_publisher<sensor_msgs::msg::JointState>(
      "joint_states", rclcpp::QoS(10));

  sensor_msgs::msg::JointState msg;
  msg.header.stamp = node->now();
  msg.name = {"left_wheel_joint", "right_wheel_joint"};
  msg.position = {0.0, 0.0};
  msg.velocity = {0.0, 0.0};

  rclcpp::sleep_for(std::chrono::milliseconds(500));
  pub->publish(msg);
  RCLCPP_INFO(node->get_logger(), "published static joint_states");
  rclcpp::shutdown();
  return 0;
}