// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 KY (kyshipit)

// twist_to_stamped.cpp —— 把 teleop 的 Twist 转成 TwistStamped 转发给 diff_drive。
//
// 背景：diff_drive_controller（use_stamped_vel=true）只订阅 TwistStamped，
// 而 teleop_twist_keyboard 只发普通 Twist，类型不匹配导致按键无反应。
// 本节点订阅 /cmd_vel（Twist），加仿真时间戳转成 TwistStamped，发到
// /diff_drive_controller/cmd_vel，使键盘遥控能工作（方便手动建图）。
//
// 用法：
//   ros2 run teleop_twist_keyboard teleop_twist_keyboard \
//     --ros-args -r /cmd_vel:=/cmd_vel_raw
//   ros2 run mbot_gazebo twist_to_stamped

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = rclcpp::Node::make_shared("twist_to_stamped");

  auto pub = node->create_publisher<geometry_msgs::msg::TwistStamped>(
      "/diff_drive_controller/cmd_vel", rclcpp::QoS(10));

  auto sub = node->create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel_raw", rclcpp::QoS(10),
      [&pub, &node](geometry_msgs::msg::Twist::SharedPtr msg) {
        geometry_msgs::msg::TwistStamped out;
        out.header.stamp = node->now();
        out.header.frame_id = "";
        out.twist = *msg;
        pub->publish(out);
      });

  RCLCPP_INFO(node->get_logger(),
              "Relaying /cmd_vel_raw (Twist) -> /diff_drive_controller/cmd_vel (TwistStamped)");
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}