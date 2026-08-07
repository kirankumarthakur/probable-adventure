#include <string>

#include <spdlog/spdlog.h>

#include "padv/AiManager.hpp"

auto setup_logger() -> void { spdlog::set_pattern("[%T] [%^%l%$] %v"); }

auto main(int argc, char** argv) -> int {
  setup_logger();

  int   pop      = 10;
  int   gen      = 20;
  float mut_rate = 0.1f;
  float mut_str  = 0.5f;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--pop" && i + 1 < argc) {
      pop = std::stoi(argv[++i]);
    } else if (arg == "--gen" && i + 1 < argc) {
      gen = std::stoi(argv[++i]);
    } else if (arg == "--mut-rate" && i + 1 < argc) {
      mut_rate = std::stof(argv[++i]);
    } else if (arg == "--mut-str" && i + 1 < argc) {
      mut_str = std::stof(argv[++i]);
    }
  }

  spdlog::info("Starting Engine with Multithreaded AI Manager");
  spdlog::info("Config: pop={}, gen={}, mut_rate={:.2f}, mut_str={:.2f}", pop, gen, mut_rate, mut_str);

  padv::AiManager manager;
  manager.population_size   = pop;
  manager.generations       = gen;
  manager.mutation_rate     = mut_rate;
  manager.mutation_strength = mut_str;

  // 21 inputs: HP, Stamina, 11 Vision Rays, 8 Sound Rays
  manager.initialize(21, 24, 5);
  manager.train();

  return 0;
}