#pragma once

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <SFML/Graphics.hpp>

#include "padv/ui/ClayRenderer.hpp"
#include "padv/ui/FrameStringArena.hpp"

namespace padv::ui {

struct InputState {
  sf::Vector2f mouse_pos{0.0f, 0.0f};
  bool         mouse_down{false};
  bool         mouse_clicked{false};
  float        scroll_delta{0.0f};
};

inline auto button(Clay_ElementId id, Clay_String label, bool active, const InputState& input, float width = 0.0f,
                   float height = 32.0f, bool enabled = true) -> bool {
  bool            clicked = false;
  Clay_SizingAxis w_axis =
      (width > 0.0f) ? CLAY_SIZING_FIXED(width) : (width < 0.0f ? CLAY_SIZING_GROW() : CLAY_SIZING_FIT());

  Clay_Color bg = enabled ? (active ? Clay_Color{35, 115, 230, 255}
                                    : (Clay_Hovered() ? Clay_Color{52, 58, 70, 255} : Clay_Color{36, 40, 48, 255}))
                          : Clay_Color{26, 28, 34, 255};

  Clay_Color border_col =
      enabled ? (active ? Clay_Color{70, 150, 255, 255} : Clay_Color{60, 66, 78, 255}) : Clay_Color{38, 42, 50, 255};

  Clay_Color text_col = enabled ? (active ? Clay_Color{255, 255, 255, 255} : Clay_Color{215, 220, 230, 255})
                                : Clay_Color{100, 105, 115, 255};

  CLAY(id,
       {.layout = {.sizing         = {w_axis, CLAY_SIZING_FIXED(height)},
                   .padding        = {10, 10, 4, 4},
                   .childAlignment = {(width < 0.0f) ? CLAY_ALIGN_X_LEFT : CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = bg,
        .cornerRadius    = CLAY_CORNER_RADIUS(4.0f),
        .border          = {.color = border_col, .width = {1, 1, 1, 1, 0}}}) {
    if (enabled && Clay_Hovered() && input.mouse_clicked) {
      clicked = true;
    }
    CLAY_TEXT(label, CLAY_TEXT_CONFIG({.textColor = text_col, .fontSize = 15, .wrapMode = CLAY_TEXT_WRAP_NONE}));
  }
  return clicked;
}

inline auto button(Clay_ElementId id, std::string_view label, bool active, const InputState& input, float width = 0.0f,
                   float height = 32.0f, bool enabled = true) -> bool {
  return button(id, intern_str(label), active, input, width, height, enabled);
}

inline auto card_header(std::string_view title) -> void {
  CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIT()}, .padding = {0, 0, 0, 8}}}) {
    CLAY_TEXT(intern_str(title),
              CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}, .fontSize = 17, .wrapMode = CLAY_TEXT_WRAP_NONE}));
  }
}

inline auto stat_row(Clay_String label, Clay_String value, Clay_Color val_color = Clay_Color{100, 215, 130, 255})
    -> void {
  CLAY_AUTO_ID({.layout = {.sizing          = {CLAY_SIZING_GROW(), CLAY_SIZING_FIT()},
                           .childAlignment  = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
                           .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIT()}}}) {
      CLAY_TEXT(label,
                CLAY_TEXT_CONFIG({.textColor = {150, 155, 168, 255}, .fontSize = 15, .wrapMode = CLAY_TEXT_WRAP_NONE}));
    }
    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(), CLAY_SIZING_FIT()}}}) {
      CLAY_TEXT(value, CLAY_TEXT_CONFIG({.textColor = val_color, .fontSize = 15, .wrapMode = CLAY_TEXT_WRAP_NONE}));
    }
  }
}

inline auto stat_row(std::string_view label, std::string_view value,
                     Clay_Color val_color = Clay_Color{100, 215, 130, 255}) -> void {
  stat_row(intern_str(label), intern_str(value), val_color);
}

inline auto format_marker_val(float val) -> std::string {
  if (val >= 10000.0f) {
    return std::format("{:.1f}k", val / 1000.0f);
  }
  if (val >= 1000.0f) {
    if (std::fmod(val, 1000.0f) == 0.0f) {
      return std::format("{:.0f}k", val / 1000.0f);
    }
    return std::format("{:.1f}k", val / 1000.0f);
  }
  if (val < 1.0f && val > 0.0f) {
    return std::format("{:.2f}", val);
  }
  if (val < 10.0f && val > 0.0f && std::fmod(val, 1.0f) != 0.0f) {
    return std::format("{:.1f}", val);
  }
  return std::format("{:.0f}", val);
}

inline auto draw_sparkline(sf::RenderWindow& window, sf::FloatRect bounds, const std::vector<float>& series1,
                           sf::Color color1, const std::vector<float>& series2 = {},
                           sf::Color color2 = sf::Color::Transparent,
                           const std::vector<float>& series3 = {},
                           sf::Color color3 = sf::Color::Transparent,
                           float custom_max = 0.0f,
                           sf::Vector2f mouse_pos = {-1.0f, -1.0f},
                           std::string_view label1 = "Val", std::string_view label2 = "Avg",
                           std::string_view label3 = "Std",
                           int total_gens = 0) -> void {
  if (bounds.size.x < 10.0f || bounds.size.y < 10.0f) return;

  sf::Font& font = ClayRenderer::instance().get_font();

  sf::RectangleShape bg(bounds.size);
  bg.setPosition(bounds.position);
  bg.setFillColor(sf::Color(18, 20, 25));
  bg.setOutlineColor(sf::Color(44, 48, 58));
  bg.setOutlineThickness(1.0f);
  window.draw(bg);

  float pad_left = 34.0f;
  float pad_right = 10.0f;
  float pad_top = 18.0f;
  float pad_bottom = 18.0f;

  float plot_x = bounds.position.x + pad_left;
  float plot_y = bounds.position.y + pad_top;
  float plot_w = bounds.size.x - (pad_left + pad_right);
  float plot_h = bounds.size.y - (pad_top + pad_bottom);

  if (plot_w <= 10.0f || plot_h <= 10.0f) return;

  float max_v = custom_max;
  if (max_v <= 0.0f) {
    for (float v : series1) max_v = std::max(max_v, v);
    for (float v : series2) max_v = std::max(max_v, v);
    for (float v : series3) max_v = std::max(max_v, v);
  }
  if (max_v <= 0.0f) max_v = 1.0f;

  float y_levels[] = {1.0f, 0.5f, 0.0f};
  for (float t : y_levels) {
    float line_y = plot_y + (1.0f - t) * plot_h;
    sf::RectangleShape hline(sf::Vector2f(plot_w, 1.0f));
    hline.setPosition(sf::Vector2f(plot_x, line_y));
    hline.setFillColor(sf::Color(36, 40, 50));
    window.draw(hline);

    float val = max_v * t;
    std::string val_str = format_marker_val(val);
    sf::Text val_txt(font, val_str, 11);
    val_txt.setFillColor(sf::Color(120, 126, 140));
    auto tb = val_txt.getLocalBounds();
    val_txt.setPosition(sf::Vector2f(plot_x - tb.size.x - 6.0f, line_y - 6.0f));
    window.draw(val_txt);
  }

  int max_gens = (total_gens > 0) ? total_gens : std::max(1, static_cast<int>(series1.size()));
  float x_levels[] = {0.0f, 0.5f, 1.0f};
  for (float t : x_levels) {
    float line_x = plot_x + t * plot_w;
    sf::RectangleShape vline(sf::Vector2f(1.0f, plot_h));
    vline.setPosition(sf::Vector2f(line_x, plot_y));
    vline.setFillColor(sf::Color(30, 34, 42));
    window.draw(vline);

    int gen_val = static_cast<int>(std::round(t * static_cast<float>(max_gens)));
    std::string gen_str = std::format("G{}", gen_val);
    sf::Text gen_txt(font, gen_str, 11);
    gen_txt.setFillColor(sf::Color(110, 115, 130));
    auto tb = gen_txt.getLocalBounds();
    float gx = line_x - tb.size.x * 0.5f;
    if (t == 0.0f) gx = plot_x;
    else if (t == 1.0f) gx = plot_x + plot_w - tb.size.x;
    gen_txt.setPosition(sf::Vector2f(gx, plot_y + plot_h + 4.0f));
    window.draw(gen_txt);
  }

  if (!series1.empty()) {
    float last_val = series1.back();
    std::string last_str = std::format("Now: {}", format_marker_val(last_val));
    sf::Text last_txt(font, last_str, 12);
    last_txt.setFillColor(color1);
    last_txt.setPosition(sf::Vector2f(plot_x, bounds.position.y + 3.0f));
    window.draw(last_txt);

    float peak_val = series1[0];
    for (float v : series1) peak_val = std::max(peak_val, v);
    std::string peak_str = std::format("Max: {}", format_marker_val(peak_val));
    sf::Text peak_txt(font, peak_str, 12);
    peak_txt.setFillColor(sf::Color(160, 170, 185));
    auto pb = peak_txt.getLocalBounds();
    peak_txt.setPosition(sf::Vector2f(plot_x + plot_w - pb.size.x, bounds.position.y + 3.0f));
    window.draw(peak_txt);
  }

  auto render_curve = [&](const std::vector<float>& s, sf::Color c) {
    if (s.empty()) return;
    if (s.size() == 1) {
      float ny = std::clamp(s[0] / max_v, 0.0f, 1.0f);
      float py = plot_y + (1.0f - ny) * plot_h;
      sf::CircleShape pt(2.5f);
      pt.setOrigin(sf::Vector2f(2.5f, 2.5f));
      pt.setPosition(sf::Vector2f(plot_x + 0.5f * plot_w, py));
      pt.setFillColor(c);
      window.draw(pt);
      return;
    }

    sf::VertexArray va(sf::PrimitiveType::LineStrip, s.size());
    size_t peak_idx = 0;
    float peak_val = s[0];

    for (size_t i = 0; i < s.size(); ++i) {
      float nx = static_cast<float>(i) / static_cast<float>(s.size() - 1);
      float ny = std::clamp(s[i] / max_v, 0.0f, 1.0f);
      float px = plot_x + nx * plot_w;
      float py = plot_y + (1.0f - ny) * plot_h;
      va[i].position = sf::Vector2f(px, py);
      va[i].color = c;

      if (s[i] > peak_val) {
        peak_val = s[i];
        peak_idx = i;
      }
    }
    window.draw(va);

    if (s.size() <= 40) {
      for (size_t i = 0; i < s.size(); ++i) {
        float nx = static_cast<float>(i) / static_cast<float>(s.size() - 1);
        float ny = std::clamp(s[i] / max_v, 0.0f, 1.0f);
        float px = plot_x + nx * plot_w;
        float py = plot_y + (1.0f - ny) * plot_h;
        sf::CircleShape pt(2.0f);
        pt.setOrigin(sf::Vector2f(2.0f, 2.0f));
        pt.setPosition(sf::Vector2f(px, py));
        pt.setFillColor(c);
        window.draw(pt);
      }
    }

    float peak_nx = static_cast<float>(peak_idx) / static_cast<float>(s.size() - 1);
    float peak_ny = std::clamp(peak_val / max_v, 0.0f, 1.0f);
    float peak_px = plot_x + peak_nx * plot_w;
    float peak_py = plot_y + (1.0f - peak_ny) * plot_h;

    sf::CircleShape pdot(3.0f);
    pdot.setOrigin(sf::Vector2f(3.0f, 3.0f));
    pdot.setPosition(sf::Vector2f(peak_px, peak_py));
    pdot.setFillColor(sf::Color::White);
    pdot.setOutlineColor(c);
    pdot.setOutlineThickness(1.5f);
    window.draw(pdot);
  };

  render_curve(series1, color1);
  if (!series2.empty() && color2 != sf::Color::Transparent) {
    render_curve(series2, color2);
  }
  if (!series3.empty() && color3 != sf::Color::Transparent) {
    render_curve(series3, color3);
  }

  if (mouse_pos.x >= plot_x && mouse_pos.x <= plot_x + plot_w &&
      mouse_pos.y >= bounds.position.y && mouse_pos.y <= bounds.position.y + bounds.size.y &&
      !series1.empty()) {
    size_t idx = 0;
    if (series1.size() > 1) {
      float frac = std::clamp((mouse_pos.x - plot_x) / plot_w, 0.0f, 1.0f);
      idx = static_cast<size_t>(std::clamp(std::round(frac * static_cast<float>(series1.size() - 1)), 0.0f, static_cast<float>(series1.size() - 1)));
    }

    float nx = (series1.size() > 1) ? (static_cast<float>(idx) / static_cast<float>(series1.size() - 1)) : 0.5f;
    float cur_x = plot_x + nx * plot_w;

    sf::RectangleShape vguide(sf::Vector2f(1.0f, plot_h));
    vguide.setPosition(sf::Vector2f(cur_x, plot_y));
    vguide.setFillColor(sf::Color(100, 150, 220, 140));
    window.draw(vguide);

    float ny1 = std::clamp(series1[idx] / max_v, 0.0f, 1.0f);
    float py1 = plot_y + (1.0f - ny1) * plot_h;

    sf::CircleShape dot1(4.0f);
    dot1.setOrigin(sf::Vector2f(4.0f, 4.0f));
    dot1.setPosition(sf::Vector2f(cur_x, py1));
    dot1.setFillColor(color1);
    dot1.setOutlineColor(sf::Color::White);
    dot1.setOutlineThickness(1.5f);
    window.draw(dot1);

    bool has_s2 = (!series2.empty() && idx < series2.size() && color2 != sf::Color::Transparent);
    if (has_s2) {
      float ny2 = std::clamp(series2[idx] / max_v, 0.0f, 1.0f);
      float py2 = plot_y + (1.0f - ny2) * plot_h;

      sf::CircleShape dot2(4.0f);
      dot2.setOrigin(sf::Vector2f(4.0f, 4.0f));
      dot2.setPosition(sf::Vector2f(cur_x, py2));
      dot2.setFillColor(color2);
      dot2.setOutlineColor(sf::Color::White);
      dot2.setOutlineThickness(1.5f);
      window.draw(dot2);
    }

    bool has_s3 = (!series3.empty() && idx < series3.size() && color3 != sf::Color::Transparent);
    if (has_s3) {
      float ny3 = std::clamp(series3[idx] / max_v, 0.0f, 1.0f);
      float py3 = plot_y + (1.0f - ny3) * plot_h;

      sf::CircleShape dot3(4.0f);
      dot3.setOrigin(sf::Vector2f(4.0f, 4.0f));
      dot3.setPosition(sf::Vector2f(cur_x, py3));
      dot3.setFillColor(color3);
      dot3.setOutlineColor(sf::Color::White);
      dot3.setOutlineThickness(1.5f);
      window.draw(dot3);
    }

    float tip_w = (has_s2 || has_s3) ? 136.0f : 104.0f;
    float tip_h = 24.0f + 16.0f + (has_s2 ? 16.0f : 0.0f) + (has_s3 ? 16.0f : 0.0f);
    float tip_x = cur_x + 10.0f;
    if (tip_x + tip_w > bounds.position.x + bounds.size.x - 4.0f) {
      tip_x = cur_x - tip_w - 10.0f;
    }
    float tip_y = std::clamp(mouse_pos.y - tip_h * 0.5f, plot_y, plot_y + plot_h - tip_h);

    sf::RectangleShape tip_bg(sf::Vector2f(tip_w, tip_h));
    tip_bg.setPosition(sf::Vector2f(tip_x, tip_y));
    tip_bg.setFillColor(sf::Color(14, 16, 22, 245));
    tip_bg.setOutlineColor(sf::Color(60, 70, 90));
    tip_bg.setOutlineThickness(1.0f);
    window.draw(tip_bg);

    sf::Text t_gen(font, std::format("Gen {}", idx + 1), 12);
    t_gen.setFillColor(sf::Color(220, 225, 235));
    t_gen.setPosition(sf::Vector2f(tip_x + 6.0f, tip_y + 3.0f));
    window.draw(t_gen);

    sf::Text t_val1(font, std::format("{}: {}", label1, format_marker_val(series1[idx])), 12);
    t_val1.setFillColor(color1);
    t_val1.setPosition(sf::Vector2f(tip_x + 6.0f, tip_y + 19.0f));
    window.draw(t_val1);

    float text_y = tip_y + 35.0f;
    if (has_s2) {
      sf::Text t_val2(font, std::format("{}: {}", label2, format_marker_val(series2[idx])), 12);
      t_val2.setFillColor(color2);
      t_val2.setPosition(sf::Vector2f(tip_x + 6.0f, text_y));
      window.draw(t_val2);
      text_y += 16.0f;
    }

    if (has_s3) {
      sf::Text t_val3(font, std::format("{}: {}", label3, format_marker_val(series3[idx])), 12);
      t_val3.setFillColor(color3);
      t_val3.setPosition(sf::Vector2f(tip_x + 6.0f, text_y));
      window.draw(t_val3);
    }
  }
}

}
