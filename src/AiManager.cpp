#include "padv/AiManager.hpp"

#include <algorithm>
#include <fstream>
#include <future>
#include <utility>

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <spdlog/spdlog.h>

#include "padv/Simulation.hpp"

namespace padv {

bool AiManager::load_brain(const std::string& filename, MlpBrain& brain) {
  std::ifstream is(filename, std::ios::binary);
  if (is.is_open()) {
    try {
      cereal::BinaryInputArchive archive(is);
      archive(brain);
      return true;
    } catch (...) {
      return false;
    }
  }
  return false;
}

void AiManager::save_brain(const std::string& filename, const MlpBrain& brain) {
  std::ofstream os(filename, std::ios::binary);
  if (os.is_open()) {
    cereal::BinaryOutputArchive archive(os);
    archive(brain);
  }
}

void AiManager::initialize(I32 in, I32 hid, I32 out) {
  std::uniform_real_distribution<F32> dist(-1.0f, 1.0f);

  population.clear();

  MlpBrain seed_brain;
  bool     has_seed = load_brain("best_brain.dat", seed_brain);

  // Discard saved brain if structural configuration (inputs/outputs) changed
  if (has_seed && (seed_brain.input_size != in || seed_brain.hidden_size != hid || seed_brain.output_size != out)) {
    spdlog::warn("Saved brain dimensions do not match current config. Discarding best_brain.dat.");
    has_seed = false;
  }

  for (I32 i = 0; i < population_size; ++i) {
    MlpBrain brain;

    if (has_seed) {
      brain = seed_brain;
      // Index 0 stays identical to the best saved brain. Mutate the rest.
      if (i > 0) {
        mutate(brain);
      }
    } else {
      brain.input_size  = in;
      brain.hidden_size = hid;
      brain.output_size = out;

      brain.hidden_weights.resize(in * hid);
      for (auto& w : brain.hidden_weights) w = dist(rng);

      brain.hidden_biases.resize(hid);
      for (auto& b : brain.hidden_biases) b = dist(rng);

      brain.output_weights.resize(hid * out);
      for (auto& w : brain.output_weights) w = dist(rng);

      brain.output_biases.resize(out);
      for (auto& b : brain.output_biases) b = dist(rng);
    }

    population.push_back(std::move(brain));
  }

  if (has_seed) {
    spdlog::info("Initialized population using saved brain from best_brain.dat");
  } else {
    spdlog::info("Initialized population randomly");
  }
}

MlpBrain AiManager::crossover(const MlpBrain& parent1, const MlpBrain& parent2) {
  MlpBrain child = parent1;

  std::uniform_int_distribution<Size> hidden_weight_dist(0, parent1.hidden_weights.size() - 1);
  std::uniform_int_distribution<Size> hidden_bias_dist(0, parent1.hidden_biases.size() - 1);
  std::uniform_int_distribution<Size> output_weight_dist(0, parent1.output_weights.size() - 1);
  std::uniform_int_distribution<Size> output_bias_dist(0, parent1.output_biases.size() - 1);

  Size hidden_weight_split = hidden_weight_dist(rng);
  Size hidden_bias_split   = hidden_bias_dist(rng);
  Size output_weight_split = output_weight_dist(rng);
  Size output_bias_split   = output_bias_dist(rng);

  for (Size i = hidden_weight_split; i < child.hidden_weights.size(); ++i) {
    child.hidden_weights[i] = parent2.hidden_weights[i];
  }

  for (Size i = hidden_bias_split; i < child.hidden_biases.size(); ++i) {
    child.hidden_biases[i] = parent2.hidden_biases[i];
  }

  for (Size i = output_weight_split; i < child.output_weights.size(); ++i) {
    child.output_weights[i] = parent2.output_weights[i];
  }

  for (Size i = output_bias_split; i < child.output_biases.size(); ++i) {
    child.output_biases[i] = parent2.output_biases[i];
  }

  return child;
}

void AiManager::mutate(MlpBrain& brain) {
  std::uniform_real_distribution<F32> chance(0.0f, 1.0f);
  std::uniform_real_distribution<F32> shift(-mutation_strength, mutation_strength);
  std::uniform_real_distribution<F32> reset(-1.0f, 1.0f);

  auto mutate_value = [&](F32& value) {
    F32 roll = chance(rng);

    if (roll < mutation_rate) {
      value += shift(rng);
    } else if (roll < mutation_rate + 0.02f) {
      value = reset(rng);
    }

    value = std::clamp(value, static_cast<F32>(-5.0f), static_cast<F32>(5.0f));
  };

  for (auto& w : brain.hidden_weights) mutate_value(w);
  for (auto& b : brain.hidden_biases) mutate_value(b);
  for (auto& w : brain.output_weights) mutate_value(w);
  for (auto& b : brain.output_biases) mutate_value(b);
}

void AiManager::train() {
  for (I32 generation = 0; generation < generations; ++generation) {
    std::vector<std::future<std::pair<F32, MlpBrain>>> futures;

    for (I32 i = 0; i < population_size; ++i) {
      futures.push_back(std::async(std::launch::async, [this, generation, i]() {
        F32 fitness = play_game(population[i], generation, "dummy", false);
        return std::make_pair(fitness, population[i]);
      }));
    }

    std::vector<std::pair<F32, MlpBrain>> scored;

    for (auto& future : futures) {
      scored.push_back(future.get());
    }

    std::sort(scored.begin(), scored.end(), [](const auto& lhs, const auto& rhs) { return lhs.first > rhs.first; });

    spdlog::info("Generation {} completed. Best fitness: {:.1f}", generation, scored.front().first);

    play_game(scored.front().second, generation, "best", true);
    save_brain("best_brain.dat", scored.front().second);

    std::vector<MlpBrain> next_generation;

    next_generation.push_back(scored[0].second);
    next_generation.push_back(scored[1].second);

    while (next_generation.size() < static_cast<Size>(population_size)) {
      MlpBrain child = crossover(scored[0].second, scored[1].second);
      mutate(child);
      next_generation.push_back(std::move(child));
    }

    population = std::move(next_generation);
  }
}

}  // namespace padv