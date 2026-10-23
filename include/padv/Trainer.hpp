#pragma once

#include <atomic>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "CheckpointManager.hpp"
#include "ModalPolicy.hpp"
#include "Types.hpp"

namespace padv {

struct RunMetrics {
  I32         generation          = 0;
  F32         best_fitness        = 0.0f;
  F32         avg_fitness         = 0.0f;
  F32         min_fitness         = 0.0f;
  F32         std_fitness         = 0.0f;
  F32         best_error          = 0.0f;
  F32         avg_error           = 0.0f;
  F32         std_error           = 0.0f;
  F32         best_survival_ticks = 0.0f;
  F32         avg_survival_ticks  = 0.0f;
  F32         std_survival_ticks  = 0.0f;
  F32         best_kills          = 0.0f;
  F32         avg_kills           = 0.0f;
  F32         std_kills           = 0.0f;
  F32         best_damage         = 0.0f;
  F32         avg_damage          = 0.0f;
  F32         std_damage          = 0.0f;
  F32         best_weight_norm    = 0.0f;
  F32         avg_weight_norm     = 0.0f;
  F32         std_weight_norm     = 0.0f;
  F32         mlp_weight_norm     = 0.0f;
  F32         mlp_weight_var      = 0.0f;
  I32         cleave_count        = 0;
  I32         shield_count        = 0;
  I32         warcry_count        = 0;
  I32         wall_hug_ticks      = 0;
  F32         damage_dealt        = 0.0f;
  F32         damage_taken        = 0.0f;
  std::string checkpoint_path;
};

class Trainer {
 public:
  I32 population_size   = 30;
  I32 generations       = 50;
  F32 mutation_rate     = 0.1f;
  F32 mutation_strength = 0.35f;
  I32 tournament_size   = 3;
  I32 elitism_count     = 3;
  I64 max_eval_ticks    = 3000;

  std::vector<ModalPolicy> population;
  ModalPolicy              best_sample;
  std::mt19937             rng{std::random_device{}()};

  CheckpointManager checkpoint_mgr{"checkpoints"};

  std::atomic<bool>  is_training{false};
  std::atomic<bool>  stop_requested{false};
  std::atomic<I32>   current_generation{0};
  std::atomic<float> current_best_fitness{0.0f};
  std::atomic<float> current_avg_fitness{0.0f};

  mutable std::mutex      history_mutex;
  std::vector<float>      history_best_fitness;
  std::vector<float>      history_avg_fitness;
  std::vector<float>      history_std_fitness;
  std::vector<float>      history_mlp_weight_norm;
  std::vector<float>      history_mlp_weight_var;
  std::vector<RunMetrics> run_history;

  Trainer();
  ~Trainer();

  auto initialize(I32 pop_size, I32 gens) -> void;
  auto train() -> void;
  auto start_background_training() -> void;
  auto stop_training() -> void;

  [[nodiscard]] auto get_best_sample() const -> const ModalPolicy&;
  [[nodiscard]] auto get_history_metrics() const -> std::vector<RunMetrics>;
  auto save_best_sample(const std::string& filename) const -> Bool;
  auto load_best_sample(const std::string& filename, ModalPolicy& out_policy) -> Bool;

 private:
  std::thread worker_thread_;
};

}  // namespace padv
