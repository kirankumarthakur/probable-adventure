#pragma once

#include <cereal/archives/binary.hpp>
#include <entt/entt.hpp>

#include "Components.hpp"

namespace padv {

auto serialize_state(const entt::registry& registry, const Arena& arena,
                     cereal::BinaryOutputArchive& archive) -> void;

auto deserialize_state(entt::registry& registry, Arena& arena,
                       cereal::BinaryInputArchive& archive) -> void;

}  // namespace padv
