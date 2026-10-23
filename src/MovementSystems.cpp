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

namespace {

constexpr float ARENA_LIMIT = 98.0f;

}

auto movement_system(entt::registry& registry) -> void {
  auto view = registry.view<Transform, Movement, Command, ActionState>(entt::exclude<Dead, Stunned>);

  for (auto entity : view) {
    auto&       transform = view.get<Transform>(entity);
    const auto& movement  = view.get<Movement>(entity);
    const auto& command   = view.get<Command>(entity);
    const auto& current   = view.get<ActionState>(entity);
    bool        is_adv    = registry.all_of<Adventurer>(entity);

    if (current.action == Action::Warcry || current.action == Action::Cleave) {
      continue;
    }

    float move_sq = glm::dot(command.move, command.move);
    if (move_sq < 1e-6f) continue;

    glm::vec2 target_dir = command.move / std::sqrt(move_sq);

    float current_angle = std::atan2(transform.forward.y, transform.forward.x);
    float target_angle  = std::atan2(target_dir.y, target_dir.x);
    float angle_diff    = target_angle - current_angle;

    while (angle_diff > M_PI) angle_diff -= 2.0f * M_PI;
    while (angle_diff < -M_PI) angle_diff += 2.0f * M_PI;

    if (std::abs(angle_diff) <= movement.turn_speed) {
      transform.forward = target_dir;
    } else {
      current_angle += (angle_diff > 0.0f ? movement.turn_speed : -movement.turn_speed);
      transform.forward = glm::vec2(std::cos(current_angle), std::sin(current_angle));
    }

    float speed = static_cast<float>(movement.speed);

    if (current.action == Action::Shield) {
      if (auto* shield = registry.try_get<ShieldSkill>(entity)) speed *= static_cast<float>(shield->movement_modifier);
    }

    if (auto* slowed = registry.try_get<Slowed>(entity)) speed *= static_cast<float>(slowed->slow_modifier);

    float move_mag = std::min(1.0f, std::sqrt(move_sq));

    glm::vec2 proposed_pos = transform.position + target_dir * (speed * move_mag);
    proposed_pos.x         = std::clamp(proposed_pos.x, -ARENA_LIMIT, ARENA_LIMIT);
    proposed_pos.y         = std::clamp(proposed_pos.y, -ARENA_LIMIT, ARENA_LIMIT);

    bool collision    = false;
    auto all_entities = registry.view<Transform>(entt::exclude<Dead>);

    for (auto other : all_entities) {
      if (other == entity) continue;

      bool other_is_adv = registry.all_of<Adventurer>(other);
      float entity_min_dist = 1.3f;
      if (is_adv && other_is_adv) {
        entity_min_dist = 2.4f;
      } else if (is_adv || other_is_adv) {
        entity_min_dist = 1.8f;
      }
      float entity_min_dist_sq = entity_min_dist * entity_min_dist;

      float current_dist_sq  = glm::distance2(transform.position, registry.get<Transform>(other).position);
      float proposed_dist_sq = glm::distance2(proposed_pos, registry.get<Transform>(other).position);

      if (proposed_dist_sq < entity_min_dist_sq && proposed_dist_sq < current_dist_sq) {
        collision = true;
        break;
      }
    }

    if (!collision) {
      transform.position = proposed_pos;
    }
  }
}

auto collision_system(entt::registry& registry) -> void {
  auto view = registry.view<Transform>(entt::exclude<Dead>);

  for (auto entity : view) {
    auto& transform = view.get<Transform>(entity);
    transform.position.x = std::clamp(transform.position.x, -ARENA_LIMIT, ARENA_LIMIT);
    transform.position.y = std::clamp(transform.position.y, -ARENA_LIMIT, ARENA_LIMIT);
  }

  for (auto it1 = view.begin(); it1 != view.end(); ++it1) {
    auto  e1 = *it1;
    auto& t1 = view.get<Transform>(e1);
    bool  e1_is_adv = registry.all_of<Adventurer>(e1);

    for (auto it2 = std::next(it1); it2 != view.end(); ++it2) {
      auto  e2 = *it2;
      auto& t2 = view.get<Transform>(e2);
      bool  e2_is_adv = registry.all_of<Adventurer>(e2);

      float min_sep = 1.3f;
      if (e1_is_adv && e2_is_adv) {
        min_sep = 2.4f;
      } else if (e1_is_adv || e2_is_adv) {
        min_sep = 1.8f;
      }
      float min_sep_sq = min_sep * min_sep;

      glm::vec2 delta = t1.position - t2.position;
      float     d2    = glm::dot(delta, delta);

      if (d2 < min_sep_sq) {
        float     d       = std::sqrt(std::max(1e-4f, d2));
        glm::vec2 normal  = (d > 1e-4f) ? (delta / d) : glm::vec2(1.0f, 0.0f);
        float     overlap = 0.5f * (min_sep - d);

        t1.position += normal * overlap;
        t2.position -= normal * overlap;

        t1.position.x = std::clamp(t1.position.x, -ARENA_LIMIT, ARENA_LIMIT);
        t1.position.y = std::clamp(t1.position.y, -ARENA_LIMIT, ARENA_LIMIT);
        t2.position.x = std::clamp(t2.position.x, -ARENA_LIMIT, ARENA_LIMIT);
        t2.position.y = std::clamp(t2.position.y, -ARENA_LIMIT, ARENA_LIMIT);
      }
    }
  }
}

auto recovery_system(entt::registry& registry) -> void {
  auto view = registry.view<Health, HealthRegen, Stamina, StaminaRegen, ActionState, Command>(entt::exclude<Dead>);

  for (auto entity : view) {
    auto&       health        = view.get<Health>(entity);
    auto&       health_regen  = view.get<HealthRegen>(entity);
    auto&       stamina       = view.get<Stamina>(entity);
    auto&       stamina_regen = view.get<StaminaRegen>(entity);
    const auto& current       = view.get<ActionState>(entity);

    if (current.action == Action::None) {
      health.current  = std::min(health.max, health.current + health_regen.rate);
      stamina.current = std::min(stamina.max, stamina.current + stamina_regen.rate);
    }
  }
}

auto status_system(entt::registry& registry) -> void {
  for (auto entity : registry.view<Stunned>()) {
    auto& stunned = registry.get<Stunned>(entity);
    if (--stunned.time_left <= 0.0f) registry.remove<Stunned>(entity);
  }
  for (auto entity : registry.view<Slowed>()) {
    auto& slowed = registry.get<Slowed>(entity);
    if (--slowed.time_left <= 0.0f) registry.remove<Slowed>(entity);
  }
}

}
