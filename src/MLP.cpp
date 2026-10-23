#include "padv/MLP.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace padv {

namespace {

auto apply_activation(const Eigen::VectorXf& x, Activation act) -> Eigen::VectorXf {
  switch (act) {
    case Activation::ReLU:
      return x.cwiseMax(0.0f);
    case Activation::GELU:
      return x.unaryExpr([](float val) {
        return 0.5f * val * (1.0f + std::tanh(0.7978845608f * (val + 0.044715f * val * val * val)));
      });
    case Activation::SiLU:
      return x.unaryExpr([](float val) {
        float c = std::clamp(val, -20.0f, 20.0f);
        return c / (1.0f + std::exp(-c));
      });
    case Activation::Tanh:
      return x.unaryExpr([](float val) {
        return std::tanh(std::clamp(val, -20.0f, 20.0f));
      });
    case Activation::Sigmoid:
      return x.unaryExpr([](float val) {
        float c = std::clamp(val, -20.0f, 20.0f);
        return 1.0f / (1.0f + std::exp(-c));
      });
    case Activation::Linear:
    default:
      return x;
  }
}

auto init_matrix_he(Eigen::MatrixXf& mat, std::mt19937& rng) -> void {
  float scale = std::sqrt(2.0f / static_cast<float>(std::max(1, static_cast<int>(mat.cols()))));
  std::normal_distribution<float> dist(0.0f, scale);
  for (int r = 0; r < mat.rows(); ++r) {
    for (int c = 0; c < mat.cols(); ++c) {
      mat(r, c) = dist(rng);
    }
  }
}

auto init_matrix_xavier(Eigen::MatrixXf& mat, std::mt19937& rng) -> void {
  float scale = std::sqrt(2.0f / static_cast<float>(std::max(1, static_cast<int>(mat.rows() + mat.cols()))));
  std::normal_distribution<float> dist(0.0f, scale);
  for (int r = 0; r < mat.rows(); ++r) {
    for (int c = 0; c < mat.cols(); ++c) {
      mat(r, c) = dist(rng);
    }
  }
}

auto mutate_mat(Eigen::MatrixXf& mat, float rate, float strength, std::mt19937& rng) -> void {
  std::uniform_real_distribution<float> prob(0.0f, 1.0f);
  std::normal_distribution<float> shift(0.0f, strength);
  std::uniform_real_distribution<float> reset(-1.0f, 1.0f);

  for (int r = 0; r < mat.rows(); ++r) {
    for (int c = 0; c < mat.cols(); ++c) {
      float p = prob(rng);
      if (p < rate) {
        mat(r, c) += shift(rng);
      } else if (p < rate + 0.02f) {
        mat(r, c) = reset(rng);
      }
      mat(r, c) = std::clamp(mat(r, c), -5.0f, 5.0f);
    }
  }
}

auto mutate_vec(Eigen::VectorXf& vec, float rate, float strength, std::mt19937& rng) -> void {
  std::uniform_real_distribution<float> prob(0.0f, 1.0f);
  std::normal_distribution<float> shift(0.0f, strength);

  for (int i = 0; i < vec.size(); ++i) {
    if (prob(rng) < rate) {
      vec(i) += shift(rng);
      vec(i) = std::clamp(vec(i), -5.0f, 5.0f);
    }
  }
}

auto crossover_mat(Eigen::MatrixXf& child, const Eigen::MatrixXf& p1, const Eigen::MatrixXf& p2,
                   std::mt19937& rng) -> void {
  std::uniform_int_distribution<int> coin(0, 1);
  for (int r = 0; r < child.rows(); ++r) {
    for (int c = 0; c < child.cols(); ++c) {
      child(r, c) = coin(rng) ? p1(r, c) : p2(r, c);
    }
  }
}

auto crossover_vec(Eigen::VectorXf& child, const Eigen::VectorXf& p1, const Eigen::VectorXf& p2,
                   std::mt19937& rng) -> void {
  std::uniform_int_distribution<int> coin(0, 1);
  for (int i = 0; i < child.size(); ++i) {
    child(i) = coin(rng) ? p1(i) : p2(i);
  }
}

}

auto activation_to_string(Activation act) -> std::string {
  switch (act) {
    case Activation::ReLU:    return "ReLU";
    case Activation::GELU:    return "GELU";
    case Activation::SiLU:    return "SiLU";
    case Activation::Tanh:    return "Tanh";
    case Activation::Sigmoid: return "Sigmoid";
    case Activation::Linear:  return "Linear";
    default:                  return "GELU";
  }
}

auto string_to_activation(const std::string& str) -> Activation {
  if (str == "ReLU")    return Activation::ReLU;
  if (str == "GELU")    return Activation::GELU;
  if (str == "SiLU")    return Activation::SiLU;
  if (str == "Tanh")    return Activation::Tanh;
  if (str == "Sigmoid") return Activation::Sigmoid;
  if (str == "Linear")  return Activation::Linear;
  return Activation::GELU;
}

MLP::MLP() {
  configs_ = {
    {64, Activation::GELU},
    {64, Activation::GELU},
    {32, Activation::GELU}
  };
  rebuild_layers();
  std::mt19937 rng(1337);
  initialize(rng);
}

MLP::MLP(std::vector<LayerConfig> configs) : configs_(std::move(configs)) {
  if (configs_.empty()) {
    configs_.push_back({32, Activation::GELU});
  }
  rebuild_layers();
  std::mt19937 rng(1337);
  initialize(rng);
}

auto MLP::set_architecture(const std::vector<LayerConfig>& configs) -> void {
  configs_ = configs;
  if (configs_.empty()) {
    configs_.push_back({32, Activation::GELU});
  }
  rebuild_layers();
  std::mt19937 rng(1337);
  initialize(rng);
}

auto MLP::get_architecture() const -> const std::vector<LayerConfig>& {
  return configs_;
}

auto MLP::get_layers() const -> const std::vector<Layer>& {
  return layers_;
}

auto MLP::get_layers_mut() -> std::vector<Layer>& {
  return layers_;
}

auto MLP::add_layer(int units, Activation act) -> void {
  int safe_units = std::clamp(units, 4, 1024);
  configs_.push_back({safe_units, act});
  rebuild_layers();
  std::mt19937 rng(std::random_device{}());
  initialize(rng);
}

auto MLP::remove_layer(size_t index) -> bool {
  if (configs_.size() <= 1 || index >= configs_.size()) {
    return false;
  }
  configs_.erase(configs_.begin() + index);
  rebuild_layers();
  std::mt19937 rng(std::random_device{}());
  initialize(rng);
  return true;
}

auto MLP::update_layer(size_t index, int units, Activation act) -> bool {
  if (index >= configs_.size()) {
    return false;
  }
  int safe_units = std::clamp(units, 4, 1024);
  configs_[index] = {safe_units, act};
  rebuild_layers();
  std::mt19937 rng(std::random_device{}());
  initialize(rng);
  return true;
}

auto MLP::move_layer(size_t from_idx, size_t to_idx) -> bool {
  if (from_idx >= configs_.size() || to_idx >= configs_.size() || from_idx == to_idx) {
    return false;
  }
  std::swap(configs_[from_idx], configs_[to_idx]);
  rebuild_layers();
  std::mt19937 rng(std::random_device{}());
  initialize(rng);
  return true;
}

auto MLP::rebuild_layers() -> void {
  layers_.clear();
  layers_.reserve(configs_.size());

  int current_dim = INPUT_DIM;
  for (const auto& cfg : configs_) {
    Layer layer;
    layer.in_features = current_dim;
    layer.out_features = cfg.units;
    layer.activation = cfg.activation;
    layer.weights.resize(layer.out_features, layer.in_features);
    layer.bias.resize(layer.out_features);
    layers_.push_back(std::move(layer));
    current_dim = cfg.units;
  }

  loco_head_.in_features = current_dim;
  loco_head_.out_features = LOCO_DIM;
  loco_head_.activation = Activation::Tanh;
  loco_head_.weights.resize(LOCO_DIM, current_dim);
  loco_head_.bias.resize(LOCO_DIM);

  combat_head_.in_features = current_dim;
  combat_head_.out_features = COMBAT_DIM;
  combat_head_.activation = Activation::Sigmoid;
  combat_head_.weights.resize(COMBAT_DIM, current_dim);
  combat_head_.bias.resize(COMBAT_DIM);
}

auto MLP::initialize(std::mt19937& rng) -> void {
  for (auto& layer : layers_) {
    init_matrix_he(layer.weights, rng);
    layer.bias.setZero();
  }
  init_matrix_xavier(loco_head_.weights, rng);
  loco_head_.bias.setZero();

  init_matrix_xavier(combat_head_.weights, rng);
  combat_head_.bias.setZero();
}

auto MLP::forward(const Eigen::VectorXf& in) const -> std::pair<Eigen::Vector2f, Eigen::Vector3f> {
  Eigen::VectorXf cur = in;

  for (const auto& layer : layers_) {
    Eigen::VectorXf z = layer.weights * cur + layer.bias;
    cur = apply_activation(z, layer.activation);
  }

  Eigen::VectorXf loco_raw = loco_head_.weights * cur + loco_head_.bias;
  Eigen::Vector2f loco;
  loco.x() = std::tanh(loco_raw(0));
  loco.y() = std::tanh(loco_raw(1));

  Eigen::VectorXf combat_raw = combat_head_.weights * cur + combat_head_.bias;
  Eigen::Vector3f combat;
  for (int i = 0; i < COMBAT_DIM; ++i) {
    float c = std::clamp(combat_raw(i), -20.0f, 20.0f);
    combat(i) = 1.0f / (1.0f + std::exp(-c));
  }

  return {loco, combat};
}

auto MLP::mutate(float rate, float strength, std::mt19937& rng) -> void {
  for (auto& layer : layers_) {
    mutate_mat(layer.weights, rate, strength, rng);
    mutate_vec(layer.bias, rate, strength, rng);
  }
  mutate_mat(loco_head_.weights, rate, strength, rng);
  mutate_vec(loco_head_.bias, rate, strength, rng);

  mutate_mat(combat_head_.weights, rate, strength, rng);
  mutate_vec(combat_head_.bias, rate, strength, rng);
}

auto MLP::crossover(const MLP& p1, const MLP& p2, std::mt19937& rng) -> MLP {
  if (p1.configs_ == p2.configs_) {
    MLP child;
    child.configs_ = p1.configs_;
    child.rebuild_layers();

    for (size_t i = 0; i < child.layers_.size(); ++i) {
      crossover_mat(child.layers_[i].weights, p1.layers_[i].weights, p2.layers_[i].weights, rng);
      crossover_vec(child.layers_[i].bias, p1.layers_[i].bias, p2.layers_[i].bias, rng);
    }
    crossover_mat(child.loco_head_.weights, p1.loco_head_.weights, p2.loco_head_.weights, rng);
    crossover_vec(child.loco_head_.bias, p1.loco_head_.bias, p2.loco_head_.bias, rng);

    crossover_mat(child.combat_head_.weights, p1.combat_head_.weights, p2.combat_head_.weights, rng);
    crossover_vec(child.combat_head_.bias, p1.combat_head_.bias, p2.combat_head_.bias, rng);

    return child;
  }

  MLP child = p1;
  child.mutate(0.05f, 0.1f, rng);
  return child;
}

auto MLP::get_parameter_count() const -> size_t {
  size_t total = 0;
  for (const auto& layer : layers_) {
    total += static_cast<size_t>(layer.weights.size() + layer.bias.size());
  }
  total += static_cast<size_t>(loco_head_.weights.size() + loco_head_.bias.size());
  total += static_cast<size_t>(combat_head_.weights.size() + combat_head_.bias.size());
  return total;
}

auto MLP::save_safetensors(const std::string& filepath) const -> bool {
  struct TensorInfo {
    std::string name;
    std::vector<int> shape;
    const float* data;
    size_t num_elements;
  };

  std::vector<TensorInfo> tensors;
  for (size_t i = 0; i < layers_.size(); ++i) {
    tensors.push_back({
      "h_" + std::to_string(i) + "_w",
      {static_cast<int>(layers_[i].weights.rows()), static_cast<int>(layers_[i].weights.cols())},
      layers_[i].weights.data(),
      static_cast<size_t>(layers_[i].weights.size())
    });
    tensors.push_back({
      "h_" + std::to_string(i) + "_b",
      {static_cast<int>(layers_[i].bias.size())},
      layers_[i].bias.data(),
      static_cast<size_t>(layers_[i].bias.size())
    });
  }

  tensors.push_back({
    "loco_w",
    {static_cast<int>(loco_head_.weights.rows()), static_cast<int>(loco_head_.weights.cols())},
    loco_head_.weights.data(),
    static_cast<size_t>(loco_head_.weights.size())
  });
  tensors.push_back({
    "loco_b",
    {static_cast<int>(loco_head_.bias.size())},
    loco_head_.bias.data(),
    static_cast<size_t>(loco_head_.bias.size())
  });

  tensors.push_back({
    "combat_w",
    {static_cast<int>(combat_head_.weights.rows()), static_cast<int>(combat_head_.weights.cols())},
    combat_head_.weights.data(),
    static_cast<size_t>(combat_head_.weights.size())
  });
  tensors.push_back({
    "combat_b",
    {static_cast<int>(combat_head_.bias.size())},
    combat_head_.bias.data(),
    static_cast<size_t>(combat_head_.bias.size())
  });

  std::string meta_json = "{\"format\":\"safetensors\",\"model\":\"MLP\",\"num_hidden\":\"" +
                          std::to_string(configs_.size()) + "\",\"parameters\":\"" +
                          std::to_string(get_parameter_count()) + "\"";

  for (size_t i = 0; i < configs_.size(); ++i) {
    meta_json += ",\"layer_" + std::to_string(i) + "_units\":\"" + std::to_string(configs_[i].units) + "\"";
    meta_json += ",\"layer_" + std::to_string(i) + "_act\":\"" + activation_to_string(configs_[i].activation) + "\"";
  }
  meta_json += "}";

  std::string json_header = "{\"__metadata__\":" + meta_json;

  size_t current_offset = 0;
  for (const auto& t : tensors) {
    size_t byte_len = t.num_elements * sizeof(float);
    json_header += ",\"" + t.name + "\":{\"dtype\":\"F32\",\"shape\":[";
    for (size_t s = 0; s < t.shape.size(); ++s) {
      json_header += std::to_string(t.shape[s]) + (s + 1 < t.shape.size() ? "," : "");
    }
    json_header += "],\"data_offsets\":[" + std::to_string(current_offset) + "," +
                   std::to_string(current_offset + byte_len) + "]}";
    current_offset += byte_len;
  }
  json_header += "}";

  std::ofstream os(filepath, std::ios::binary);
  if (!os.is_open()) return false;

  uint64_t header_len = static_cast<uint64_t>(json_header.size());
  os.write(reinterpret_cast<const char*>(&header_len), sizeof(header_len));
  os.write(json_header.data(), static_cast<std::streamsize>(header_len));

  for (const auto& t : tensors) {
    os.write(reinterpret_cast<const char*>(t.data), static_cast<std::streamsize>(t.num_elements * sizeof(float)));
  }

  return os.good();
}

auto MLP::load_safetensors(const std::string& filepath) -> bool {
  std::ifstream is(filepath, std::ios::binary);
  if (!is.is_open()) return false;

  uint64_t header_len = 0;
  is.read(reinterpret_cast<char*>(&header_len), sizeof(header_len));
  if (header_len == 0 || header_len > 10 * 1024 * 1024) return false;

  std::string json_header(header_len, '\0');
  is.read(json_header.data(), static_cast<std::streamsize>(header_len));

  auto find_meta_val = [&](const std::string& key) -> std::string {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = json_header.find(needle);
    if (pos == std::string::npos) return "";
    size_t start = pos + needle.size();
    size_t end = json_header.find('"', start);
    if (end == std::string::npos) return "";
    return json_header.substr(start, end - start);
  };

  std::string num_hidden_str = find_meta_val("num_hidden");
  if (!num_hidden_str.empty()) {
    try {
      int num_hidden = std::stoi(num_hidden_str);
      std::vector<LayerConfig> new_configs;
      for (int i = 0; i < num_hidden; ++i) {
        std::string units_str = find_meta_val("layer_" + std::to_string(i) + "_units");
        std::string act_str = find_meta_val("layer_" + std::to_string(i) + "_act");
        int u = units_str.empty() ? 64 : std::stoi(units_str);
        Activation act = act_str.empty() ? Activation::GELU : string_to_activation(act_str);
        new_configs.push_back({u, act});
      }
      configs_ = std::move(new_configs);
      rebuild_layers();
    } catch (...) {
      return false;
    }
  }

  auto read_raw = [&](float* ptr, size_t count) -> bool {
    is.read(reinterpret_cast<char*>(ptr), static_cast<std::streamsize>(count * sizeof(float)));
    return is.good();
  };

  for (auto& layer : layers_) {
    if (!read_raw(layer.weights.data(), static_cast<size_t>(layer.weights.size()))) return false;
    if (!read_raw(layer.bias.data(), static_cast<size_t>(layer.bias.size()))) return false;
  }

  if (!read_raw(loco_head_.weights.data(), static_cast<size_t>(loco_head_.weights.size()))) return false;
  if (!read_raw(loco_head_.bias.data(), static_cast<size_t>(loco_head_.bias.size()))) return false;

  if (!read_raw(combat_head_.weights.data(), static_cast<size_t>(combat_head_.weights.size()))) return false;
  if (!read_raw(combat_head_.bias.data(), static_cast<size_t>(combat_head_.bias.size()))) return false;

  return true;
}

auto MLP::save_json(const std::string& filepath) const -> bool {
  std::ofstream os(filepath);
  if (!os.is_open()) return false;

  os << "{\n";
  os << "  \"model\": \"MLP\",\n";
  os << "  \"num_hidden\": " << configs_.size() << ",\n";
  os << "  \"parameters\": " << get_parameter_count() << ",\n";
  os << "  \"layers\": [\n";
  for (size_t i = 0; i < configs_.size(); ++i) {
    os << "    {\"units\": " << configs_[i].units << ", \"act\": \""
       << activation_to_string(configs_[i].activation) << "\"}"
       << (i + 1 < configs_.size() ? ",\n" : "\n");
  }
  os << "  ]\n";
  os << "}\n";
  return os.good();
}

auto MLP::load_json(const std::string& filepath) -> bool {
  return load_safetensors(filepath);
}

auto MLP::save(const std::string& filepath) const -> bool {
  if (filepath.ends_with(".json")) {
    return save_json(filepath);
  }
  return save_safetensors(filepath);
}

auto MLP::load(const std::string& filepath) -> bool {
  return load_safetensors(filepath);
}

}
