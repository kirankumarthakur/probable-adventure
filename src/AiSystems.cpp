#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "padv/Systems.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace padv {

void perception_system(entt::registry& registry) {
  auto knights  = registry.view<Knight, Transform, Perception, PerceptionData>(entt::exclude<Dead>);
  auto monsters = registry.view<Monster, Transform>(entt::exclude<Dead>);

  constexpr int   NUM_VISION_RAYS = 11;
  constexpr int   NUM_SOUND_RAYS  = 8;
  constexpr float MONSTER_RADIUS  = 1.5f;

  for (auto knight : knights) {
    auto& transform  = knights.get<Transform>(knight);
    auto& perception = knights.get<Perception>(knight);
    auto& data       = knights.get<PerceptionData>(knight);

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

      for (auto monster : monsters) {
        auto&     m_trans = monsters.get<Transform>(monster);
        glm::vec2 L       = m_trans.position - transform.position;

        float tca = glm::dot(L, ray_dir);
        if (tca < 0 || tca > perception.vision_radius + MONSTER_RADIUS) continue;

        float d2      = glm::dot(L, L) - tca * tca;
        float radius2 = MONSTER_RADIUS * MONSTER_RADIUS;
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
      for (auto monster : monsters) {
        if (!registry.all_of<Audible>(monster)) continue;

        auto&     m_trans = monsters.get<Transform>(monster);
        glm::vec2 L       = m_trans.position - transform.position;

        float tca = glm::dot(L, ray_dir);
        if (tca < 0 || tca > perception.hearing_radius + MONSTER_RADIUS) continue;

        float d2      = glm::dot(L, L) - tca * tca;
        float radius2 = MONSTER_RADIUS * MONSTER_RADIUS;

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

void mlp_ai_system(entt::registry& registry) {
  auto view = registry.view<MlpBrain, Command, Health, Stamina, PerceptionData>(entt::exclude<Dead>);

  for (auto entity : view) {
    const auto& brain      = view.get<MlpBrain>(entity);
    auto&       command    = view.get<Command>(entity);
    const auto& hp         = view.get<Health>(entity);
    const auto& stamina    = view.get<Stamina>(entity);
    const auto& perception = view.get<PerceptionData>(entity);

    std::vector<float> inputs(brain.input_size, 0.0f);

    int expected_inputs = 2 + perception.vision_rays.size() + perception.sound_rays.size();

    if (brain.input_size >= expected_inputs && expected_inputs > 2) {
      inputs[0] = static_cast<float>(hp.current / hp.max);
      inputs[1] = static_cast<float>(stamina.current / stamina.max);

      int idx = 2;
      for (const auto& ray : perception.vision_rays) {
        inputs[idx++] = ray.hit ? (1.0f - (ray.distance / 100.0f)) : 0.0f;
      }
      for (const auto& ray : perception.sound_rays) {
        inputs[idx++] = ray.hit ? 1.0f : 0.0f;
      }
    }

    std::vector<float> hidden(brain.hidden_size, 0.0f);
    for (int h = 0; h < brain.hidden_size; ++h) {
      float sum = brain.hidden_biases[h];
      for (int i = 0; i < brain.input_size; ++i) {
        sum += inputs[i] * brain.hidden_weights[i * brain.hidden_size + h];
      }
      hidden[h] = std::max(0.0f, sum);
    }

    std::vector<float> outputs(brain.output_size, 0.0f);
    for (int o = 0; o < brain.output_size; ++o) {
      float sum = brain.output_biases[o];
      for (int h = 0; h < brain.hidden_size; ++h) {
        sum += hidden[h] * brain.output_weights[h * brain.output_size + o];
      }
      outputs[o] = sum;
    }

    if (brain.output_size >= 5) {
      command.move = glm::vec2(outputs[0], outputs[1]);
      if (outputs[2] > 0.5f)
        command.requested_action = Action::Cleave;
      else if (outputs[3] > 0.5f)
        command.requested_action = Action::Shield;
      else if (outputs[4] > 0.5f)
        command.requested_action = Action::Warcry;
      else
        command.requested_action = Action::None;
    }
  }
}

void monster_ai_system(entt::registry& registry) {
  auto knights = registry.view<Knight, Transform>(entt::exclude<Dead>);
  if (knights.begin() == knights.end()) return;

  glm::vec2 knight_pos = knights.get<Transform>(*knights.begin()).position;
  auto      monsters   = registry.view<Monster, Transform, Command, CleaveSkill>(entt::exclude<Dead, Stunned>);

  for (auto entity : monsters) {
    auto&       transform = monsters.get<Transform>(entity);
    auto&       command   = monsters.get<Command>(entity);
    const auto& skill     = monsters.get<CleaveSkill>(entity);

    glm::vec2 delta    = knight_pos - transform.position;
    float     dist_sq  = glm::dot(delta, delta);
    float     range_sq = skill.range * skill.range;

    glm::vec2 direction = dist_sq > 0.0001f ? glm::normalize(delta) : glm::vec2(1.0f, 0.0f);

    // Apply Boids-style separation so they flank and steer around each other
    glm::vec2 separation(0.0f);
    int       neighbors = 0;
    for (auto other : monsters) {
      if (other == entity) continue;
      glm::vec2 other_pos = monsters.get<Transform>(other).position;
      glm::vec2 diff      = transform.position - other_pos;
      float     d_sq      = glm::dot(diff, diff);
      if (d_sq > 0.0001f && d_sq < 9.0f) {  // 3.0 unit radius to care about neighbors
        separation += glm::normalize(diff) / std::sqrt(d_sq);
        neighbors++;
      }
    }

    if (neighbors > 0) {
      // Blend primary direction (weighted heavily) with the separation vector
      direction = glm::normalize(direction * 2.0f + separation);
    }

    if (dist_sq < (range_sq * 0.6f)) {
      command.move = glm::vec2(0.0f);
    } else {
      command.move = direction;
    }

    command.requested_action = (dist_sq > range_sq) ? Action::None : Action::Cleave;
  }
}

}  // namespace padv