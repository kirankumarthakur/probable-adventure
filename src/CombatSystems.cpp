#include <algorithm>
#include <random>

#include <glm/glm.hpp>

#include "padv/Systems.hpp"

namespace padv {

auto damage_system(entt::registry& registry, Arena& arena) -> void {
  auto view = registry.view<Transform, Health, DamageQueue, ActionState>(entt::exclude<Dead>);

  thread_local static std::mt19937     rng(std::random_device{}());
  std::uniform_real_distribution<float> rand_vel(-0.1f, 0.1f);

  for (auto entity : view) {
    const auto& transform    = view.get<Transform>(entity);
    auto&       health       = view.get<Health>(entity);
    auto&       queue        = view.get<DamageQueue>(entity);
    const auto& action       = view.get<ActionState>(entity);
    bool        is_adventurer = registry.all_of<Adventurer>(entity);

    for (const auto& hit : queue.hits) {
      F32 damage     = hit.damage;
      F32 dot_source = glm::dot(transform.forward, hit.source_dir);

      if (dot_source > 0.0f) {
        damage *= (1.0f + 0.5f * dot_source);
      }

      F32 blocked_amount = 0.0f;
      if (action.action == Action::Shield) {
        auto* shield = registry.try_get<ShieldSkill>(entity);
        if (shield && -dot_source >= shield->fov_cos) {
          blocked_amount = std::min(damage, shield->armor);
          damage -= blocked_amount;
        }
      }

      health.current -= damage;
      if (health.current < 0.0f) health.current = 0.0f;

      if (is_adventurer) {
        arena.damage_blocked += blocked_amount;
        arena.damage_taken += damage;
      } else {
        arena.damage_dealt += damage;
      }

      if (blocked_amount > 0.1f) {
        auto txt = registry.create();
        registry.emplace<FloatingText>(txt, blocked_amount,
                                       transform.position + glm::vec2(rand_vel(rng), rand_vel(rng)),
                                       glm::vec2(0.0f, -0.05f), 30.0f, 1);
      }

      if (damage > 0.1f) {
        auto txt = registry.create();
        registry.emplace<FloatingText>(txt, damage, transform.position + glm::vec2(rand_vel(rng), rand_vel(rng)),
                                       glm::vec2(0.0f, -0.1f), 30.0f, 0);
      }
    }

    queue.hits.clear();
  }
}

auto death_system(entt::registry& registry, Arena& arena) -> void {
  auto view = registry.view<Health>(entt::exclude<Dead>);

  for (auto entity : view) {
    if (view.get<Health>(entity).current > 0.0f) continue;

    registry.emplace<Dead>(entity);

    if (registry.all_of<Adventurer>(entity)) {
      arena.game_over = true;
    } else if (registry.all_of<Slime>(entity)) {
      ++arena.slimes_killed;
      for (auto adv : registry.view<Adventurer, Health>()) {
        auto& h = registry.get<Health>(adv);
        h.current = std::min(h.max, h.current + 15.0f);
      }
    }
  }
}

}  // namespace padv
