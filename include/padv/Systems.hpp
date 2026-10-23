#pragma once

#include <entt/entt.hpp>

#include "Components.hpp"

namespace padv {

auto perception_system(entt::registry& registry) -> void;
auto adventurer_ai_system(entt::registry& registry) -> void;
auto slime_ai_system(entt::registry& registry) -> void;

auto finish_action(ActionState& current) -> void;
auto execution_system(entt::registry& registry, Arena& arena) -> void;
auto action_system(entt::registry& registry) -> void;
auto cooldown_system(entt::registry& registry) -> void;

auto damage_system(entt::registry& registry, Arena& arena) -> void;
auto death_system(entt::registry& registry, Arena& arena) -> void;

auto movement_system(entt::registry& registry) -> void;
auto collision_system(entt::registry& registry) -> void;
auto recovery_system(entt::registry& registry) -> void;
auto status_system(entt::registry& registry) -> void;
auto floating_text_system(entt::registry& registry) -> void;

auto spawn_system(entt::registry& registry, Arena& arena) -> void;

}  // namespace padv
