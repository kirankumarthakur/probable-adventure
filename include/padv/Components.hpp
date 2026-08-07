#pragma once

#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "Types.hpp"

namespace glm {

template <class Archive>
void serialize(Archive& archive, glm::vec2& v) {
  archive(v.x, v.y);
}

}  // namespace glm

namespace padv {

struct Knight {
  template <class Archive>
  void serialize(Archive&) {}
};

struct Monster {
  template <class Archive>
  void serialize(Archive&) {}
};

struct Dead {};
struct Audible {};

struct Transform {
  glm::vec2 position;
  glm::vec2 forward;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(position, forward);
  }
};

struct Health {
  F32 current;
  F32 max;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(current, max);
  }
};

struct HealthRegen {
  F32 rate;
};

struct Stamina {
  F32 current;
  F32 max;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(current, max);
  }
};

struct StaminaRegen {
  F32 rate;
};

struct Movement {
  F32 speed;
  F32 turn_speed;
};

struct Perception {
  F32 vision_radius;
  F32 vision_cos;
  F32 hearing_radius;
};

struct RaycastHit {
  glm::vec2 direction;
  F32       distance;
  Bool      hit;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(direction, distance, hit);
  }
};

struct PerceptionData {
  std::vector<RaycastHit> vision_rays;
  std::vector<RaycastHit> sound_rays;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(vision_rays, sound_rays);
  }
};

enum class Action { None, Cleave, Shield, Warcry, Attack };

struct Command {
  glm::vec2 move{};
  Action    requested_action = Action::None;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(move, requested_action);
  }
};

struct MlpBrain {
  std::vector<F32> hidden_weights;
  std::vector<F32> hidden_biases;

  std::vector<F32> output_weights;
  std::vector<F32> output_biases;

  I32 input_size;
  I32 hidden_size;
  I32 output_size;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(hidden_weights, hidden_biases, output_weights, output_biases, input_size, hidden_size, output_size);
  }
};

struct CleaveSkill {
  F32 damage;
  F32 range;
  F32 fov_cos;
  F32 cast_time;
  F32 stamina_cost;
  F32 cooldown;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(damage, range, fov_cos, cast_time, stamina_cost, cooldown);
  }
};

struct ShieldSkill {
  F32 armor;
  F32 fov_cos;
  F32 movement_modifier;
  F32 stamina_cost_active;
  F32 cooldown;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(armor, fov_cos, movement_modifier, stamina_cost_active, cooldown);
  }
};

struct WarcrySkill {
  F32 cast_time;
  F32 stun_radius;
  F32 slow_radius;
  F32 stun_duration;
  F32 slow_duration;
  F32 slow_modifier;
  F32 stamina_cost;
  F32 cooldown;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(cast_time, stun_radius, slow_radius, stun_duration, slow_duration, slow_modifier, stamina_cost, cooldown);
  }
};

struct AttackSkill {
  F32 damage;
  F32 range;
  F32 cast_time;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(damage, range, cast_time);
  }
};

template <typename Skill>
struct Cooldown {
  F32 time_left;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(time_left);
  }
};

struct CurrentAction {
  Action action = Action::None;

  F32 elapsed  = 0.0f;
  F32 duration = 0.0f;

  Bool executed = false;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(action, elapsed, duration, executed);
  }
};

struct Stunned {
  F32 time_left;
};

struct Slowed {
  F32 time_left;
  F32 slow_modifier;
};

struct IncomingHit {
  entt::entity attacker = entt::null;
  F32          damage;
  glm::vec2    source_dir;
};

struct DamageQueue {
  std::vector<IncomingHit> hits;
};

struct FloatingText {
  F32       value;
  glm::vec2 position;
  glm::vec2 velocity;
  F32       time_left;
  I32       type;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(value, position, velocity, time_left, type);
  }
};

struct Simulation {
  I64  ticks           = 0;
  I64  next_spawn_tick = 0;
  Bool game_over       = false;
  I32  monsters_killed = 0;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(ticks, game_over, monsters_killed);
  }
};

}  // namespace padv