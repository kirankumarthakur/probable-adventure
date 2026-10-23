#include <algorithm>
#include <limits>

#include "padv/Systems.hpp"

namespace padv {

auto finish_action(ActionState& current) -> void {
  current.action   = Action::None;
  current.elapsed  = 0.0f;
  current.duration = 0.0f;
  current.executed = false;
}

auto execution_system(entt::registry& registry, Arena& arena) -> void {
  auto view = registry.view<Command, ActionState, Stamina>(entt::exclude<Dead, Stunned>);

  for (auto entity : view) {
    auto& command = view.get<Command>(entity);
    auto& current = view.get<ActionState>(entity);
    auto& stamina = view.get<Stamina>(entity);

    if (current.action == Action::Shield) {
      if (command.requested_action == Action::Shield && stamina.current >= 0.5f) {
        stamina.current -= 0.5f;
        command.requested_action = Action::None;
        continue;
      }

      if (auto* skill = registry.try_get<ShieldSkill>(entity)) {
        if (auto* cooldown = registry.try_get<Cooldown<ShieldSkill>>(entity)) {
          cooldown->time_left = skill->cooldown;
        }
      }
      finish_action(current);
      command.requested_action = Action::None;
      continue;
    }

    if (current.action != Action::None) {
      command.requested_action = Action::None;
      continue;
    }

    switch (command.requested_action) {
      case Action::None:
        break;

      case Action::Cleave: {
        auto* skill    = registry.try_get<CleaveSkill>(entity);
        auto* cooldown = registry.try_get<Cooldown<CleaveSkill>>(entity);

        if (!skill || !cooldown || cooldown->time_left > 0.0f || stamina.current < skill->stamina_cost) break;

        stamina.current -= skill->stamina_cost;
        cooldown->time_left = skill->cooldown;

        current.action   = Action::Cleave;
        current.elapsed  = 0.0f;
        current.duration = skill->cast_time;
        current.executed = false;
        if (registry.all_of<Adventurer>(entity)) {
          arena.cleave_count++;
        }
        break;
      }

      case Action::Shield: {
        auto* skill    = registry.try_get<ShieldSkill>(entity);
        auto* cooldown = registry.try_get<Cooldown<ShieldSkill>>(entity);

        if (!skill || !cooldown || cooldown->time_left > 0.0f || stamina.current < skill->stamina_cost_active) break;

        stamina.current -= skill->stamina_cost_active;

        current.action   = Action::Shield;
        current.elapsed  = 0.0f;
        current.duration = std::numeric_limits<F32>::max();
        current.executed = false;
        if (registry.all_of<Adventurer>(entity)) {
          arena.shield_count++;
        }
        break;
      }

      case Action::Warcry: {
        auto* skill    = registry.try_get<WarcrySkill>(entity);
        auto* cooldown = registry.try_get<Cooldown<WarcrySkill>>(entity);

        if (!skill || !cooldown || cooldown->time_left > 0.0f || stamina.current < skill->stamina_cost) break;

        stamina.current -= skill->stamina_cost;
        cooldown->time_left = skill->cooldown;

        current.action   = Action::Warcry;
        current.elapsed  = 0.0f;
        current.duration = skill->cast_time;
        current.executed = false;
        if (registry.all_of<Adventurer>(entity)) {
          arena.warcry_count++;
        }
        break;
      }
    }

    command.requested_action = Action::None;
  }
}

auto action_system(entt::registry& registry) -> void {
  auto view = registry.view<ActionState>(entt::exclude<Dead>);

  for (auto entity : view) {
    auto& current = view.get<ActionState>(entity);

    if (current.action == Action::None) continue;

    ++current.elapsed;

    switch (current.action) {
      case Action::None:
      case Action::Shield:
        break;

      case Action::Cleave: {
        if (current.elapsed < current.duration) {
          break;
        }

        const auto& transform = registry.get<Transform>(entity);
        const auto& skill     = registry.get<CleaveSkill>(entity);
        const F32   range_sq  = skill.range * skill.range;

        if (registry.all_of<Adventurer>(entity)) {
          auto targets = registry.view<Slime, Transform, DamageQueue>(entt::exclude<Dead>);

          for (auto target : targets) {
            const auto& target_transform = targets.get<Transform>(target);
            glm::vec2   delta            = target_transform.position - transform.position;

            if (glm::dot(delta, delta) > range_sq) {
              continue;
            }

            glm::vec2 dir = glm::normalize(delta);
            if (glm::dot(transform.forward, dir) < skill.fov_cos) {
              continue;
            }

            targets.get<DamageQueue>(target).hits.push_back({entity, skill.damage, dir});
          }
        } else if (registry.all_of<Slime>(entity)) {
          auto targets = registry.view<Adventurer, Transform, DamageQueue>(entt::exclude<Dead>);

          for (auto target : targets) {
            const auto& target_transform = targets.get<Transform>(target);
            glm::vec2   delta            = target_transform.position - transform.position;

            if (glm::dot(delta, delta) > range_sq) continue;

            glm::vec2 dir = glm::normalize(delta);
            if (glm::dot(transform.forward, dir) < skill.fov_cos) continue;

            targets.get<DamageQueue>(target).hits.push_back({entity, skill.damage, dir});
          }
        }

        finish_action(current);
        break;
      }

      case Action::Warcry: {
        if (current.elapsed < current.duration) break;

        const auto& transform = registry.get<Transform>(entity);
        const auto& skill     = registry.get<WarcrySkill>(entity);
        auto        targets   = registry.view<Slime, Transform>();

        const F32 stun_radius_sq = skill.stun_radius * skill.stun_radius;
        const F32 slow_radius_sq = skill.slow_radius * skill.slow_radius;

        for (auto target : targets) {
          const auto& target_transform = targets.get<Transform>(target);
          glm::vec2   delta            = target_transform.position - transform.position;
          F32         dist_sq          = glm::dot(delta, delta);

          if (dist_sq <= stun_radius_sq) {
            registry.emplace_or_replace<Stunned>(target, skill.stun_duration);
          }

          if (dist_sq <= slow_radius_sq) {
            registry.emplace_or_replace<Slowed>(target, skill.slow_duration, skill.slow_modifier);
          }
        }

        finish_action(current);
        break;
      }
    }
  }
}

auto cooldown_system(entt::registry& registry) -> void {
  for (auto entity : registry.view<Cooldown<CleaveSkill>>()) {
    auto& cooldown = registry.get<Cooldown<CleaveSkill>>(entity);
    if (cooldown.time_left > 0.0f) --cooldown.time_left;
  }

  for (auto entity : registry.view<Cooldown<ShieldSkill>>()) {
    auto& cooldown = registry.get<Cooldown<ShieldSkill>>(entity);
    if (cooldown.time_left > 0.0f) --cooldown.time_left;
  }

  for (auto entity : registry.view<Cooldown<WarcrySkill>>()) {
    auto& cooldown = registry.get<Cooldown<WarcrySkill>>(entity);
    if (cooldown.time_left > 0.0f) --cooldown.time_left;
  }
}

auto floating_text_system(entt::registry& registry) -> void {
  auto                      view = registry.view<FloatingText>();
  std::vector<entt::entity> to_delete;

  for (auto entity : view) {
    auto& txt = view.get<FloatingText>(entity);
    txt.time_left -= 1.0f;
    txt.position += txt.velocity;

    if (txt.time_left <= 0.0f) {
      to_delete.push_back(entity);
    }
  }

  for (auto e : to_delete) {
    registry.destroy(e);
  }
}

}  // namespace padv
