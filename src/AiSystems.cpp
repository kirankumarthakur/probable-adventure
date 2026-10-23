#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "padv/ModalPolicy.hpp"
#include "padv/SpatialGrid2D.hpp"
#include "padv/Systems.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace padv {

auto perception_system(entt::registry& registry) -> void {
  auto adventurers = registry.view<Adventurer, Transform, Health, Stamina, Perception, SensorData>(entt::exclude<Dead>);
  auto slimes      = registry.view<Slime, Transform>(entt::exclude<Dead>);

  constexpr int   NUM_VISION_RAYS = 11;
  constexpr int   NUM_SOUND_RAYS  = 8;
  constexpr float SLIME_RADIUS    = 1.5f;

  for (auto adv : adventurers) {
    auto&       transform  = adventurers.get<Transform>(adv);
    const auto& health     = adventurers.get<Health>(adv);
    const auto& stamina    = adventurers.get<Stamina>(adv);
    auto&       perception = adventurers.get<Perception>(adv);
    auto&       data       = adventurers.get<SensorData>(adv);

    data.hp_ratio      = health.max > 0.0f ? (health.current / health.max) : 0.0f;
    data.stamina_ratio = stamina.max > 0.0f ? (stamina.current / stamina.max) : 0.0f;

    auto* cd_cleave = registry.try_get<Cooldown<CleaveSkill>>(adv);
    auto* cd_shield = registry.try_get<Cooldown<ShieldSkill>>(adv);
    auto* cd_warcry = registry.try_get<Cooldown<WarcrySkill>>(adv);

    data.cleave_cd = (cd_cleave && cd_cleave->time_left > 0.0f) ? 0.0f : 1.0f;
    data.shield_cd = (cd_shield && cd_shield->time_left > 0.0f) ? 0.0f : 1.0f;
    data.warcry_cd = (cd_warcry && cd_warcry->time_left > 0.0f) ? 0.0f : 1.0f;

    data.is_debuffed = (registry.all_of<Stunned>(adv) || registry.all_of<Slowed>(adv)) ? 1.0f : 0.0f;

    constexpr float WARNING_DIST = 25.0f;
    float           dist_north   = transform.position.y - (-100.0f);
    float           dist_south   = 100.0f - transform.position.y;
    float           dist_east    = 100.0f - transform.position.x;
    float           dist_west    = transform.position.x - (-100.0f);

    data.wall_north = std::clamp(1.0f - dist_north / WARNING_DIST, 0.0f, 1.0f);
    data.wall_south = std::clamp(1.0f - dist_south / WARNING_DIST, 0.0f, 1.0f);
    data.wall_east  = std::clamp(1.0f - dist_east / WARNING_DIST, 0.0f, 1.0f);
    data.wall_west  = std::clamp(1.0f - dist_west / WARNING_DIST, 0.0f, 1.0f);

    data.vision_rays.clear();
    data.sound_rays.clear();

    float fwd_angle      = std::atan2(transform.forward.y, transform.forward.x);
    float fov_half_angle = std::acos(perception.vision_cos);

    float start_angle = fwd_angle - fov_half_angle;
    float angle_step  = (2.0f * fov_half_angle) / std::max(1, NUM_VISION_RAYS - 1);

    for (int i = 0; i < NUM_VISION_RAYS; ++i) {
      float     ray_angle = start_angle + i * angle_step;
      glm::vec2 ray_dir(std::cos(ray_angle), std::sin(ray_angle));

      float closest_dist = perception.vision_radius;
      bool  hit_anything = false;

      for (auto slime : slimes) {
        auto&     s_trans = slimes.get<Transform>(slime);
        glm::vec2 L       = s_trans.position - transform.position;

        float tca = glm::dot(L, ray_dir);
        if (tca < 0 || tca > perception.vision_radius + SLIME_RADIUS) continue;

        float d2      = glm::dot(L, L) - tca * tca;
        float radius2 = SLIME_RADIUS * SLIME_RADIUS;
        if (d2 > radius2) continue;

        float thc      = std::sqrt(radius2 - d2);
        float t0       = tca - thc;
        float t1       = tca + thc;
        float hit_dist = t0 < 0.0f ? t1 : t0;

        if (hit_dist < closest_dist && hit_dist > 0.0f) {
          closest_dist = hit_dist;
          hit_anything = true;
        }
      }
      data.vision_rays.push_back({ray_dir, closest_dist, hit_anything});
    }

    float sound_angle_step = (2.0f * M_PI) / NUM_SOUND_RAYS;
    for (int i = 0; i < NUM_SOUND_RAYS; ++i) {
      float     ray_angle = i * sound_angle_step;
      glm::vec2 ray_dir(std::cos(ray_angle), std::sin(ray_angle));

      bool hit_anything = false;
      for (auto slime : slimes) {
        if (!registry.all_of<Audible>(slime)) continue;

        auto&     s_trans = slimes.get<Transform>(slime);
        glm::vec2 L       = s_trans.position - transform.position;

        float tca = glm::dot(L, ray_dir);
        if (tca < 0 || tca > perception.hearing_radius + SLIME_RADIUS) continue;

        float d2      = glm::dot(L, L) - tca * tca;
        float radius2 = SLIME_RADIUS * SLIME_RADIUS;

        if (d2 <= radius2) {
          float thc = std::sqrt(radius2 - d2);
          float t0  = tca - thc;
          if (t0 <= perception.hearing_radius) {
            hit_anything = true;
            break;
          }
        }
      }
      data.sound_rays.push_back({ray_dir, perception.hearing_radius, hit_anything});
    }
  }
}

auto adventurer_ai_system(entt::registry& registry) -> void {
  auto view = registry.view<ModalPolicy, Command, SensorData>(entt::exclude<Dead>);
  for (auto entity : view) {
    auto&       policy  = view.get<ModalPolicy>(entity);
    auto&       command = view.get<Command>(entity);
    const auto& sensors = view.get<SensorData>(entity);
    command             = policy.evaluate(sensors);
  }
}

auto slime_ai_system(entt::registry& registry) -> void {
  auto adventurers = registry.view<Adventurer, Transform>(entt::exclude<Dead>);
  if (adventurers.begin() == adventurers.end()) return;

  glm::vec2 adv_pos = adventurers.get<Transform>(*adventurers.begin()).position;
  auto      slimes  = registry.view<Slime, Transform, Command, CleaveSkill>(entt::exclude<Dead>);

  SpatialGrid2D grid(6.0f);
  for (auto entity : slimes) {
    grid.insert(entity, slimes.get<Transform>(entity).position);
  }

  std::vector<entt::entity> neighbors;
  neighbors.reserve(16);

  for (auto entity : slimes) {
    auto&       transform = slimes.get<Transform>(entity);
    auto&       command   = slimes.get<Command>(entity);
    const auto& skill     = slimes.get<CleaveSkill>(entity);
    auto*       behavior  = registry.try_get<SlimeBehavior>(entity);

    if (registry.all_of<Stunned>(entity)) {
      command.move             = glm::vec2(0.0f);
      command.requested_action = Action::None;
      if (behavior) behavior->state = SlimeState::Stunned;
      continue;
    }

    glm::vec2 delta   = adv_pos - transform.position;
    float     dist_sq = glm::dot(delta, delta);
    float     dist    = std::sqrt(std::max(0.0001f, dist_sq));

    glm::vec2 direction(1.0f, 0.0f);
    if (dist > 0.0001f) direction = delta / dist;

    auto* cooldown = registry.try_get<Cooldown<CleaveSkill>>(entity);
    bool  cd_ready = (!cooldown || cooldown->time_left <= 0.0f);

    if (dist <= skill.range && cd_ready) {
      if (behavior) behavior->state = SlimeState::Pounce;
      command.requested_action = Action::Cleave;
      command.move             = glm::vec2(0.0f);
    } else if (dist <= 8.0f && behavior && std::abs(adv_pos.x) < 80.0f && std::abs(adv_pos.y) < 80.0f) {
      behavior->state = SlimeState::Encircle;
      glm::vec2 target_pos =
          adv_pos +
          glm::vec2(std::cos(behavior->encircle_angle), std::sin(behavior->encircle_angle)) *
              static_cast<float>(behavior->preferred_dist);
      glm::vec2 to_target = target_pos - transform.position;
      float     to_dist   = glm::length(to_target);
      if (to_dist > 0.5f) {
        direction = to_target / to_dist;
      }
      command.requested_action = Action::None;
    } else {
      if (behavior) behavior->state = SlimeState::Approach;
      command.requested_action = Action::None;
    }

    grid.query_radius(transform.position, 1.8f, neighbors);
    glm::vec2 separation(0.0f);
    int       count = 0;

    for (auto other : neighbors) {
      if (other == entity) continue;
      glm::vec2 other_pos = slimes.get<Transform>(other).position;
      glm::vec2 diff      = transform.position - other_pos;
      float     d_sq      = glm::dot(diff, diff);

      if (d_sq < 0.01f) {
        separation += glm::vec2(1.0f, 0.5f);
        count++;
      } else if (d_sq < 3.24f) {
        separation += glm::normalize(diff) / std::sqrt(d_sq);
        count++;
      }
    }

    if (count > 0) {
      direction = glm::normalize(direction * 3.5f + separation);
    }

    if (command.requested_action != Action::Cleave) {
      command.move = direction;
    }
  }
}

}
