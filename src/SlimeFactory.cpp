#include "padv/SlimeFactory.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <random>

#include "padv/Components.hpp"

namespace padv {

auto SlimeFactory::spawn(entt::registry& registry, glm::vec2 pos) -> entt::entity {
  thread_local std::mt19937           rng(std::random_device{}());
  std::uniform_real_distribution<F32> dist_angle(0.0f, 6.2831853f);
  std::uniform_real_distribution<F32> dist_pref(3.5f, 7.0f);

  auto entity = registry.create();
  registry.emplace<Slime>(entity);
  registry.emplace<Transform>(entity, pos, glm::vec2(1.0f, 0.0f));
  registry.emplace<Command>(entity);
  registry.emplace<ActionState>(entity);
  registry.emplace<DamageQueue>(entity);
  registry.emplace<Audible>(entity);

  registry.emplace<SlimeBehavior>(entity, SlimeState::Approach, dist_angle(rng), dist_pref(rng), 0.0f);

  registry.emplace<Health>(entity, 50.0f, 50.0f);
  registry.emplace<Stamina>(entity, 100.0f, 100.0f);
  registry.emplace<Movement>(entity, 0.2f, 0.8f);
  registry.emplace<CleaveSkill>(entity, 8.0f, 4.2f, 0.5f, 15.0f, 0.0f, 35.0f);
  registry.emplace<Cooldown<CleaveSkill>>(entity, 0.0f);

  return entity;
}

auto create_slime(entt::registry& registry) -> void {
  auto adventurers = registry.view<Adventurer, Transform>(entt::exclude<Dead>);

  if (adventurers.begin() == adventurers.end()) return;

  glm::vec2 adv_pos = adventurers.get<Transform>(*adventurers.begin()).position;

  auto slimes = registry.view<Slime>(entt::exclude<Dead>);

  Size alive_count = static_cast<Size>(std::distance(slimes.begin(), slimes.end()));

  F32 min_radius = 15.0f + static_cast<F32>(alive_count) * 0.5f;
  F32 max_radius = min_radius + 5.0f;

  thread_local std::mt19937 rng(std::random_device{}());

  std::uniform_real_distribution<F32> dist_angle(0.0f, 6.2831853f);
  std::uniform_real_distribution<F32> dist_radius(min_radius, max_radius);

  F32 angle  = dist_angle(rng);
  F32 radius = dist_radius(rng);

  glm::vec2 spawn_pos = adv_pos + glm::vec2(std::cos(angle), std::sin(angle)) * static_cast<float>(radius);
  if (spawn_pos.x < -92.0f || spawn_pos.x > 92.0f || spawn_pos.y < -92.0f || spawn_pos.y > 92.0f) {
    glm::vec2 to_center = -adv_pos;
    if (glm::length(to_center) > 1.0f) {
      to_center = glm::normalize(to_center);
      spawn_pos = adv_pos + to_center * radius;
    }
  }

  spawn_pos.x = std::clamp(spawn_pos.x, -92.0f, 92.0f);
  spawn_pos.y = std::clamp(spawn_pos.y, -92.0f, 92.0f);

  auto entity = SlimeFactory::spawn(registry, spawn_pos);

  glm::vec2 to_adv = adv_pos - spawn_pos;
  glm::vec2 initial_forward = glm::length(to_adv) > 0.0001f ? glm::normalize(to_adv) : glm::vec2(1.0f, 0.0f);

  registry.get<Transform>(entity).forward = initial_forward;
}

}  // namespace padv
