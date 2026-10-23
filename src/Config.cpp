#include "padv/Config.hpp"

#include <fstream>
#include <sstream>

#include <spdlog/spdlog.h>

namespace padv {

auto AppConfig::save_to_file(const std::string& path) const -> Bool {
  std::ofstream os(path);
  if (!os.is_open()) return false;

  os << "font_path=" << font_path << "\n";
  os << "font_size=" << font_size << "\n";
  os << "ui_scale=" << ui_scale << "\n";
  os << "population_size=" << population_size << "\n";
  os << "generations=" << generations << "\n";
  os << "mutation_rate=" << mutation_rate << "\n";
  os << "mutation_strength=" << mutation_strength << "\n";
  os << "tournament_size=" << tournament_size << "\n";
  os << "elitism_count=" << elitism_count << "\n";
  os << "max_eval_ticks=" << max_eval_ticks << "\n";
  os << "checkpoint_policy=" << static_cast<int>(checkpoint_policy) << "\n";
  os << "top_k_count=" << top_k_count << "\n";
  os << "periodic_interval=" << periodic_interval << "\n";

  return os.good();
}

auto AppConfig::load_from_file(const std::string& path) -> Bool {
  std::ifstream is(path);
  if (!is.is_open()) return false;

  std::string line;
  while (std::getline(is, line)) {
    auto eq_pos = line.find('=');
    if (eq_pos == std::string::npos) continue;

    std::string key = line.substr(0, eq_pos);
    std::string val = line.substr(eq_pos + 1);

    try {
      if (key == "font_path") font_path = val;
      else if (key == "font_size") font_size = std::stof(val);
      else if (key == "ui_scale") ui_scale = std::stof(val);
      else if (key == "population_size") population_size = std::stoi(val);
      else if (key == "generations") generations = std::stoi(val);
      else if (key == "mutation_rate") mutation_rate = std::stof(val);
      else if (key == "mutation_strength") mutation_strength = std::stof(val);
      else if (key == "tournament_size") tournament_size = std::stoi(val);
      else if (key == "elitism_count") elitism_count = std::stoi(val);
      else if (key == "max_eval_ticks") max_eval_ticks = std::stoll(val);
      else if (key == "checkpoint_policy") checkpoint_policy = static_cast<CheckpointRetention>(std::stoi(val));
      else if (key == "top_k_count") top_k_count = std::stoi(val);
      else if (key == "periodic_interval") periodic_interval = std::stoi(val);
    } catch (...) {
    }
  }

  spdlog::info("AppConfig: Loaded configuration from {}", path);
  return true;
}

}  // namespace padv
