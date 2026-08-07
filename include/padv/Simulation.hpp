#pragma once

#include <string>

#include <entt/entity/fwd.hpp>

#include "Components.hpp"

namespace padv {

void build_knight(entt::registry& registry, const MlpBrain& brain);

F32 play_game(const MlpBrain& brain, I32 generation, const std::string& save_suffix, Bool save_snapshot);

}  // namespace padv