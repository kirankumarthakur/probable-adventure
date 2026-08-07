#pragma once

#include <random>
#include <string>
#include <vector>

#include "Components.hpp"

namespace padv {

struct AiManager {
  I32 population_size = 10;
  I32 generations     = 20;

  F32 mutation_rate     = 0.1f;
  F32 mutation_strength = 0.5f;

  std::vector<MlpBrain> population;

  std::mt19937 rng{std::random_device{}()};

  void initialize(I32 in, I32 hid, I32 out);

  MlpBrain crossover(const MlpBrain&, const MlpBrain&);

  void mutate(MlpBrain&);

  void train();

  bool load_brain(const std::string& filename, MlpBrain& brain);

  void save_brain(const std::string& filename, const MlpBrain& brain);
};

}  // namespace padv