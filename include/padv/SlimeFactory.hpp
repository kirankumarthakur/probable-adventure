#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "Components.hpp"

namespace padv {

struct SlimeFactory {
  static auto spawn(entt::registry& registry, glm::vec2 pos) -> entt::entity;
};

auto create_slime(entt::registry& registry) -> void;

}  // namespace padv
