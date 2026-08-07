#include "padv/MonsterFactory.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <random>

#include "padv/Components.hpp"

namespace padv {

auto MonsterFactory::spawn(entt::registry& registry, glm::vec2 pos) -> entt::entity {
  auto entity = registry.create();
  registry.emplace<Monster>(entity);
  registry.emplace<Transform>(entity, pos, glm::vec2(1.0f, 0.0f));
  registry.emplace<Command>(entity);
  registry.emplace<CurrentAction>(entity);
  registry.emplace<DamageQueue>(entity);
  registry.emplace<Audible>(entity);

  registry.emplace<Health>(entity, 50.0f, 50.0f);
  registry.emplace<Stamina>(entity, 100.0f, 100.0f);
  registry.emplace<Movement>(entity, 0.2f, 0.8f);
  registry.emplace<CleaveSkill>(entity, 8.0f, 3.5f, 0.85f, 15.0f, 0.0f, 40.0f);
  registry.emplace<Cooldown<CleaveSkill>>(entity, 0.0f);

  return entity;
}

void create_monster(entt::registry& registry) {
  auto knights = registry.view<Knight, Transform>(entt::exclude<Dead>);

  if (knights.begin() == knights.end()) return;

  glm::vec2 knight_pos = knights.get<Transform>(*knights.begin()).position;

  auto monsters = registry.view<Monster>(entt::exclude<Dead>);

  Size alive_count = static_cast<Size>(std::distance(monsters.begin(), monsters.end()));

  F32 min_radius = 15.0f + static_cast<F32>(alive_count) * 0.5f;
  F32 max_radius = min_radius + 5.0f;

  static std::mt19937 rng(std::random_device{}());

  std::uniform_real_distribution<F32> dist_angle(0.0f, 6.2831853f);
  std::uniform_real_distribution<F32> dist_radius(min_radius, max_radius);

  F32 angle  = dist_angle(rng);
  F32 radius = dist_radius(rng);

  glm::vec2 spawn_pos = knight_pos + glm::vec2(std::cos(angle), std::sin(angle)) * static_cast<float>(radius);

  spawn_pos.x = std::clamp(spawn_pos.x, -95.0f, 95.0f);
  spawn_pos.y = std::clamp(spawn_pos.y, -95.0f, 95.0f);

  auto entity = MonsterFactory::spawn(registry, spawn_pos);

  glm::vec2 to_knight       = knight_pos - spawn_pos;
  glm::vec2 initial_forward = glm::length(to_knight) > 0.0001f ? glm::normalize(to_knight) : glm::vec2(1.0f, 0.0f);

  registry.get<Transform>(entity).forward = initial_forward;
}

}  // namespace padv