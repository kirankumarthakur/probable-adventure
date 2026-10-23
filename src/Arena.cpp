#include "padv/Arena.hpp"

#include <algorithm>
#include <entt/entt.hpp>

#include "padv/Systems.hpp"

namespace padv {

auto compute_fitness(const Arena& arena) -> F32 {
  F32 score = 0.0f;
  score += arena.damage_dealt * 15.0f;
  score += static_cast<F32>(arena.slimes_killed) * 600.0f;
  score += arena.damage_blocked * 6.0f;
  score += static_cast<F32>(arena.slimes_stunned) * 30.0f;

  float kill_multiplier = std::min(10.0f, 1.0f + static_cast<F32>(arena.slimes_killed) * 0.5f);
  score += static_cast<F32>(arena.ticks) * 0.05f * kill_multiplier;

  score += arena.center_control_ticks * 0.4f;
  score += arena.distance_traveled * 0.5f;

  score -= arena.damage_taken * 1.5f;
  score -= arena.wall_penalty_score;
  return std::max<F32>(0.0f, score);
}

auto build_adventurer(entt::registry& registry, const ModalPolicy& policy) -> void {
  auto adventurer = registry.create();

  registry.emplace<Adventurer>(adventurer);
  registry.emplace<Transform>(adventurer, glm::vec2(0.0f), glm::vec2(1.0f, 0.0f));
  registry.emplace<Health>(adventurer, 100.0f, 100.0f);
  registry.emplace<Stamina>(adventurer, 100.0f, 100.0f);
  registry.emplace<HealthRegen>(adventurer, 0.05f);
  registry.emplace<StaminaRegen>(adventurer, 0.5f);
  registry.emplace<Movement>(adventurer, 0.6f, 0.8f);
  registry.emplace<Command>(adventurer);
  registry.emplace<ActionState>(adventurer);
  registry.emplace<Perception>(adventurer, 100.0f, 0.0f, 50.0f);
  registry.emplace<SensorData>(adventurer);
  registry.emplace<DamageQueue>(adventurer);
  registry.emplace<ModalPolicy>(adventurer, policy);

  registry.emplace<CleaveSkill>(adventurer, 20.0f, 8.0f, 0.85f, 20.0f, 15.0f, 30.0f);
  registry.emplace<ShieldSkill>(adventurer, 15.0f, 0.5f, 0.5f, 5.0f, 50.0f);
  registry.emplace<WarcrySkill>(adventurer, 30.0f, 15.0f, 25.0f, 100.0f, 150.0f, 0.5f, 40.0f, 300.0f);
  registry.emplace<Cooldown<CleaveSkill>>(adventurer, 0.0f);
  registry.emplace<Cooldown<ShieldSkill>>(adventurer, 0.0f);
  registry.emplace<Cooldown<WarcrySkill>>(adventurer, 0.0f);
}

auto step_simulation(entt::registry& registry, Arena& arena, Action manual_override) -> void {
  if (arena.game_over) return;

  perception_system(registry);
  slime_ai_system(registry);
  adventurer_ai_system(registry);
  if (manual_override != Action::None) {
    auto adv_view = registry.view<Adventurer, Command>(entt::exclude<Dead>);
    for (auto e : adv_view) {
      adv_view.get<Command>(e).requested_action = manual_override;
    }
  }
  execution_system(registry, arena);
  movement_system(registry);
  collision_system(registry);
  action_system(registry);
  damage_system(registry, arena);
  death_system(registry, arena);
  if (arena.game_over) return;

  spawn_system(registry, arena);
  cooldown_system(registry);
  recovery_system(registry);
  status_system(registry);
  floating_text_system(registry);

  auto advs = registry.view<Adventurer, Transform>(entt::exclude<Dead>);
  for (auto e : advs) {
    const auto& pos = advs.get<Transform>(e).position;

    float dist_center = glm::length(pos);
    if (dist_center <= 50.0f) {
      arena.center_control_ticks += 1.0f;
    }

    float d_wall = std::min(100.0f - std::abs(pos.x), 100.0f - std::abs(pos.y));
    if (d_wall < 30.0f) {
      arena.wall_hug_ticks++;
      arena.wall_penalty_score += (30.0f - d_wall) * 0.8f;
    }

    if (auto* last = registry.try_get<LastPosition>(e)) {
      arena.distance_traveled += glm::distance(pos, last->pos);
      last->pos = pos;
    } else {
      registry.emplace<LastPosition>(e, pos);
    }
  }

  ++arena.ticks;
}

auto run_arena_trial(const ModalPolicy& policy, I64 max_ticks, Arena* out_arena) -> F32 {
  entt::registry registry;
  Arena          arena;

  build_adventurer(registry, policy);

  while (!arena.game_over && arena.ticks < max_ticks) {
    step_simulation(registry, arena);
  }

  if (out_arena) {
    *out_arena = arena;
  }

  return compute_fitness(arena);
}

}  // namespace padv
