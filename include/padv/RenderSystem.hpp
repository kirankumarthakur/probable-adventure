#pragma once

#include <SFML/Graphics.hpp>
#include <entt/entt.hpp>

#include "Types.hpp"

namespace padv {

struct RenderDebugFlags {
  bool show_sprites  = true;
  bool show_hitboxes = false;
  bool show_dir_vecs = false;
  bool show_raycasts = false;
};

auto render_world(sf::RenderWindow& window, entt::registry& registry, const sf::View& view,
                  const RenderDebugFlags& flags, float dt) -> void;

auto render_floating_combat_text(sf::RenderWindow& window, entt::registry& registry,
                                 const sf::View& view) -> void;

}  // namespace padv
