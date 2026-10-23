#pragma once

#include <cstddef>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Dense>

#include "padv/Types.hpp"

namespace padv {

enum class Activation {
  ReLU,
  GELU,
  SiLU,
  Tanh,
  Sigmoid,
  Linear
};

auto activation_to_string(Activation act) -> std::string;
auto string_to_activation(const std::string& str) -> Activation;

struct LayerConfig {
  int units{64};
  Activation activation{Activation::GELU};

  bool operator==(const LayerConfig& other) const {
    return units == other.units && activation == other.activation;
  }
};

struct Layer {
  int in_features{0};
  int out_features{0};
  Activation activation{Activation::GELU};
  Eigen::MatrixXf weights;
  Eigen::VectorXf bias;
};

class MLP {
 public:
  static constexpr int INPUT_DIM = 28;
  static constexpr int LOCO_DIM = 2;
  static constexpr int COMBAT_DIM = 3;

  MLP();
  explicit MLP(std::vector<LayerConfig> configs);

  auto set_architecture(const std::vector<LayerConfig>& configs) -> void;
  [[nodiscard]] auto get_architecture() const -> const std::vector<LayerConfig>&;
  [[nodiscard]] auto get_layers() const -> const std::vector<Layer>&;
  [[nodiscard]] auto get_layers_mut() -> std::vector<Layer>&;

  auto add_layer(int units, Activation act) -> void;
  auto remove_layer(size_t index) -> bool;
  auto update_layer(size_t index, int units, Activation act) -> bool;
  auto move_layer(size_t from_idx, size_t to_idx) -> bool;

  auto initialize(std::mt19937& rng) -> void;
  [[nodiscard]] auto forward(const Eigen::VectorXf& in) const -> std::pair<Eigen::Vector2f, Eigen::Vector3f>;

  auto mutate(float rate, float strength, std::mt19937& rng) -> void;
  [[nodiscard]] static auto crossover(const MLP& p1, const MLP& p2, std::mt19937& rng) -> MLP;

  [[nodiscard]] auto get_parameter_count() const -> size_t;

  [[nodiscard]] auto save(const std::string& filepath) const -> bool;
  [[nodiscard]] auto load(const std::string& filepath) -> bool;

  [[nodiscard]] auto save_safetensors(const std::string& filepath) const -> bool;
  [[nodiscard]] auto load_safetensors(const std::string& filepath) -> bool;

  [[nodiscard]] auto save_json(const std::string& filepath) const -> bool;
  [[nodiscard]] auto load_json(const std::string& filepath) -> bool;

 private:
  std::vector<LayerConfig> configs_;
  std::vector<Layer> layers_;
  Layer loco_head_;
  Layer combat_head_;

  auto rebuild_layers() -> void;
};

}
