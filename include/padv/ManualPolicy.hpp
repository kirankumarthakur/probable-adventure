#pragma once

#include "IPolicy.hpp"

namespace padv {

class ManualPolicy final : public IPolicy {
 public:
  glm::vec2 movement_input{0.0f, 0.0f};
  Action    action_input{Action::None};

  auto set_input(glm::vec2 move, Action act) -> void {
    movement_input = move;
    action_input   = act;
  }

  [[nodiscard]] auto evaluate(const SensorData&) -> Command override {
    Command cmd;
    cmd.move             = movement_input;
    cmd.requested_action = action_input;
    return cmd;
  }
};

}  // namespace padv
