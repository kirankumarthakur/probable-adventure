#include "padv/Simulation.hpp"

#include <format>
#include <fstream>
#include <memory>

#include <cereal/archives/binary.hpp>
#include <entt/entt.hpp>

#include "padv/Serialization.hpp"
#include "padv/Systems.hpp"

namespace padv {

void build_knight(entt::registry& registry, const MlpBrain& brain) {
  auto knight = registry.create();

  registry.emplace<Knight>(knight);
  registry.emplace<Transform>(knight, glm::vec2(0.0f), glm::vec2(1.0f, 0.0f));
  registry.emplace<Health>(knight, 100.0f, 100.0f);
  registry.emplace<Stamina>(knight, 100.0f, 100.0f);
  registry.emplace<HealthRegen>(knight, 0.05f);
  registry.emplace<StaminaRegen>(knight, 0.5f);
  registry.emplace<Movement>(knight, 0.6f, 0.8f);
  registry.emplace<Command>(knight);
  registry.emplace<CurrentAction>(knight);
  registry.emplace<Perception>(knight, 100.0f, 0.0f, 50.0f);
  registry.emplace<PerceptionData>(knight);
  registry.emplace<DamageQueue>(knight);
  registry.emplace<MlpBrain>(knight, brain);
  registry.emplace<AttackSkill>(knight, 10.0f, 5.0f, 10.0f);

  // Reduced Cleave Cooldown from 100.0f down to 30.0f ticks
  registry.emplace<CleaveSkill>(knight, 20.0f, 8.0f, 0.85f, 20.0f, 15.0f, 30.0f);

  registry.emplace<ShieldSkill>(knight, 15.0f, 0.5f, 0.5f, 5.0f, 50.0f);
  registry.emplace<WarcrySkill>(knight, 30.0f, 15.0f, 25.0f, 100.0f, 150.0f, 0.5f, 40.0f, 300.0f);
  registry.emplace<Cooldown<CleaveSkill>>(knight, 0.0f);
  registry.emplace<Cooldown<ShieldSkill>>(knight, 0.0f);
  registry.emplace<Cooldown<WarcrySkill>>(knight, 0.0f);
}

F32 play_game(const MlpBrain& brain, I32 generation, const std::string& save_suffix, Bool save_snapshot) {
  entt::registry registry;
  Simulation     simulation;

  build_knight(registry, brain);

  std::ofstream                                snapshot;
  std::unique_ptr<cereal::BinaryOutputArchive> archive;

  if (save_snapshot) {
    snapshot.open(std::format("snapshot_gen{}_{}.dat", generation, save_suffix), std::ios::binary);

    if (snapshot.is_open()) {
      archive = std::make_unique<cereal::BinaryOutputArchive>(snapshot);
    }
  }

  constexpr I64 MAX_TEST_TICKS = 100000;

  while (!simulation.game_over && simulation.ticks < MAX_TEST_TICKS) {
    perception_system(registry);
    mlp_ai_system(registry);
    monster_ai_system(registry);
    execution_system(registry);
    movement_system(registry);
    collision_system(registry);
    action_system(registry);
    damage_system(registry);
    death_system(registry, simulation);
    spawn_system(registry, simulation);
    cooldown_system(registry);
    recovery_system(registry);
    status_system(registry);
    floating_text_system(registry);

    if (archive) {
      serialize_state(registry, simulation, *archive);
    }

    ++simulation.ticks;
  }

  return static_cast<F32>(simulation.monsters_killed) * 500.0f + static_cast<F32>(simulation.ticks) * 0.1f;
}

}  // namespace padv