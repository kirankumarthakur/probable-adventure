#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <SFML/Graphics.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4201)
#pragma warning(disable: 4100)
#pragma warning(disable: 4244)
#endif

#include "clay.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace padv::ui {

inline auto to_clay_string(std::string_view sv) -> Clay_String {
  return Clay_String{false, static_cast<int32_t>(sv.size()), sv.data()};
}

class ClayRenderer {
 public:
  static auto instance() -> ClayRenderer&;

  auto init(sf::RenderWindow& window) -> bool;
  auto shutdown() -> void;

  auto begin_frame(sf::Vector2f window_size, sf::Vector2f mouse_pos, bool mouse_down, float scroll_delta) -> void;
  auto end_frame(sf::RenderWindow& window) -> Clay_RenderCommandArray;

  auto get_font() -> sf::Font& { return font_; }

 private:
  ClayRenderer() = default;
  ~ClayRenderer();

  sf::Font font_;
  char* clay_memory_{nullptr};
  size_t memory_size_{0};
};

}
