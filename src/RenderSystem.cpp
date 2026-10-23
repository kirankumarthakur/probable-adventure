#include "padv/RenderSystem.hpp"

#include <cmath>
#include <cstdint>
#include <format>
#include <string>

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>

#include "padv/Components.hpp"
#include "padv/TextureManager.hpp"

namespace padv {

namespace {

constexpr float SCALE = 15.0f;

auto draw_arena_background(sf::RenderWindow& window) -> void {
  constexpr float BOUNDS = 100.0f * SCALE;

  sf::RectangleShape floor(sf::Vector2f(BOUNDS * 2.0f, BOUNDS * 2.0f));
  floor.setOrigin(sf::Vector2f(BOUNDS, BOUNDS));
  floor.setPosition(sf::Vector2f(0.0f, 0.0f));
  floor.setFillColor(sf::Color(25, 27, 32));
  window.draw(floor);

  constexpr float GRID_STEP = 10.0f * SCALE;
  for (float x = -BOUNDS; x <= BOUNDS; x += GRID_STEP) {
    sf::VertexArray line(sf::PrimitiveType::Lines, 2);
    line[0].position = sf::Vector2f(x, -BOUNDS);
    line[0].color    = sf::Color(35, 38, 45);
    line[1].position = sf::Vector2f(x, BOUNDS);
    line[1].color    = sf::Color(35, 38, 45);
    window.draw(line);
  }
  for (float y = -BOUNDS; y <= BOUNDS; y += GRID_STEP) {
    sf::VertexArray line(sf::PrimitiveType::Lines, 2);
    line[0].position = sf::Vector2f(-BOUNDS, y);
    line[0].color    = sf::Color(35, 38, 45);
    line[1].position = sf::Vector2f(BOUNDS, y);
    line[1].color    = sf::Color(35, 38, 45);
    window.draw(line);
  }

  sf::RectangleShape border(sf::Vector2f(BOUNDS * 2.0f, BOUNDS * 2.0f));
  border.setOrigin(sf::Vector2f(BOUNDS, BOUNDS));
  border.setPosition(sf::Vector2f(0.0f, 0.0f));
  border.setFillColor(sf::Color::Transparent);
  border.setOutlineColor(sf::Color(80, 85, 100));
  border.setOutlineThickness(3.0f);
  window.draw(border);
}

auto draw_shadow(sf::RenderWindow& window, sf::Vector2f pos, float radius) -> void {
  sf::CircleShape shadow(radius);
  shadow.setOrigin(sf::Vector2f(radius, radius * 0.5f));
  shadow.setScale(sf::Vector2f(1.0f, 0.5f));
  shadow.setPosition(sf::Vector2f(pos.x, pos.y + radius * 0.8f));
  shadow.setFillColor(sf::Color(0, 0, 0, 80));
  window.draw(shadow);
}

}

auto render_world(sf::RenderWindow& window, entt::registry& registry, const sf::View&,
                  const RenderDebugFlags& flags, float dt) -> void {
  draw_arena_background(window);

  auto& tex_mgr = TextureManager::instance();

  auto slimes = registry.view<Slime, Transform, Health, ActionState, Command>();
  for (auto entity : slimes) {
    const auto& transform = slimes.get<Transform>(entity);
    const auto& health    = slimes.get<Health>(entity);
    const auto& action    = slimes.get<ActionState>(entity);
    const auto& command   = slimes.get<Command>(entity);

    if (health.current <= 0.0f) continue;

    sf::Vector2f screen_pos(transform.position.x * SCALE, transform.position.y * SCALE);

    draw_shadow(window, screen_pos, 1.2f * SCALE);

    std::string anim = "slime_idle";
    bool        flip = (transform.forward.x < 0.0f);

    if (action.action == Action::Cleave) {
      anim = "slime_attack";
    } else if (registry.all_of<Stunned>(entity)) {
      anim = "slime_hurt";
    } else if (glm::dot(command.move, command.move) > 0.001f) {
      anim = "slime_move";
      flip = (command.move.x < 0.0f);
    }

    auto* anim_comp = registry.try_get<SpriteAnimation>(entity);
    if (!anim_comp) {
      anim_comp = &registry.emplace<SpriteAnimation>(entity);
    }

    if (anim_comp->current_anim != anim) {
      anim_comp->current_anim  = anim;
      anim_comp->current_frame = 0;
      anim_comp->frame_timer   = 0.0f;
    }

    anim_comp->frame_timer += dt;
    if (anim_comp->frame_timer >= anim_comp->frame_duration) {
      anim_comp->frame_timer -= anim_comp->frame_duration;
      int count                = tex_mgr.get_frame_count(anim);
      anim_comp->current_frame = count > 0 ? (anim_comp->current_frame + 1) % count : 0;
    }
    anim_comp->flip_x = flip;

    if (flags.show_sprites) {
      const auto* tex = tex_mgr.get_frame(anim_comp->current_anim, anim_comp->current_frame);
      if (tex) {
        sf::Sprite sprite(*tex);
        auto tex_size = tex->getSize();
        sprite.setOrigin(sf::Vector2f(static_cast<float>(tex_size.x) * 0.5f, static_cast<float>(tex_size.y) * 0.5f));
        sprite.setPosition(screen_pos);

        float target_h = 3.2f * SCALE;
        float scale_y  = target_h / static_cast<float>(tex_size.y);
        float scale_x  = anim_comp->flip_x ? -scale_y : scale_y;
        sprite.setScale(sf::Vector2f(scale_x, scale_y));

        window.draw(sprite);
      }
    } else {
      sf::ConvexShape shape(3);
      float           size      = 1.0f * SCALE;
      float           fwd_angle = std::atan2(transform.forward.y, transform.forward.x);
      shape.setPoint(0, sf::Vector2f(std::cos(fwd_angle) * size, std::sin(fwd_angle) * size));
      shape.setPoint(1, sf::Vector2f(std::cos(fwd_angle - 2.5f) * size * 0.8f, std::sin(fwd_angle - 2.5f) * size * 0.8f));
      shape.setPoint(2, sf::Vector2f(std::cos(fwd_angle + 2.5f) * size * 0.8f, std::sin(fwd_angle + 2.5f) * size * 0.8f));
      shape.setPosition(screen_pos);
      shape.setFillColor(sf::Color(255, 165, 0));
      window.draw(shape);
    }

    sf::RectangleShape hp_bg(sf::Vector2f(22.0f, 4.0f));
    hp_bg.setPosition(sf::Vector2f(screen_pos.x - 11.0f, screen_pos.y - 22.0f));
    hp_bg.setFillColor(sf::Color(40, 40, 40));

    float hp_ratio =
        health.max > 0.0f ? std::clamp<float>(static_cast<float>(health.current / health.max), 0.0f, 1.0f) : 0.0f;
    sf::RectangleShape hp_fg(sf::Vector2f(22.0f * hp_ratio, 4.0f));
    hp_fg.setPosition(sf::Vector2f(screen_pos.x - 11.0f, screen_pos.y - 22.0f));
    hp_fg.setFillColor(sf::Color(230, 50, 50));

    window.draw(hp_bg);
    window.draw(hp_fg);

    if (flags.show_hitboxes) {
      sf::CircleShape hitbox(1.5f * SCALE);
      hitbox.setOrigin(sf::Vector2f(1.5f * SCALE, 1.5f * SCALE));
      hitbox.setPosition(screen_pos);
      hitbox.setFillColor(sf::Color::Transparent);
      hitbox.setOutlineColor(sf::Color(0, 255, 0, 150));
      hitbox.setOutlineThickness(1.0f);
      window.draw(hitbox);
    }

    if (flags.show_dir_vecs && glm::dot(command.move, command.move) > 0.0001f) {
      glm::vec2       target_dir = glm::normalize(command.move);
      sf::VertexArray dir_line(sf::PrimitiveType::Lines, 2);
      dir_line[0].position = screen_pos;
      dir_line[0].color    = sf::Color::Cyan;
      dir_line[1].position =
          sf::Vector2f(screen_pos.x + target_dir.x * 2.5f * SCALE, screen_pos.y + target_dir.y * 2.5f * SCALE);
      dir_line[1].color = sf::Color::Cyan;
      window.draw(dir_line);
    }
  }

  auto adventurers = registry.view<Adventurer, Transform, Health, Stamina, ActionState, Command, SensorData>();
  for (auto entity : adventurers) {
    const auto& transform = adventurers.get<Transform>(entity);
    const auto& health    = adventurers.get<Health>(entity);
    const auto& stamina   = adventurers.get<Stamina>(entity);
    const auto& action    = adventurers.get<ActionState>(entity);
    const auto& command   = adventurers.get<Command>(entity);
    const auto& sensors   = adventurers.get<SensorData>(entity);

    if (health.current <= 0.0f) continue;

    sf::Vector2f screen_pos(transform.position.x * SCALE, transform.position.y * SCALE);

    draw_shadow(window, screen_pos, 1.5f * SCALE);

    if (flags.show_raycasts) {
      for (const auto& ray : sensors.vision_rays) {
        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0].position = screen_pos;
        line[0].color    = ray.hit ? sf::Color(0, 255, 0, 150) : sf::Color(100, 100, 100, 50);
        line[1].position = sf::Vector2f(screen_pos.x + ray.direction.x * ray.distance * SCALE,
                                        screen_pos.y + ray.direction.y * ray.distance * SCALE);
        line[1].color    = line[0].color;
        window.draw(line);
      }

      for (const auto& ray : sensors.sound_rays) {
        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0].position = screen_pos;
        line[0].color    = ray.hit ? sf::Color(255, 165, 0, 200) : sf::Color(50, 50, 50, 50);
        line[1].position = sf::Vector2f(screen_pos.x + ray.direction.x * ray.distance * SCALE,
                                        screen_pos.y + ray.direction.y * ray.distance * SCALE);
        line[1].color    = line[0].color;
        window.draw(line);
      }
    }

    if (action.action != Action::None) {
      float progress = action.duration > 0.0f ? (action.elapsed / action.duration) : 1.0f;

      if (action.action == Action::Cleave) {
        float range = 8.0f * SCALE;
        float angle = 0.6f;

        sf::ConvexShape cone(3);
        cone.setPoint(0, sf::Vector2f(0.0f, 0.0f));
        float fwd_angle = std::atan2(transform.forward.y, transform.forward.x);
        cone.setPoint(1, sf::Vector2f(std::cos(fwd_angle - angle) * range, std::sin(fwd_angle - angle) * range));
        cone.setPoint(2, sf::Vector2f(std::cos(fwd_angle + angle) * range, std::sin(fwd_angle + angle) * range));

        cone.setPosition(screen_pos);
        cone.setFillColor(sf::Color(255, 230, 80, 90));
        window.draw(cone);
      } else if (action.action == Action::Warcry) {
        float           max_radius     = 15.0f * SCALE;
        float           current_radius = max_radius * progress;
        sf::CircleShape ripple(current_radius);
        ripple.setOrigin(sf::Vector2f(current_radius, current_radius));
        ripple.setPosition(screen_pos);
        ripple.setFillColor(sf::Color::Transparent);
        ripple.setOutlineColor(sf::Color(0, 255, 255, static_cast<std::uint8_t>(255 * (1.0f - progress))));
        ripple.setOutlineThickness(3.0f);
        window.draw(ripple);
      }
    }

    if (action.action == Action::Shield) {
      float           shield_r = 2.2f * SCALE;
      sf::CircleShape aura(shield_r);
      aura.setOrigin(sf::Vector2f(shield_r, shield_r));
      aura.setPosition(screen_pos);
      aura.setFillColor(sf::Color(80, 180, 255, 50));
      aura.setOutlineColor(sf::Color(100, 220, 255, 180));
      aura.setOutlineThickness(2.0f);
      window.draw(aura);
    }

    std::string anim = "adv_idle";
    bool        flip = (transform.forward.x < 0.0f);

    if (action.action == Action::Cleave) {
      anim = "adv_slash";
    } else if (sensors.is_debuffed > 0.5f) {
      anim = "adv_hurt";
    } else if (glm::dot(command.move, command.move) > 0.001f) {
      anim = "adv_run";
      flip = (command.move.x < 0.0f);
    }

    auto* anim_comp = registry.try_get<SpriteAnimation>(entity);
    if (!anim_comp) {
      anim_comp = &registry.emplace<SpriteAnimation>(entity);
    }

    if (anim_comp->current_anim != anim) {
      anim_comp->current_anim  = anim;
      anim_comp->current_frame = 0;
      anim_comp->frame_timer   = 0.0f;
    }

    anim_comp->frame_timer += dt;
    if (anim_comp->frame_timer >= anim_comp->frame_duration) {
      anim_comp->frame_timer -= anim_comp->frame_duration;
      int count                = tex_mgr.get_frame_count(anim);
      anim_comp->current_frame = count > 0 ? (anim_comp->current_frame + 1) % count : 0;
    }
    anim_comp->flip_x = flip;

    if (flags.show_sprites) {
      const auto* tex = tex_mgr.get_frame(anim_comp->current_anim, anim_comp->current_frame);
      if (tex) {
        sf::Sprite sprite(*tex);
        auto tex_size = tex->getSize();
        sprite.setOrigin(sf::Vector2f(static_cast<float>(tex_size.x) * 0.5f, static_cast<float>(tex_size.y) * 0.5f));
        sprite.setPosition(screen_pos);

        float target_h = 3.6f * SCALE;
        float scale_y  = target_h / static_cast<float>(tex_size.y);
        float scale_x  = anim_comp->flip_x ? -scale_y : scale_y;
        sprite.setScale(sf::Vector2f(scale_x, scale_y));

        window.draw(sprite);
      }
    } else {
      sf::ConvexShape shape(3);
      float           size      = 1.2f * SCALE;
      float           fwd_angle = std::atan2(transform.forward.y, transform.forward.x);
      shape.setPoint(0, sf::Vector2f(std::cos(fwd_angle) * size, std::sin(fwd_angle) * size));
      shape.setPoint(1, sf::Vector2f(std::cos(fwd_angle - 2.5f) * size * 0.8f, std::sin(fwd_angle - 2.5f) * size * 0.8f));
      shape.setPoint(2, sf::Vector2f(std::cos(fwd_angle + 2.5f) * size * 0.8f, std::sin(fwd_angle + 2.5f) * size * 0.8f));
      shape.setPosition(screen_pos);
      shape.setFillColor(sf::Color(50, 120, 255));
      window.draw(shape);
    }

    sf::RectangleShape hp_bg(sf::Vector2f(32.0f, 5.0f));
    hp_bg.setPosition(sf::Vector2f(screen_pos.x - 16.0f, screen_pos.y - 28.0f));
    hp_bg.setFillColor(sf::Color(40, 40, 40));

    float hp_ratio =
        health.max > 0.0f ? std::clamp<float>(static_cast<float>(health.current / health.max), 0.0f, 1.0f) : 0.0f;
    sf::RectangleShape hp_fg(sf::Vector2f(32.0f * hp_ratio, 5.0f));
    hp_fg.setPosition(sf::Vector2f(screen_pos.x - 16.0f, screen_pos.y - 28.0f));
    hp_fg.setFillColor(sf::Color(50, 220, 80));

    window.draw(hp_bg);
    window.draw(hp_fg);

    sf::RectangleShape st_bg(sf::Vector2f(32.0f, 3.0f));
    st_bg.setPosition(sf::Vector2f(screen_pos.x - 16.0f, screen_pos.y - 22.0f));
    st_bg.setFillColor(sf::Color(40, 40, 40));

    float st_ratio =
        stamina.max > 0.0f ? std::clamp<float>(static_cast<float>(stamina.current / stamina.max), 0.0f, 1.0f) : 0.0f;
    sf::RectangleShape st_fg(sf::Vector2f(32.0f * st_ratio, 3.0f));
    st_fg.setPosition(sf::Vector2f(screen_pos.x - 16.0f, screen_pos.y - 22.0f));
    st_fg.setFillColor(sf::Color(50, 150, 255));

    window.draw(st_bg);
    window.draw(st_fg);

    if (flags.show_hitboxes) {
      sf::CircleShape hitbox(1.5f * SCALE);
      hitbox.setOrigin(sf::Vector2f(1.5f * SCALE, 1.5f * SCALE));
      hitbox.setPosition(screen_pos);
      hitbox.setFillColor(sf::Color::Transparent);
      hitbox.setOutlineColor(sf::Color(0, 255, 0, 150));
      hitbox.setOutlineThickness(1.0f);
      window.draw(hitbox);
    }

    if (flags.show_dir_vecs && glm::dot(command.move, command.move) > 0.0001f) {
      glm::vec2       target_dir = glm::normalize(command.move);
      sf::VertexArray dir_line(sf::PrimitiveType::Lines, 2);
      dir_line[0].position = screen_pos;
      dir_line[0].color    = sf::Color::Cyan;
      dir_line[1].position =
          sf::Vector2f(screen_pos.x + target_dir.x * 2.5f * SCALE, screen_pos.y + target_dir.y * 2.5f * SCALE);
      dir_line[1].color = sf::Color::Cyan;
      window.draw(dir_line);
    }
  }
}

auto render_floating_combat_text(sf::RenderWindow& window, entt::registry& registry,
                                 const sf::View& view) -> void {
  static sf::Font font;
  static bool loaded = font.openFromFile("C:/Windows/Fonts/segoeui.ttf");
  if (!loaded) return;

  sf::View original_view = window.getView();
  window.setView(window.getDefaultView());

  auto advs = registry.view<Adventurer, Transform, ActionState>(entt::exclude<Dead>);
  for (auto entity : advs) {
    const auto& trans = advs.get<Transform>(entity);
    const auto& act = advs.get<ActionState>(entity);
    if (act.action != Action::None) {
      sf::Vector2i pixel_pos = window.mapCoordsToPixel(sf::Vector2f(trans.position.x * SCALE, trans.position.y * SCALE), view);
      std::string act_name;
      sf::Color act_col;
      switch (act.action) {
        case Action::Cleave:
          act_name = "CLEAVE";
          act_col = sf::Color(255, 110, 40);
          break;
        case Action::Shield:
          act_name = "SHIELD";
          act_col = sf::Color(60, 180, 255);
          break;
        case Action::Warcry:
          act_name = "WARCRY";
          act_col = sf::Color(230, 80, 255);
          break;
        default:
          break;
      }
      if (!act_name.empty()) {
        sf::Text badge(font, act_name, 13);
        badge.setStyle(sf::Text::Bold);
        badge.setFillColor(act_col);
        badge.setOutlineColor(sf::Color(0, 0, 0, 220));
        badge.setOutlineThickness(1.5f);
        auto bounds = badge.getLocalBounds();
        badge.setOrigin(sf::Vector2f(bounds.size.x * 0.5f, bounds.size.y * 0.5f));
        badge.setPosition(sf::Vector2f(static_cast<float>(pixel_pos.x), static_cast<float>(pixel_pos.y) - 46.0f));

        sf::RectangleShape pill(sf::Vector2f(bounds.size.x + 14.0f, 18.0f));
        pill.setOrigin(sf::Vector2f((bounds.size.x + 14.0f) * 0.5f, 9.0f));
        pill.setPosition(sf::Vector2f(static_cast<float>(pixel_pos.x), static_cast<float>(pixel_pos.y) - 46.0f));
        pill.setFillColor(sf::Color(16, 18, 24, 210));
        pill.setOutlineColor(act_col);
        pill.setOutlineThickness(1.0f);

        window.draw(pill);
        window.draw(badge);
      }
    }
  }

  auto ftexts = registry.view<FloatingText>();
  for (auto entity : ftexts) {
    const auto& ft = ftexts.get<FloatingText>(entity);
    sf::Vector2i pixel_pos = window.mapCoordsToPixel(sf::Vector2f(ft.position.x * SCALE, ft.position.y * SCALE), view);

    sf::Color color = (ft.type == 0) ? sf::Color(255, 60, 60, 220) : sf::Color(180, 180, 255, 220);
    std::string text_str = std::format("{:.0f}", ft.value);
    sf::Text text(font, text_str, 18);
    text.setStyle(sf::Text::Bold);
    text.setFillColor(color);
    text.setOutlineColor(sf::Color(0, 0, 0, 200));
    text.setOutlineThickness(1.5f);
    text.setPosition(sf::Vector2f(static_cast<float>(pixel_pos.x), static_cast<float>(pixel_pos.y)));
    window.draw(text);
  }

  window.setView(original_view);
}

}
