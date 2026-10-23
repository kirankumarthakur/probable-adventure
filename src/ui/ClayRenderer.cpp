#define CLAY_IMPLEMENTATION
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4201)
#pragma warning(disable: 4100)
#pragma warning(disable: 4244)
#pragma warning(disable: 4305)
#pragma warning(disable: 4189)
#pragma warning(disable: 4389)
#endif
#include "clay.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "padv/ui/ClayRenderer.hpp"
#include "padv/ui/FrameStringArena.hpp"

#include <algorithm>
#include <cstdlib>

namespace padv::ui {

namespace {

auto measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* user_data) -> Clay_Dimensions {
  auto* font = static_cast<sf::Font*>(user_data);
  if (!font) return {0.0f, 0.0f};
  sf::String sf_str = sf::String::fromUtf8(text.chars, text.chars + text.length);
  sf::Text sf_text(*font, sf_str, config->fontSize);
  auto bounds = sf_text.getLocalBounds();
  float w = bounds.size.x + bounds.position.x + 4.0f;
  float h = static_cast<float>(config->fontSize) * 1.35f;
  return {w, h};
}

auto handle_clay_error(Clay_ErrorData error_data) -> void {
  (void)error_data;
}

auto draw_rounded_rect(sf::RenderWindow& window, sf::Vector2f pos, sf::Vector2f size, float radius, sf::Color color) -> void {
  if (radius <= 1.0f) {
    sf::RectangleShape rect(size);
    rect.setPosition(pos);
    rect.setFillColor(color);
    window.draw(rect);
    return;
  }

  radius = std::min({radius, size.x * 0.5f, size.y * 0.5f});

  sf::RectangleShape center(sf::Vector2f(size.x - 2.0f * radius, size.y));
  center.setPosition(sf::Vector2f(pos.x + radius, pos.y));
  center.setFillColor(color);
  window.draw(center);

  sf::RectangleShape left(sf::Vector2f(radius, size.y - 2.0f * radius));
  left.setPosition(sf::Vector2f(pos.x, pos.y + radius));
  left.setFillColor(color);
  window.draw(left);

  sf::RectangleShape right(sf::Vector2f(radius, size.y - 2.0f * radius));
  right.setPosition(sf::Vector2f(pos.x + size.x - radius, pos.y + radius));
  right.setFillColor(color);
  window.draw(right);

  sf::CircleShape corner(radius);
  corner.setFillColor(color);

  corner.setPosition(pos);
  window.draw(corner);

  corner.setPosition(sf::Vector2f(pos.x + size.x - 2.0f * radius, pos.y));
  window.draw(corner);

  corner.setPosition(sf::Vector2f(pos.x, pos.y + size.y - 2.0f * radius));
  window.draw(corner);

  corner.setPosition(sf::Vector2f(pos.x + size.x - 2.0f * radius, pos.y + size.y - 2.0f * radius));
  window.draw(corner);
}

}

auto ClayRenderer::instance() -> ClayRenderer& {
  static ClayRenderer s_instance;
  return s_instance;
}

ClayRenderer::~ClayRenderer() {
  shutdown();
}

auto ClayRenderer::init(sf::RenderWindow& window) -> bool {
  if (!font_.openFromFile("C:/Windows/Fonts/segoeui.ttf")) {
    if (!font_.openFromFile("C:/Windows/Fonts/arial.ttf")) {
      return false;
    }
  }

  uint64_t total_memory = Clay_MinMemorySize();
  memory_size_ = static_cast<size_t>(total_memory);
  clay_memory_ = static_cast<char*>(std::malloc(memory_size_));
  if (!clay_memory_) return false;

  Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(total_memory, clay_memory_);

  Clay_Initialize(arena,
                  Clay_Dimensions{static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)},
                  Clay_ErrorHandler{handle_clay_error});

  Clay_SetMeasureTextFunction(measure_text, &font_);

  return true;
}

auto ClayRenderer::shutdown() -> void {
  if (clay_memory_) {
    std::free(clay_memory_);
    clay_memory_ = nullptr;
    memory_size_ = 0;
  }
}

auto ClayRenderer::begin_frame(sf::Vector2f window_size, sf::Vector2f mouse_pos, bool mouse_down, float scroll_delta) -> void {
  FrameStringArena::instance().clear();
  Clay_SetLayoutDimensions(Clay_Dimensions{window_size.x, window_size.y});
  Clay_SetPointerState(Clay_Vector2{mouse_pos.x, mouse_pos.y}, mouse_down);
  Clay_UpdateScrollContainers(false, Clay_Vector2{0.0f, scroll_delta}, 0.016f);
  Clay_BeginLayout();
}

auto ClayRenderer::end_frame(sf::RenderWindow& window) -> Clay_RenderCommandArray {
  Clay_RenderCommandArray commands = Clay_EndLayout(0.016f);

  for (int i = 0; i < commands.length; ++i) {
    Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&commands, i);
    sf::Vector2f pos(cmd->boundingBox.x, cmd->boundingBox.y);
    sf::Vector2f size(cmd->boundingBox.width, cmd->boundingBox.height);

    switch (cmd->commandType) {
      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
        const auto& c = cmd->renderData.rectangle.backgroundColor;
        sf::Color sf_color(static_cast<std::uint8_t>(c.r),
                           static_cast<std::uint8_t>(c.g),
                           static_cast<std::uint8_t>(c.b),
                           static_cast<std::uint8_t>(c.a));
        float radius = cmd->renderData.rectangle.cornerRadius.topLeft;
        draw_rounded_rect(window, pos, size, radius, sf_color);
        break;
      }

      case CLAY_RENDER_COMMAND_TYPE_BORDER: {
        const auto& bc = cmd->renderData.border.color;
        sf::Color sf_border(static_cast<std::uint8_t>(bc.r),
                            static_cast<std::uint8_t>(bc.g),
                            static_cast<std::uint8_t>(bc.b),
                            static_cast<std::uint8_t>(bc.a));
        const auto& bw = cmd->renderData.border.width;

        if (bw.top > 0) {
          sf::RectangleShape top_line(sf::Vector2f(size.x, static_cast<float>(bw.top)));
          top_line.setPosition(pos);
          top_line.setFillColor(sf_border);
          window.draw(top_line);
        }
        if (bw.bottom > 0) {
          sf::RectangleShape btm_line(sf::Vector2f(size.x, static_cast<float>(bw.bottom)));
          btm_line.setPosition(sf::Vector2f(pos.x, pos.y + size.y - bw.bottom));
          btm_line.setFillColor(sf_border);
          window.draw(btm_line);
        }
        if (bw.left > 0) {
          sf::RectangleShape left_line(sf::Vector2f(static_cast<float>(bw.left), size.y));
          left_line.setPosition(pos);
          left_line.setFillColor(sf_border);
          window.draw(left_line);
        }
        if (bw.right > 0) {
          sf::RectangleShape right_line(sf::Vector2f(static_cast<float>(bw.right), size.y));
          right_line.setPosition(sf::Vector2f(pos.x + size.x - bw.right, pos.y));
          right_line.setFillColor(sf_border);
          window.draw(right_line);
        }
        break;
      }

      case CLAY_RENDER_COMMAND_TYPE_TEXT: {
        const auto& tc = cmd->renderData.text.textColor;
        sf::Color sf_text_col(static_cast<std::uint8_t>(tc.r),
                              static_cast<std::uint8_t>(tc.g),
                              static_cast<std::uint8_t>(tc.b),
                              static_cast<std::uint8_t>(tc.a));

        sf::String sf_str = sf::String::fromUtf8(cmd->renderData.text.stringContents.chars,
                                                 cmd->renderData.text.stringContents.chars + cmd->renderData.text.stringContents.length);
        sf::Text text(font_, sf_str, cmd->renderData.text.fontSize);
        text.setPosition(pos);
        text.setFillColor(sf_text_col);
        window.draw(text);
        break;
      }

      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
        break;
      }

      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
        break;
      }

      default:
        break;
    }
  }

  return commands;
}

}
