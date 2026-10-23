#pragma once

#include <random>
#include <string>
#include <utility>

#include <Eigen/Dense>

#include "padv/IPolicy.hpp"
#include "padv/MLP.hpp"
#include "padv/Types.hpp"

namespace padv {

class ModalPolicy final : public IPolicy {
 public:
  static constexpr I32 INPUT_DIM  = 28;
  static constexpr I32 LOCO_DIM   = 2;
  static constexpr I32 COMBAT_DIM = 3;

  MLP model;

  ModalPolicy();
  explicit ModalPolicy(MLP custom_model);

  auto initialize(std::mt19937& rng) -> void;
  [[nodiscard]] auto forward(const Eigen::VectorXf& in) const -> std::pair<Eigen::Vector2f, Eigen::Vector3f>;
  [[nodiscard]] auto evaluate(const SensorData& sensors) -> Command override;

  auto mutate(F32 rate, F32 strength, std::mt19937& rng) -> void;
  [[nodiscard]] static auto crossover(const ModalPolicy& p1, const ModalPolicy& p2, std::mt19937& rng) -> ModalPolicy;

  [[nodiscard]] auto save(const std::string& filepath) const -> Bool;
  [[nodiscard]] auto load(const std::string& filepath) -> Bool;

  [[nodiscard]] auto save_safetensors(const std::string& filepath) const -> Bool;
  [[nodiscard]] auto load_safetensors(const std::string& filepath) -> Bool;

  [[nodiscard]] auto save_json(const std::string& filepath) const -> Bool;
  [[nodiscard]] auto load_json(const std::string& filepath) -> Bool;

  [[nodiscard]] auto get_parameter_count() const -> Size;
};

}
