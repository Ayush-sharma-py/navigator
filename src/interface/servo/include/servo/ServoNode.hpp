/*
 * Package:   servo
 * Filename:  include/servo/ServoNode
 * Author:    Joshua Williams
 * Email:     joshmackwilliams@protonmail.com
 * Copyright: 2022, Nova UTD
 * License:   MIT License
 */

#pragma once

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/float32.hpp"
#include "servo/servo_params.hpp"
#include "std_msgs/msg/bool.hpp"

namespace navigator {
namespace servo {

class ServoNode final : public rclcpp::Node {
public:
  ServoNode();
  ServoNode(servo_params params);

private:
  void new_position(std_msgs::msg::Float32::SharedPtr message);
  void enable(std_msgs::msg::Bool::SharedPtr message);
  void init();

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher;
  rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr subscription;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr enable_subscription;
  servo_params params;

  std::chrono::time_point<std::chrono::system_clock> last_enabled;
  bool enabled = false;
};

}
}
