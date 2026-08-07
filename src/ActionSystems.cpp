#include <algorithm>
#include <limits>

#include "padv/Systems.hpp"

namespace padv {

void finish_action(CurrentAction& current) {
  current.action   = Action::None;
  current.elapsed  = 0.0f;
  current.duration = 0.0f;
  current.executed = false;
}

void execution_system(entt::registry& registry) {
  auto view = registry.view<Command, CurrentAction, Stamina>(entt::exclude<Dead, Stunned>);

  for (auto entity : view) {
    auto& command = view.get<Command>(entity);
    auto& current = view.get<CurrentAction>(entity);
    auto& stamina = view.get<Stamina>(entity);

    // Shield Fix: Maintain shield if holding button, stop and trigger cooldown if button dropped or out of stamina
    if (current.action == Action::Shield) {
      if (command.requested_action == Action::Shield && stamina.current >= 0.5f) {
        stamina.current -= 0.5f;  // Drain stamina while maintaining
        command.requested_action = Action::None;
        continue;  // Keep shielding
      }

      // Stop shielding (either AI dropped command or ran out of stamina)
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

      case Action::Attack: {
        auto* skill = registry.try_get<AttackSkill>(entity);
        if (!skill) break;
        current.action   = Action::Attack;
        current.elapsed  = 0.0f;
        current.duration = skill->cast_time;
        current.executed = false;
        break;
      }

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
        break;
      }

      case Action::Shield: {
        auto* skill    = registry.try_get<ShieldSkill>(entity);
        auto* cooldown = registry.try_get<Cooldown<ShieldSkill>>(entity);

        if (!skill || !cooldown || cooldown->time_left > 0.0f || stamina.current < skill->stamina_cost_active) break;

        // Initial casting cost
        stamina.current -= skill->stamina_cost_active;

        current.action   = Action::Shield;
        current.elapsed  = 0.0f;
        current.duration = std::numeric_limits<F32>::max();  // Active indefinitely until stopped
        current.executed = false;
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
        break;
      }
    }

    command.requested_action = Action::None;
  }
}

void action_system(entt::registry& registry) {
  auto view = registry.view<CurrentAction>(entt::exclude<Dead>);

  for (auto entity : view) {
    auto& current = view.get<CurrentAction>(entity);

    if (current.action == Action::None) continue;

    ++current.elapsed;

    switch (current.action) {
      case Action::None:
      case Action::Shield:
        break;

      case Action::Attack: {
        if (current.elapsed < current.duration) break;

        const auto& transform = registry.get<Transform>(entity);
        const auto& skill     = registry.get<AttackSkill>(entity);

        if (registry.all_of<Knight>(entity)) {
          auto      targets  = registry.view<Monster, Transform, DamageQueue>(entt::exclude<Dead>);
          const F32 range_sq = skill.range * skill.range;

          for (auto target : targets) {
            const auto& target_transform = targets.get<Transform>(target);
            glm::vec2   delta            = target_transform.position - transform.position;
            if (glm::dot(delta, delta) > range_sq) continue;
            targets.get<DamageQueue>(target).hits.push_back({entity, skill.damage, glm::normalize(delta)});
          }
        } else {
          auto      targets  = registry.view<Knight, Transform, DamageQueue>(entt::exclude<Dead>);
          const F32 range_sq = skill.range * skill.range;

          for (auto target : targets) {
            const auto& target_transform = targets.get<Transform>(target);
            glm::vec2   delta            = target_transform.position - transform.position;
            if (glm::dot(delta, delta) > range_sq) continue;
            targets.get<DamageQueue>(target).hits.push_back({entity, skill.damage, glm::normalize(delta)});
          }
        }

        finish_action(current);
        break;
      }

      case Action::Cleave: {
        if (current.elapsed < current.duration) {
          break;
        }

        const auto& transform = registry.get<Transform>(entity);
        const auto& skill     = registry.get<CleaveSkill>(entity);
        const F32   range_sq  = skill.range * skill.range;

        if (registry.all_of<Knight>(entity)) {
          auto targets = registry.view<Monster, Transform, DamageQueue>(entt::exclude<Dead>);

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
        } else if (registry.all_of<Monster>(entity)) {
          auto targets = registry.view<Knight, Transform, DamageQueue>(entt::exclude<Dead>);

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
        auto        targets   = registry.view<Monster, Transform>();

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

void cooldown_system(entt::registry& registry) {
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

void floating_text_system(entt::registry& registry) {
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