#include "padv/TextureManager.hpp"

#include <filesystem>
#include <format>
#include <iostream>

#include <spdlog/spdlog.h>

namespace padv {

auto TextureManager::instance() -> TextureManager& {
  static TextureManager s_instance;
  return s_instance;
}

auto TextureManager::load_sequence(const std::string& anim_name, const std::string& folder,
                                   const std::string& prefix, int count, int pad_width) -> bool {
  auto& frames = m_animations[anim_name];
  frames.reserve(count);

  for (int i = 0; i < count; ++i) {
    std::string filename = std::format("{}/{}_{:0{}}.png", folder, prefix, i, pad_width);
    sf::Texture tex;
    if (!tex.loadFromFile(filename)) {
      spdlog::warn("TextureManager: Failed to load {}", filename);
      return false;
    }
    frames.push_back(std::move(tex));
  }

  spdlog::info("TextureManager: Loaded '{}' ({} frames)", anim_name, frames.size());
  return true;
}

auto TextureManager::load_all_assets(const std::string& base_path) -> Bool {
  if (m_loaded) return true;

  std::string root = base_path;
  if (!std::filesystem::exists(root)) {
    if (std::filesystem::exists("../" + base_path)) {
      root = "../" + base_path;
    }
  }

  spdlog::info("TextureManager: Loading assets from {}", root);

  // Adventurer Animations
  load_sequence("adv_idle", root + "/ADVENTURER/01-Idle/Normal", "FR_Adventurer_Idle", 12);
  load_sequence("adv_run", root + "/ADVENTURER/02-Run", "FR_Adventurer_Run", 10);
  load_sequence("adv_slash", root + "/ADVENTURER/03-Slash", "FR_Adventurer_Slash", 8);
  load_sequence("adv_hurt", root + "/ADVENTURER/06-Hurt", "FR_Adventurer_Hurt", 6);
  load_sequence("adv_dead", root + "/ADVENTURER/07-Dead", "FR_Adventurer_Dead", 9);

  // Slime Animations
  load_sequence("slime_idle", root + "/SLIME04/01-Idle", "FR_Slime4_Idle", 12);
  load_sequence("slime_move", root + "/SLIME04/02-Move", "FR_Slime4_Move", 10);
  load_sequence("slime_attack", root + "/SLIME04/03-Attack", "FR_Slime4_Attack", 8);
  load_sequence("slime_hurt", root + "/SLIME04/04-Hurt", "FR_Slime4_Hurt", 6);
  load_sequence("slime_dead", root + "/SLIME04/05-Dead", "FR_Slime4_Dead", 6);

  m_loaded = true;
  return true;
}

auto TextureManager::get_frame(const std::string& anim_name, int frame_idx) const -> const sf::Texture* {
  auto it = m_animations.find(anim_name);
  if (it == m_animations.end() || it->second.empty()) return nullptr;
  int idx = frame_idx % static_cast<int>(it->second.size());
  if (idx < 0) idx += static_cast<int>(it->second.size());
  return &it->second[idx];
}

auto TextureManager::get_frame_count(const std::string& anim_name) const -> int {
  auto it = m_animations.find(anim_name);
  if (it == m_animations.end()) return 0;
  return static_cast<int>(it->second.size());
}

}  // namespace padv
