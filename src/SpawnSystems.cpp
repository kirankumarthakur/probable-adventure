#include "padv/Systems.hpp"

#include <algorithm>

#include "padv/SlimeFactory.hpp"

namespace padv {

auto spawn_system(entt::registry& registry, Arena& arena) -> void {
  if (arena.ticks < arena.next_spawn_tick) return;

  arena.next_spawn_tick = arena.ticks + std::max<I64>(30LL, 200LL - (arena.ticks / 100LL));

  create_slime(registry);
}

}  // namespace padv
