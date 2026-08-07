#pragma once

#include <entt/entt.hpp>

#include "Components.hpp"

namespace padv {

void perception_system(entt::registry&);
void mlp_ai_system(entt::registry&);
void monster_ai_system(entt::registry&);

void finish_action(CurrentAction&);
void execution_system(entt::registry&);
void action_system(entt::registry&);
void cooldown_system(entt::registry&);

void damage_system(entt::registry&);
void death_system(entt::registry&, Simulation&);

void movement_system(entt::registry&);
void collision_system(entt::registry&);
void recovery_system(entt::registry&);
void status_system(entt::registry&);
void floating_text_system(entt::registry&);

void spawn_system(entt::registry&, Simulation&);

}  // namespace padv