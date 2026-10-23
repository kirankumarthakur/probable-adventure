#pragma once

#include <string>

#include "Types.hpp"

namespace padv {

enum class CheckpointRetention { TopK, Periodic, BestOnly, All };

struct AppConfig {
  std::string font_path = "C:/Windows/Fonts/segoeui.ttf";
  float       font_size = 18.0f;
  float       ui_scale  = 1.0f;

  I32   population_size   = 30;
  I32   generations       = 50;
  float mutation_rate     = 0.1f;
  float mutation_strength = 0.35f;
  I32   tournament_size   = 3;
  I32   elitism_count     = 3;
  I64   max_eval_ticks    = 3000;

  CheckpointRetention checkpoint_policy = CheckpointRetention::TopK;
  I32                 top_k_count       = 10;
  I32                 periodic_interval = 5;

  auto save_to_file(const std::string& path = "config.json") const -> Bool;
  auto load_from_file(const std::string& path = "config.json") -> Bool;
};

}  // namespace padv
