#pragma once

#include <cereal/archives/binary.hpp>
#include <entt/entt.hpp>

#include "Components.hpp"

namespace padv {

void serialize_state(
    const entt::registry& registry,
    const Simulation& simulation,
    cereal::BinaryOutputArchive& archive);

void deserialize_state(
    entt::registry& registry,
    Simulation& simulation,
    cereal::BinaryInputArchive& archive);

} // namespace padv