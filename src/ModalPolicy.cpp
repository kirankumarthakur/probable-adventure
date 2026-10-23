#include "padv/ModalPolicy.hpp"

#include <utility>

namespace padv {

ModalPolicy::ModalPolicy() = default;

ModalPolicy::ModalPolicy(MLP custom_model) : model(std::move(custom_model)) {}

auto ModalPolicy::initialize(std::mt19937& rng) -> void {
  model.initialize(rng);
}

auto ModalPolicy::forward(const Eigen::VectorXf& in) const -> std::pair<Eigen::Vector2f, Eigen::Vector3f> {
  return model.forward(in);
}

auto ModalPolicy::evaluate(const SensorData& sensors) -> Command {
  Eigen::VectorXf in(INPUT_DIM);
  in.setZero();

  in(0) = sensors.hp_ratio;
  in(1) = sensors.stamina_ratio;
  in(2) = sensors.cleave_cd;
  in(3) = sensors.shield_cd;
  in(4) = sensors.warcry_cd;
  in(5) = sensors.is_debuffed;
  in(6) = sensors.wall_north;
  in(7) = sensors.wall_south;
  in(8) = sensors.wall_east;
  in(9) = sensors.wall_west;

  int idx = 10;
  for (const auto& ray : sensors.vision_rays) {
    if (idx >= 21) break;
    in(idx++) = ray.hit ? (1.0f - (ray.distance / 100.0f)) : 0.0f;
  }
  idx = 21;
  for (const auto& ray : sensors.sound_rays) {
    if (idx >= 28) break;
    in(idx++) = ray.hit ? 1.0f : 0.0f;
  }

  auto [loco, combat] = forward(in);

  Command cmd;
  cmd.move = glm::vec2(loco.x(), loco.y());

  if (combat(0) > 0.5f && sensors.cleave_cd > 0.9f) {
    cmd.requested_action = Action::Cleave;
  } else if (combat(1) > 0.5f && sensors.shield_cd > 0.9f) {
    cmd.requested_action = Action::Shield;
  } else if (combat(2) > 0.5f && sensors.warcry_cd > 0.9f) {
    cmd.requested_action = Action::Warcry;
  } else {
    cmd.requested_action = Action::None;
  }

  return cmd;
}

auto ModalPolicy::mutate(F32 rate, F32 strength, std::mt19937& rng) -> void {
  model.mutate(rate, strength, rng);
}

auto ModalPolicy::crossover(const ModalPolicy& p1, const ModalPolicy& p2, std::mt19937& rng) -> ModalPolicy {
  return ModalPolicy(MLP::crossover(p1.model, p2.model, rng));
}

auto ModalPolicy::save(const std::string& filepath) const -> Bool {
  return model.save(filepath);
}

auto ModalPolicy::load(const std::string& filepath) -> Bool {
  return model.load(filepath);
}

auto ModalPolicy::save_safetensors(const std::string& filepath) const -> Bool {
  return model.save_safetensors(filepath);
}

auto ModalPolicy::load_safetensors(const std::string& filepath) -> Bool {
  return model.load_safetensors(filepath);
}

auto ModalPolicy::save_json(const std::string& filepath) const -> Bool {
  return model.save_json(filepath);
}

auto ModalPolicy::load_json(const std::string& filepath) -> Bool {
  return model.load_json(filepath);
}

auto ModalPolicy::get_parameter_count() const -> Size {
  return static_cast<Size>(model.get_parameter_count());
}

}
