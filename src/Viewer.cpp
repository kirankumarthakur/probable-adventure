#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <cereal/archives/binary.hpp>
#include <entt/entt.hpp>
#include <imgui-SFML.h>
#include <imgui.h>

#include "padv/Components.hpp"
#include "padv/Serialization.hpp"

auto render_entities(sf::RenderWindow& window, entt::registry& registry, bool show_hitboxes, bool show_dir_vecs,
                     bool show_raycasts, sf::View& view) -> void {
  const float SCALE = 15.0f;

  sf::RectangleShape arena_border(sf::Vector2f(200.0f * SCALE, 200.0f * SCALE));
  arena_border.setOrigin(sf::Vector2f(100.0f * SCALE, 100.0f * SCALE));
  arena_border.setPosition(sf::Vector2f(0.0f, 0.0f));
  arena_border.setFillColor(sf::Color::Transparent);
  arena_border.setOutlineColor(sf::Color(100, 100, 100, 150));
  arena_border.setOutlineThickness(2.0f);
  window.draw(arena_border);

  auto monsters = registry.view<padv::Monster, padv::Transform, padv::Health, padv::CurrentAction, padv::Command>();
  for (auto entity : monsters) {
    const auto& transform = monsters.get<padv::Transform>(entity);
    const auto& health    = monsters.get<padv::Health>(entity);
    const auto& action    = monsters.get<padv::CurrentAction>(entity);
    const auto& command   = monsters.get<padv::Command>(entity);

    if (health.current <= 0.0f) continue;
    sf::Vector2f screen_pos(transform.position.x * SCALE, transform.position.y * SCALE);

    if (action.action == padv::Action::Cleave) {
      float range = 8.0f * SCALE;
      float angle = 0.55f;

      sf::ConvexShape cone(3);
      cone.setPoint(0, sf::Vector2f(0.0f, 0.0f));
      float fwd_angle = std::atan2(transform.forward.y, transform.forward.x);
      cone.setPoint(1, sf::Vector2f(std::cos(fwd_angle - angle) * range, std::sin(fwd_angle - angle) * range));
      cone.setPoint(2, sf::Vector2f(std::cos(fwd_angle + angle) * range, std::sin(fwd_angle + angle) * range));

      cone.setPosition(screen_pos);
      cone.setFillColor(sf::Color(255, 100, 100, 100));
      window.draw(cone);
    }

    sf::Color state_color = sf::Color(255, 165, 0);

    if (action.action != padv::Action::None) {
      state_color = sf::Color::Red;
    } else {
      float move_sq = glm::dot(command.move, command.move);
      if (move_sq > 0.0001f) {
        glm::vec2 target_dir = command.move / std::sqrt(move_sq);
        float     dot_fwd    = glm::dot(transform.forward, target_dir);

        if (dot_fwd < -0.2f) {
          state_color = sf::Color::White;
        } else if (dot_fwd < 0.95f) {
          state_color = sf::Color::Yellow;
        }
      }
    }

    sf::ConvexShape shape(3);
    float           size      = 1.0f * SCALE;
    float           fwd_angle = std::atan2(transform.forward.y, transform.forward.x);

    shape.setPoint(0, sf::Vector2f(std::cos(fwd_angle) * size, std::sin(fwd_angle) * size));
    shape.setPoint(1, sf::Vector2f(std::cos(fwd_angle - 2.5f) * size * 0.8f, std::sin(fwd_angle - 2.5f) * size * 0.8f));
    shape.setPoint(2, sf::Vector2f(std::cos(fwd_angle + 2.5f) * size * 0.8f, std::sin(fwd_angle + 2.5f) * size * 0.8f));
    shape.setPosition(screen_pos);
    shape.setFillColor(state_color);
    window.draw(shape);

    if (show_dir_vecs) {
      float move_sq = glm::dot(command.move, command.move);
      if (move_sq > 0.0001f) {
        glm::vec2       target_dir = command.move / std::sqrt(move_sq);
        sf::VertexArray dir_line(sf::PrimitiveType::Lines, 2);
        dir_line[0].position = screen_pos;
        dir_line[0].color    = sf::Color::Cyan;
        dir_line[1].position =
            sf::Vector2f(screen_pos.x + target_dir.x * 2.5f * SCALE, screen_pos.y + target_dir.y * 2.5f * SCALE);
        dir_line[1].color = sf::Color::Cyan;
        window.draw(dir_line);
      }
    }

    if (show_hitboxes) {
      sf::CircleShape hitbox(1.5f * SCALE);
      hitbox.setOrigin(sf::Vector2f(1.5f * SCALE, 1.5f * SCALE));
      hitbox.setPosition(screen_pos);
      hitbox.setFillColor(sf::Color::Transparent);
      hitbox.setOutlineColor(sf::Color(0, 255, 0, 150));
      hitbox.setOutlineThickness(1.0f);
      window.draw(hitbox);
    }

    sf::RectangleShape hp_bg(sf::Vector2f(20.0f, 4.0f));
    hp_bg.setPosition(sf::Vector2f(screen_pos.x - 10.0f, screen_pos.y - 15.0f));
    hp_bg.setFillColor(sf::Color(50, 50, 50));

    sf::RectangleShape hp_fg(sf::Vector2f(20.0f * (health.current / health.max), 4.0f));
    hp_fg.setPosition(sf::Vector2f(screen_pos.x - 10.0f, screen_pos.y - 15.0f));
    hp_fg.setFillColor(sf::Color(255, 50, 50));

    window.draw(hp_bg);
    window.draw(hp_fg);
  }

  auto knights = registry.view<padv::Knight, padv::Transform, padv::Health, padv::Stamina, padv::CurrentAction,
                               padv::Command, padv::PerceptionData>();
  for (auto entity : knights) {
    const auto& transform = knights.get<padv::Transform>(entity);
    const auto& health    = knights.get<padv::Health>(entity);
    const auto& stamina   = knights.get<padv::Stamina>(entity);
    const auto& action    = knights.get<padv::CurrentAction>(entity);
    const auto& command   = knights.get<padv::Command>(entity);
    const auto& perc_data = knights.get<padv::PerceptionData>(entity);

    if (health.current <= 0.0f) continue;
    sf::Vector2f screen_pos(transform.position.x * SCALE, transform.position.y * SCALE);

    if (show_raycasts) {
      for (const auto& ray : perc_data.vision_rays) {
        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0].position = screen_pos;
        line[0].color    = ray.hit ? sf::Color(0, 255, 0, 150) : sf::Color(100, 100, 100, 50);
        line[1].position = sf::Vector2f(screen_pos.x + ray.direction.x * ray.distance * SCALE,
                                        screen_pos.y + ray.direction.y * ray.distance * SCALE);
        line[1].color    = line[0].color;
        window.draw(line);
      }

      for (const auto& ray : perc_data.sound_rays) {
        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0].position = screen_pos;
        line[0].color    = ray.hit ? sf::Color(255, 165, 0, 200) : sf::Color(50, 50, 50, 50);
        line[1].position = sf::Vector2f(screen_pos.x + ray.direction.x * ray.distance * SCALE,
                                        screen_pos.y + ray.direction.y * ray.distance * SCALE);
        line[1].color    = line[0].color;
        window.draw(line);
      }
    }

    if (action.action != padv::Action::None) {
      float progress = action.duration > 0.0f ? (action.elapsed / action.duration) : 1.0f;

      if (action.action == padv::Action::Attack || action.action == padv::Action::Cleave) {
        float range = (action.action == padv::Action::Attack ? 5.0f : 8.0f) * SCALE;
        float angle = action.action == padv::Action::Attack ? 0.2f : 0.55f;

        sf::ConvexShape cone(3);
        cone.setPoint(0, sf::Vector2f(0.0f, 0.0f));
        float fwd_angle = std::atan2(transform.forward.y, transform.forward.x);
        cone.setPoint(1, sf::Vector2f(std::cos(fwd_angle - angle) * range, std::sin(fwd_angle - angle) * range));
        cone.setPoint(2, sf::Vector2f(std::cos(fwd_angle + angle) * range, std::sin(fwd_angle + angle) * range));

        cone.setPosition(screen_pos);
        cone.setFillColor(sf::Color(255, 255, 0, 100));
        window.draw(cone);
      } else if (action.action == padv::Action::Warcry) {
        float           max_radius     = 15.0f * SCALE;
        float           current_radius = max_radius * progress;
        sf::CircleShape ripple(current_radius);
        ripple.setOrigin(sf::Vector2f(current_radius, current_radius));
        ripple.setPosition(screen_pos);
        ripple.setFillColor(sf::Color::Transparent);
        ripple.setOutlineColor(sf::Color(0, 255, 255, static_cast<std::uint8_t>(255 * (1.0f - progress))));
        ripple.setOutlineThickness(2.0f);
        window.draw(ripple);
      }
    }

    if (show_hitboxes) {
      sf::CircleShape hitbox(1.5f * SCALE);
      hitbox.setOrigin(sf::Vector2f(1.5f * SCALE, 1.5f * SCALE));
      hitbox.setPosition(screen_pos);
      hitbox.setFillColor(sf::Color::Transparent);
      hitbox.setOutlineColor(sf::Color(0, 255, 0, 150));
      hitbox.setOutlineThickness(1.0f);
      window.draw(hitbox);
    }

    if (show_dir_vecs) {
      float move_sq = glm::dot(command.move, command.move);
      if (move_sq > 0.0001f) {
        glm::vec2       target_dir = command.move / std::sqrt(move_sq);
        sf::VertexArray dir_line(sf::PrimitiveType::Lines, 2);
        dir_line[0].position = screen_pos;
        dir_line[0].color    = sf::Color::Cyan;
        dir_line[1].position =
            sf::Vector2f(screen_pos.x + target_dir.x * 2.5f * SCALE, screen_pos.y + target_dir.y * 2.5f * SCALE);
        dir_line[1].color = sf::Color::Cyan;
        window.draw(dir_line);
      }
    }

    sf::ConvexShape shape(3);
    float           size      = 1.2f * SCALE;
    float           fwd_angle = std::atan2(transform.forward.y, transform.forward.x);

    shape.setPoint(0, sf::Vector2f(std::cos(fwd_angle) * size, std::sin(fwd_angle) * size));
    shape.setPoint(1, sf::Vector2f(std::cos(fwd_angle - 2.5f) * size * 0.8f, std::sin(fwd_angle - 2.5f) * size * 0.8f));
    shape.setPoint(2, sf::Vector2f(std::cos(fwd_angle + 2.5f) * size * 0.8f, std::sin(fwd_angle + 2.5f) * size * 0.8f));
    shape.setPosition(screen_pos);
    shape.setFillColor(sf::Color(50, 100, 250));

    if (action.action == padv::Action::Shield) {
      shape.setOutlineColor(sf::Color::Green);
      shape.setOutlineThickness(2.0f);
    }
    window.draw(shape);

    sf::RectangleShape khp_bg(sf::Vector2f(30.0f, 5.0f));
    khp_bg.setPosition(sf::Vector2f(screen_pos.x - 15.0f, screen_pos.y - 20.0f));
    khp_bg.setFillColor(sf::Color(50, 50, 50));

    sf::RectangleShape khp_fg(sf::Vector2f(30.0f * (health.current / health.max), 5.0f));
    khp_fg.setPosition(sf::Vector2f(screen_pos.x - 15.0f, screen_pos.y - 20.0f));
    khp_fg.setFillColor(sf::Color(50, 255, 50));

    window.draw(khp_bg);
    window.draw(khp_fg);

    if (action.action == padv::Action::Shield) {
      auto* shield_sk = registry.try_get<padv::ShieldSkill>(entity);
      if (shield_sk) {
        float              armor_ratio = shield_sk->armor / health.max;
        sf::RectangleShape armor_fg(sf::Vector2f(30.0f * armor_ratio, 5.0f));
        armor_fg.setPosition(
            sf::Vector2f(screen_pos.x - 15.0f + 30.0f * (health.current / health.max), screen_pos.y - 20.0f));
        armor_fg.setFillColor(sf::Color(150, 150, 150));
        window.draw(armor_fg);
      }
    }

    sf::RectangleShape kst_bg(sf::Vector2f(30.0f, 3.0f));
    kst_bg.setPosition(sf::Vector2f(screen_pos.x - 15.0f, screen_pos.y - 14.0f));
    kst_bg.setFillColor(sf::Color(50, 50, 50));

    sf::RectangleShape kst_fg(sf::Vector2f(30.0f * (stamina.current / stamina.max), 3.0f));
    kst_fg.setPosition(sf::Vector2f(screen_pos.x - 15.0f, screen_pos.y - 14.0f));
    kst_fg.setFillColor(sf::Color(50, 150, 255));

    window.draw(kst_bg);
    window.draw(kst_fg);
  }

  // 4. Draw Floating Combat Text
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  ImGui::SetNextWindowBgAlpha(0.0f);
  ImGui::Begin("Overlay", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoSavedSettings);

  auto ftexts = registry.view<padv::FloatingText>();
  for (auto entity : ftexts) {
    const auto&  ft        = ftexts.get<padv::FloatingText>(entity);
    sf::Vector2i pixel_pos = window.mapCoordsToPixel(sf::Vector2f(ft.position.x * SCALE, ft.position.y * SCALE), view);

    ImColor     color = ft.type == 0 ? ImColor(255, 50, 50, 255) : ImColor(180, 180, 180, 255);
    std::string text  = std::format("{:.0f}", ft.value);
    ImGui::GetWindowDrawList()->AddText(ImVec2(static_cast<float>(pixel_pos.x), static_cast<float>(pixel_pos.y)), color,
                                        text.c_str());
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

auto main() -> int {
  sf::RenderWindow window(sf::VideoMode(sf::Vector2u(1024, 768)), "Probable Adventure - AI Replay");
  window.setFramerateLimit(30);

  if (!ImGui::SFML::Init(window)) {
    std::cerr << "Failed to initialize ImGui-SFML\n";
    return -1;
  }

  entt::registry   registry;
  padv::Simulation sim;

  sf::View view = window.getDefaultView();
  view.setCenter(sf::Vector2f(0.0f, 0.0f));
  window.setView(view);

  std::unique_ptr<std::ifstream>              current_stream;
  std::unique_ptr<cereal::BinaryInputArchive> current_archive;

  int  current_generation = 0;
  bool playing            = false;

  bool show_hitboxes = false;
  bool show_dir_vecs = false;
  bool show_raycasts = false;

  auto load_gen = [&](int gen) -> bool {
    std::string filename   = std::format("snapshot_gen{}_best.dat", gen);
    auto        new_stream = std::make_unique<std::ifstream>(filename, std::ios::binary);
    if (!new_stream->is_open()) return false;
    current_stream     = std::move(new_stream);
    current_archive    = std::make_unique<cereal::BinaryInputArchive>(*current_stream);
    current_generation = gen;
    registry.clear();
    sim = padv::Simulation{};
    return true;
  };

  if (load_gen(0)) playing = true;
  sf::Clock deltaClock;

  while (window.isOpen()) {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
      ImGui::SFML::ProcessEvent(window, *event);
      if (event->is<sf::Event::Closed>()) window.close();
      if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::Space) playing = !playing;
      }
    }

    ImGui::SFML::Update(window, deltaClock.restart());

    ImGui::Begin("Playback Controls");
    ImGui::Checkbox("Playing (Space)", &playing);

    ImGui::Separator();
    ImGui::Checkbox("Show Hitboxes", &show_hitboxes);
    ImGui::Checkbox("Show Target Direction", &show_dir_vecs);
    ImGui::Checkbox("Show Raycasts", &show_raycasts);
    ImGui::Separator();

    int input_gen = current_generation;
    if (ImGui::InputInt("Generation", &input_gen)) {
      if (input_gen >= 0 && input_gen != current_generation) {
        if (load_gen(input_gen)) playing = true;
      }
    }

    ImGui::Text("Tick: %ld", static_cast<long int>(sim.ticks));
    ImGui::Text("Monsters Killed: %d", sim.monsters_killed);

    ImGui::Separator();
    ImGui::Text("Knight Skills:");
    auto knights = registry.view<padv::Knight>();
    if (knights.begin() != knights.end()) {
      auto knight = *knights.begin();

      auto print_cd = [&](const char* name, float cd) {
        if (cd > 0.0f)
          ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s: %.1f", name, cd);
        else
          ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s: READY", name);
      };

      if (auto* cd = registry.try_get<padv::Cooldown<padv::CleaveSkill>>(knight)) print_cd("Cleave", cd->time_left);
      if (auto* cd = registry.try_get<padv::Cooldown<padv::ShieldSkill>>(knight)) print_cd("Shield", cd->time_left);
      if (auto* cd = registry.try_get<padv::Cooldown<padv::WarcrySkill>>(knight)) print_cd("Warcry", cd->time_left);
    }

    ImGui::Separator();
    if (ImGui::Button("Restart Current Generation")) {
      if (load_gen(current_generation)) playing = true;
    }
    ImGui::End();

    if (playing && current_archive) {
      try {
        padv::deserialize_state(registry, sim, *current_archive);
        auto current_knights = registry.view<padv::Knight, padv::Transform>();
        if (current_knights.begin() != current_knights.end()) {
          auto& transform = current_knights.get<padv::Transform>(*current_knights.begin());

          sf::Vector2f target_center(transform.position.x * 15.0f, transform.position.y * 15.0f);
          sf::Vector2f current_center = view.getCenter();

          view.setCenter(current_center + (target_center - current_center) * 0.2f);
          window.setView(view);
        }
      } catch (const cereal::Exception&) {
        playing = false;
      }
    }

    window.clear(sf::Color(30, 30, 30));
    render_entities(window, registry, show_hitboxes, show_dir_vecs, show_raycasts, view);
    ImGui::SFML::Render(window);
    window.display();
  }

  ImGui::SFML::Shutdown();
  return 0;
}