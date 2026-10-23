#pragma once

#include <entt/entity/fwd.hpp>

#include "Components.hpp"
#include "ModalPolicy.hpp"

namespace padv {

auto build_adventurer(entt::registry& registry, const ModalPolicy& policy) -> void;

auto step_simulation(entt::registry& registry, Arena& arena, Action manual_override = Action::None) -> void;

auto compute_fitness(const Arena& arena) -> F32;

auto run_arena_trial(const ModalPolicy& policy, I64 max_ticks = 3000, Arena* out_arena = nullptr) -> F32;

}  // namespace padv
