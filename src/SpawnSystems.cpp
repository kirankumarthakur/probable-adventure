#include <algorithm>

#include "padv/MonsterFactory.hpp"
#include "padv/Systems.hpp"

namespace padv {

void spawn_system(entt::registry& registry, Simulation& sim) {
  if (sim.ticks < sim.next_spawn_tick) return;

  sim.next_spawn_tick = sim.ticks + std::max<I64>(30LL, 200LL - (sim.ticks / 100LL));

  create_monster(registry);
}

}  // namespace padv