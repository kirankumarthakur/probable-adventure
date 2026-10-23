#include <iostream>
#include <optional>
#include <string>

#include <spdlog/spdlog.h>

#include "padv/Config.hpp"
#include "padv/TextureManager.hpp"
#include "padv/Trainer.hpp"
#include "padv/ui/App.hpp"

namespace {

auto setup_logger() -> void {
  spdlog::set_pattern("[%T] [%^%l%$] %v");
}

auto run_headless(padv::AppConfig config) -> int {
  spdlog::info("Running Probable Adventure Engine in HEADLESS mode");
  spdlog::info("Config: pop={}, gen={}, mut_rate={:.2f}, mut_str={:.2f}",
               config.population_size, config.generations, config.mutation_rate, config.mutation_strength);

  padv::Trainer trainer;
  trainer.population_size   = config.population_size;
  trainer.generations       = config.generations;
  trainer.mutation_rate     = config.mutation_rate;
  trainer.mutation_strength = config.mutation_strength;
  trainer.tournament_size   = config.tournament_size;
  trainer.elitism_count     = config.elitism_count;
  trainer.max_eval_ticks    = config.max_eval_ticks;
  trainer.checkpoint_mgr.set_policy(config.checkpoint_policy, config.top_k_count, config.periodic_interval);

  trainer.initialize(config.population_size, config.generations);
  trainer.train();

  spdlog::info("Headless training completed successfully.");
  return 0;
}

auto run_gui_application(padv::AppConfig config) -> int {
  auto& app = padv::ui::App::instance();
  if (!app.init(config)) {
    return 1;
  }
  return app.run();
}

}

auto main(int argc, char** argv) -> int {
  setup_logger();

  padv::AppConfig config;
  config.load_from_file("config.json");

  bool headless = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--headless") {
      headless = true;
    } else if (arg == "--pop" && i + 1 < argc) {
      config.population_size = std::stoi(argv[++i]);
    } else if (arg == "--gen" && i + 1 < argc) {
      config.generations = std::stoi(argv[++i]);
    } else if (arg == "--mut-rate" && i + 1 < argc) {
      config.mutation_rate = std::stof(argv[++i]);
    } else if (arg == "--mut-str" && i + 1 < argc) {
      config.mutation_strength = std::stof(argv[++i]);
    } else if (arg == "--config" && i + 1 < argc) {
      config.load_from_file(argv[++i]);
    }
  }

  if (headless) {
    return run_headless(config);
  }

  return run_gui_application(config);
}
