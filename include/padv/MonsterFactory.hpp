#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace padv {

struct MonsterFactory {
  static auto spawn(entt::registry& registry, glm::vec2 pos) -> entt::entity;
};

void create_monster(entt::registry& registry);

}  // namespace padv