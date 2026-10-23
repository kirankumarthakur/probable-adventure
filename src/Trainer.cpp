#include "padv/Trainer.hpp"

#include <algorithm>
#include <future>
#include <numeric>
#include <utility>

#include <spdlog/spdlog.h>

#include "padv/Arena.hpp"

namespace padv {

Trainer::Trainer() = default;

Trainer::~Trainer() {
  stop_training();
}

auto Trainer::start_background_training() -> void {
  if (is_training.load()) return;
  stop_requested = false;
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
  worker_thread_ = std::thread([this]() {
    train();
  });
}

auto Trainer::stop_training() -> void {
  stop_requested = true;
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}

auto Trainer::initialize(I32 pop_size, I32 gens) -> void {
  population_size = pop_size;
  generations     = gens;
  population.clear();

  {
    std::lock_guard<std::mutex> lock(history_mutex);
    history_best_fitness.clear();
    history_avg_fitness.clear();
    history_std_fitness.clear();
    history_mlp_weight_norm.clear();
    history_mlp_weight_var.clear();
    run_history.clear();
  }
  current_generation = 0;
  current_best_fitness = 0.0f;
  current_avg_fitness = 0.0f;

  ModalPolicy seed_policy;
  bool has_seed = load_best_sample("best_brain.safetensors", seed_policy);

  for (I32 i = 0; i < population_size; ++i) {
    ModalPolicy policy;
    if (has_seed) {
      policy = seed_policy;
      if (i > 0) {
        policy.mutate(mutation_rate, mutation_strength, rng);
      }
    } else {
      policy.initialize(rng);
    }
    population.push_back(std::move(policy));
  }

  if (has_seed) {
    spdlog::info("Trainer: Initialized population using saved best sample from best_brain.safetensors");
  } else {
    spdlog::info("Trainer: Initialized population randomly");
  }
}

auto Trainer::train() -> void {
  is_training = true;
  stop_requested = false;

  for (I32 generation = 0; generation < generations; ++generation) {
    if (stop_requested.load()) {
      spdlog::info("Trainer: Training stopped early by user request at generation {}.", generation);
      break;
    }

    current_generation = generation;

    struct AgentResult {
      F32         fitness;
      ModalPolicy policy;
      Arena       arena;
    };

    std::vector<std::future<AgentResult>> futures;
    for (I32 i = 0; i < population_size; ++i) {
      futures.push_back(std::async(std::launch::async, [this, i]() {
        Arena trial_arena;
        F32   fitness = run_arena_trial(population[i], max_eval_ticks, &trial_arena);
        return AgentResult{fitness, population[i], trial_arena};
      }));
    }

    std::vector<AgentResult> scored;
    scored.reserve(futures.size());
    for (auto& future : futures) {
      scored.push_back(future.get());
    }

    std::sort(scored.begin(), scored.end(), [](const auto& lhs, const auto& rhs) {
      return lhs.fitness > rhs.fitness;
    });

    best_sample = scored.front().policy;
    const auto& top_arena = scored.front().arena;

    F32 sum_fit = 0.0f;
    F32 sum_surv = 0.0f;
    F32 sum_kills = 0.0f;
    F32 sum_dmg = 0.0f;
    F32 sum_wnorm = 0.0f;
    std::vector<F32> pop_wnorms;
    pop_wnorms.reserve(scored.size());

    for (const auto& item : scored) {
      sum_fit += item.fitness;
      sum_surv += static_cast<F32>(item.arena.ticks);
      sum_kills += static_cast<F32>(item.arena.slimes_killed);
      sum_dmg += item.arena.damage_dealt;

      float w_sq = 0.0f;
      for (const auto& layer : item.policy.model.get_layers()) {
        w_sq += layer.weights.squaredNorm();
        w_sq += layer.bias.squaredNorm();
      }
      float wn = std::sqrt(w_sq);
      pop_wnorms.push_back(wn);
      sum_wnorm += wn;
    }

    F32 n_pop = static_cast<F32>(scored.size());
    F32 avg_fit = sum_fit / n_pop;
    F32 avg_surv = sum_surv / n_pop;
    F32 avg_kills = sum_kills / n_pop;
    F32 avg_dmg = sum_dmg / n_pop;
    F32 avg_wnorm = sum_wnorm / n_pop;

    F32 var_fit = 0.0f;
    F32 var_surv = 0.0f;
    F32 var_kills = 0.0f;
    F32 var_dmg = 0.0f;
    F32 var_wnorm = 0.0f;

    for (size_t i = 0; i < scored.size(); ++i) {
      F32 df = scored[i].fitness - avg_fit;
      var_fit += df * df;
      F32 ds = static_cast<F32>(scored[i].arena.ticks) - avg_surv;
      var_surv += ds * ds;
      F32 dk = static_cast<F32>(scored[i].arena.slimes_killed) - avg_kills;
      var_kills += dk * dk;
      F32 dd = scored[i].arena.damage_dealt - avg_dmg;
      var_dmg += dd * dd;
      F32 dw = pop_wnorms[i] - avg_wnorm;
      var_wnorm += dw * dw;
    }

    F32 std_fit = std::sqrt(var_fit / n_pop);
    F32 std_surv = std::sqrt(var_surv / n_pop);
    F32 std_kills = std::sqrt(var_kills / n_pop);
    F32 std_dmg = std::sqrt(var_dmg / n_pop);
    F32 std_wnorm = std::sqrt(var_wnorm / n_pop);

    F32 best_fit = scored.front().fitness;
    F32 min_fit = scored.back().fitness;

    current_best_fitness = static_cast<float>(best_fit);
    current_avg_fitness = static_cast<float>(avg_fit);

    RunMetrics metrics;
    metrics.generation = generation;
    metrics.best_fitness = best_fit;
    metrics.avg_fitness = avg_fit;
    metrics.min_fitness = min_fit;
    metrics.std_fitness = std_fit;

    metrics.best_error = std::max(0.0f, best_fit - avg_fit);
    metrics.avg_error = avg_fit;
    metrics.std_error = std_fit;

    metrics.best_survival_ticks = static_cast<F32>(top_arena.ticks);
    metrics.avg_survival_ticks = avg_surv;
    metrics.std_survival_ticks = std_surv;

    metrics.best_kills = static_cast<F32>(top_arena.slimes_killed);
    metrics.avg_kills = avg_kills;
    metrics.std_kills = std_kills;

    metrics.best_damage = top_arena.damage_dealt;
    metrics.avg_damage = avg_dmg;
    metrics.std_damage = std_dmg;

    metrics.best_weight_norm = pop_wnorms.front();
    metrics.avg_weight_norm = avg_wnorm;
    metrics.std_weight_norm = std_wnorm;

    metrics.mlp_weight_norm = pop_wnorms.front();
    metrics.mlp_weight_var = std_wnorm * std_wnorm;
    metrics.cleave_count = top_arena.cleave_count;
    metrics.shield_count = top_arena.shield_count;
    metrics.warcry_count = top_arena.warcry_count;
    metrics.wall_hug_ticks = top_arena.wall_hug_ticks;
    metrics.damage_dealt = top_arena.damage_dealt;
    metrics.damage_taken = top_arena.damage_taken;

    {
      std::lock_guard<std::mutex> lock(history_mutex);
      history_best_fitness.push_back(static_cast<float>(best_fit));
      history_avg_fitness.push_back(static_cast<float>(avg_fit));
      history_std_fitness.push_back(static_cast<float>(std_fit));
      history_mlp_weight_norm.push_back(static_cast<float>(pop_wnorms.front()));
      history_mlp_weight_var.push_back(static_cast<float>(std_wnorm * std_wnorm));
      run_history.push_back(metrics);
    }

    spdlog::info("Generation {} complete. Best fitness: {:.1f}, Avg fitness: {:.1f}",
                 generation, best_fit, avg_fit);

    save_best_sample("best_brain.safetensors");
    checkpoint_mgr.record_checkpoint(generation, best_fit, best_sample);

    if (stop_requested.load() || generation == generations - 1) {
      break;
    }

    std::vector<ModalPolicy> next_gen;
    next_gen.reserve(population_size);

    I32 elites = std::min<I32>(elitism_count, static_cast<I32>(scored.size()));
    for (I32 e = 0; e < elites; ++e) {
      next_gen.push_back(scored[e].policy);
    }

    std::uniform_int_distribution<Size> pop_dist(0, scored.size() - 1);
    auto select_parent = [&]() -> const ModalPolicy& {
      Size best_idx = pop_dist(rng);
      for (I32 t = 1; t < tournament_size; ++t) {
        Size candidate = pop_dist(rng);
        if (scored[candidate].fitness > scored[best_idx].fitness) {
          best_idx = candidate;
        }
      }
      return scored[best_idx].policy;
    };

    while (next_gen.size() < static_cast<Size>(population_size)) {
      const auto& parent1 = select_parent();
      const auto& parent2 = select_parent();
      ModalPolicy child   = ModalPolicy::crossover(parent1, parent2, rng);
      child.mutate(mutation_rate, mutation_strength, rng);
      next_gen.push_back(std::move(child));
    }

    population = std::move(next_gen);
  }

  is_training = false;
  spdlog::info("Trainer: Training process completed.");
}

auto Trainer::get_best_sample() const -> const ModalPolicy& {
  return best_sample;
}

auto Trainer::get_history_metrics() const -> std::vector<RunMetrics> {
  std::lock_guard<std::mutex> lock(history_mutex);
  return run_history;
}

auto Trainer::save_best_sample(const std::string& filename) const -> Bool {
  return best_sample.save(filename);
}

auto Trainer::load_best_sample(const std::string& filename, ModalPolicy& out_policy) -> Bool {
  return out_policy.load(filename);
}

}  // namespace padv
