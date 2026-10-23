#include "padv/Serialization.hpp"

#include <cereal/types/vector.hpp>

namespace padv {

auto serialize_state(const entt::registry& registry, const Arena& arena,
                     cereal::BinaryOutputArchive& archive) -> void {
  archive(arena);

  entt::snapshot{registry}
      .get<entt::entity>(archive)
      .get<Adventurer>(archive)
      .get<Slime>(archive)
      .get<Transform>(archive)
      .get<Health>(archive)
      .get<Stamina>(archive)
      .get<ActionState>(archive)
      .get<Command>(archive)
      .get<SensorData>(archive)
      .get<FloatingText>(archive)
      .get<CleaveSkill>(archive)
      .get<ShieldSkill>(archive)
      .get<WarcrySkill>(archive)
      .get<Cooldown<CleaveSkill>>(archive)
      .get<Cooldown<ShieldSkill>>(archive)
      .get<Cooldown<WarcrySkill>>(archive);
}

auto deserialize_state(entt::registry& registry, Arena& arena, cereal::BinaryInputArchive& archive) -> void {
  archive(arena);

  registry.clear();

  entt::continuous_loader loader{registry};

  loader.get<entt::entity>(archive)
      .get<Adventurer>(archive)
      .get<Slime>(archive)
      .get<Transform>(archive)
      .get<Health>(archive)
      .get<Stamina>(archive)
      .get<ActionState>(archive)
      .get<Command>(archive)
      .get<SensorData>(archive)
      .get<FloatingText>(archive)
      .get<CleaveSkill>(archive)
      .get<ShieldSkill>(archive)
      .get<WarcrySkill>(archive)
      .get<Cooldown<CleaveSkill>>(archive)
      .get<Cooldown<ShieldSkill>>(archive)
      .get<Cooldown<WarcrySkill>>(archive);
}

}  // namespace padv
