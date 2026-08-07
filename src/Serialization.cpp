#include "padv/Serialization.hpp"

#include <cereal/types/vector.hpp>

namespace padv {

void serialize_state(const entt::registry& registry, const Simulation& simulation,
                     cereal::BinaryOutputArchive& archive) {
  archive(simulation);

  entt::snapshot{registry}
      .get<entt::entity>(archive)
      .get<Knight>(archive)
      .get<Monster>(archive)
      .get<Transform>(archive)
      .get<Health>(archive)
      .get<Stamina>(archive)
      .get<CurrentAction>(archive)
      .get<Command>(archive)
      .get<PerceptionData>(archive)
      .get<FloatingText>(archive)
      .get<AttackSkill>(archive)
      .get<CleaveSkill>(archive)
      .get<ShieldSkill>(archive)
      .get<WarcrySkill>(archive)
      .get<Cooldown<CleaveSkill>>(archive)
      .get<Cooldown<ShieldSkill>>(archive)
      .get<Cooldown<WarcrySkill>>(archive);
}

void deserialize_state(entt::registry& registry, Simulation& simulation, cereal::BinaryInputArchive& archive) {
  archive(simulation);

  registry.clear();

  entt::continuous_loader loader{registry};

  loader.get<entt::entity>(archive)
      .get<Knight>(archive)
      .get<Monster>(archive)
      .get<Transform>(archive)
      .get<Health>(archive)
      .get<Stamina>(archive)
      .get<CurrentAction>(archive)
      .get<Command>(archive)
      .get<PerceptionData>(archive)
      .get<FloatingText>(archive)
      .get<AttackSkill>(archive)
      .get<CleaveSkill>(archive)
      .get<ShieldSkill>(archive)
      .get<WarcrySkill>(archive)
      .get<Cooldown<CleaveSkill>>(archive)
      .get<Cooldown<ShieldSkill>>(archive)
      .get<Cooldown<WarcrySkill>>(archive);
}

}  // namespace padv