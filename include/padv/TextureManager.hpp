#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <SFML/Graphics.hpp>

#include "Types.hpp"

namespace padv {

class TextureManager {
 public:
  static auto instance() -> TextureManager&;

  auto load_all_assets(const std::string& base_path = "assets") -> Bool;

  [[nodiscard]] auto get_frame(const std::string& anim_name, int frame_idx) const -> const sf::Texture*;
  [[nodiscard]] auto get_frame_count(const std::string& anim_name) const -> int;

 private:
  TextureManager() = default;
  std::unordered_map<std::string, std::vector<sf::Texture>> m_animations;
  bool m_loaded = false;

  auto load_sequence(const std::string& anim_name, const std::string& folder,
                     const std::string& prefix, int count, int pad_width = 3) -> bool;
};

}  // namespace padv
