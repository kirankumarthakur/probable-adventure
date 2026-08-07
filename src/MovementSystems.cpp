#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "padv/Systems.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace padv {

void movement_system(entt::registry& registry) {
  auto view = registry.view<Transform, Movement, Command, CurrentAction>(entt::exclude<Dead, Stunned>);

  constexpr float min_dist    = 1.5f;
  constexpr float min_dist_sq = min_dist * min_dist;

  for (auto entity : view) {
    auto&       transform = view.get<Transform>(entity);
    const auto& movement  = view.get<Movement>(entity);
    const auto& command   = view.get<Command>(entity);
    const auto& current   = view.get<CurrentAction>(entity);

    if (current.action == Action::Warcry || current.action == Action::Attack || current.action == Action::Cleave) {
      continue;
    }

    float move_sq = glm::dot(command.move, command.move);
    if (move_sq < 1e-6f) continue;

    glm::vec2 target_dir = command.move / std::sqrt(move_sq);

    float current_angle = std::atan2(transform.forward.y, transform.forward.x);
    float target_angle  = std::atan2(target_dir.y, target_dir.x);
    float angle_diff    = target_angle - current_angle;

    // Normalize angle to [-PI, PI] to find the shortest rotation path
    while (angle_diff > M_PI) angle_diff -= 2.0f * M_PI;
    while (angle_diff < -M_PI) angle_diff += 2.0f * M_PI;

    if (std::abs(angle_diff) <= movement.turn_speed) {
      transform.forward = target_dir;
    } else {
      current_angle += (angle_diff > 0.0f ? movement.turn_speed : -movement.turn_speed);
      transform.forward = glm::vec2(std::cos(current_angle), std::sin(current_angle));
    }

    // Move forward
    float speed = static_cast<float>(movement.speed);

    if (current.action == Action::Shield) {
      if (auto* shield = registry.try_get<ShieldSkill>(entity)) speed *= static_cast<float>(shield->movement_modifier);
    }

    if (auto* slowed = registry.try_get<Slowed>(entity)) speed *= static_cast<float>(slowed->slow_modifier);

    float move_mag = std::min(1.0f, std::sqrt(move_sq));

    glm::vec2 proposed_pos = transform.position + target_dir * (speed * move_mag);
    proposed_pos.x         = std::clamp(proposed_pos.x, -100.0f, 100.0f);
    proposed_pos.y         = std::clamp(proposed_pos.y, -100.0f, 100.0f);

    bool collision    = false;
    auto all_entities = registry.view<Transform>(entt::exclude<Dead>);

    for (auto other : all_entities) {
      if (other == entity) continue;

      float current_dist_sq  = glm::distance2(transform.position, registry.get<Transform>(other).position);
      float proposed_dist_sq = glm::distance2(proposed_pos, registry.get<Transform>(other).position);

      if (proposed_dist_sq < min_dist_sq && proposed_dist_sq < current_dist_sq) {
        collision = true;
        break;
      }
    }

    if (!collision) {
      transform.position = proposed_pos;
    }
  }
}

void collision_system(entt::registry& registry) {
  (void)registry;  
}

void recovery_system(entt::registry& registry) {
  auto view = registry.view<Health, HealthRegen, Stamina, StaminaRegen, CurrentAction, Command>(entt::exclude<Dead>);

  for (auto entity : view) {
    auto&       health        = view.get<Health>(entity);
    auto&       health_regen  = view.get<HealthRegen>(entity);
    auto&       stamina       = view.get<Stamina>(entity);
    auto&       stamina_regen = view.get<StaminaRegen>(entity);
    const auto& current       = view.get<CurrentAction>(entity);
    const auto& command       = view.get<Command>(entity);

    if (current.action == Action::None && glm::dot(command.move, command.move) < 1e-6f) {
      health.current = std::min(health.max, health.current + health_regen.rate);
    }
    if (current.action == Action::None) {
      stamina.current = std::min(stamina.max, stamina.current + stamina_regen.rate);
    }
  }
}

void status_system(entt::registry& registry) {
  for (auto entity : registry.view<Stunned>()) {
    auto& stunned = registry.get<Stunned>(entity);
    if (--stunned.time_left <= 0.0f) registry.remove<Stunned>(entity);
  }
  for (auto entity : registry.view<Slowed>()) {
    auto& slowed = registry.get<Slowed>(entity);
    if (--slowed.time_left <= 0.0f) registry.remove<Slowed>(entity);
  }
}

}  // namespace padv