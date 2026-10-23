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

struct Adventurer {
  template <class Archive>
  void serialize(Archive&) {}
};

struct Slime {
  template <class Archive>
  void serialize(Archive&) {}
};

enum class SlimeState { Approach, Encircle, Pounce, Stunned };

struct SlimeBehavior {
  SlimeState state          = SlimeState::Approach;
  F32        encircle_angle = 0.0f;
  F32        preferred_dist = 5.0f;
  F32        state_timer    = 0.0f;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(state, encircle_angle, preferred_dist, state_timer);
  }
};

struct SpriteAnimation {
  std::string current_anim   = "idle";
  int         current_frame  = 0;
  float       frame_timer    = 0.0f;
  float       frame_duration = 0.08f;
  bool        flip_x         = false;
  bool        loop           = true;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(current_anim, current_frame, frame_timer, frame_duration, flip_x, loop);
  }
};

struct Dead {};
struct Audible {};
struct LastPosition {
  glm::vec2 pos{0.0f, 0.0f};
};

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

struct SensorData {
  F32 hp_ratio      = 1.0f;
  F32 stamina_ratio = 1.0f;
  F32 cleave_cd     = 1.0f;
  F32 shield_cd     = 1.0f;
  F32 warcry_cd     = 1.0f;
  F32 is_debuffed   = 0.0f;
  F32 wall_north    = 0.0f;
  F32 wall_south    = 0.0f;
  F32 wall_east     = 0.0f;
  F32 wall_west     = 0.0f;

  std::vector<RaycastHit> vision_rays;
  std::vector<RaycastHit> sound_rays;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(hp_ratio, stamina_ratio, cleave_cd, shield_cd, warcry_cd, is_debuffed, wall_north, wall_south, wall_east,
            wall_west, vision_rays, sound_rays);
  }
};

enum class Action { None, Cleave, Shield, Warcry };

struct Command {
  glm::vec2 move{};
  Action    requested_action = Action::None;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(move, requested_action);
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

template <typename Skill>
struct Cooldown {
  F32 time_left;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(time_left);
  }
};

struct ActionState {
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

struct Arena {
  I64  ticks           = 0;
  I64  next_spawn_tick = 0;
  Bool game_over       = false;
  I32  slimes_killed   = 0;

  F32 damage_dealt   = 0.0f;
  F32 damage_blocked = 0.0f;
  F32 damage_taken   = 0.0f;
  I32 slimes_stunned = 0;
  I32 wall_hug_ticks = 0;
  I32 cleave_count   = 0;
  I32 shield_count   = 0;
  I32 warcry_count   = 0;

  F32 center_control_ticks = 0.0f;
  F32 wall_penalty_score   = 0.0f;
  F32 distance_traveled    = 0.0f;

  template <class Archive>
  void serialize(Archive& archive) {
    archive(ticks, game_over, slimes_killed, damage_dealt, damage_blocked, damage_taken);
  }
};

}  // namespace padv
