#pragma once

#include <glm/glm.hpp>

#include "Components.hpp"

namespace padv {

class IPolicy {
 public:
  virtual ~IPolicy() = default;
  [[nodiscard]] virtual auto evaluate(const SensorData& sensors) -> Command = 0;
};

}  // namespace padv
