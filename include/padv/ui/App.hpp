#pragma once

#include <memory>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>
#include <entt/entt.hpp>

#include "padv/Arena.hpp"
#include "padv/CheckpointManager.hpp"
#include "padv/Config.hpp"
#include "padv/MLP.hpp"
#include "padv/ModalPolicy.hpp"
#include "padv/RenderSystem.hpp"
#include "padv/Trainer.hpp"
#include "padv/ui/ClayWidgets.hpp"

namespace padv::ui {

enum class ViewMode {
  Train,
  Analyze,
  Configure
};

class App {
 public:
  static auto instance() -> App&;

  auto init(const AppConfig& cfg) -> bool;
  auto run() -> int;

  auto set_view(ViewMode mode) -> void {
    current_view_ = mode;
    if (mode == ViewMode::Analyze) {
      refresh_checkpoints();
    }
  }
  [[nodiscard]] auto get_view() const -> ViewMode { return current_view_; }

  auto reset_live_simulation(const ModalPolicy& policy) -> void;
  auto step_live_simulation(float dt) -> void;
  auto refresh_checkpoints() -> void;
  auto load_selected_checkpoint() -> void;

  AppConfig config;
  Trainer trainer;

  entt::registry live_registry;
  Arena live_arena;
  ModalPolicy live_policy;

  bool is_sim_playing{false};
  bool step_one_tick{false};
  int sim_speed{1};
  bool camera_follow{true};
  RenderDebugFlags debug_flags;

  int selected_checkpoint_idx{-1};
  bool is_checkpoint_loaded_{false};
  std::vector<CheckpointMeta> checkpoints;

  std::vector<LayerConfig> working_configs;
  int selected_layer_idx{0};

 private:
  App() = default;
  ~App() = default;

  auto process_events() -> bool;
  auto update(float dt) -> void;
  auto render() -> void;

  auto build_top_nav(const InputState& input) -> void;
  auto build_train_view(const InputState& input) -> void;
  auto build_inspect_view(const InputState& input) -> void;
  auto build_model_view(const InputState& input) -> void;

  std::unique_ptr<sf::RenderWindow> window_;
  ViewMode current_view_{ViewMode::Train};
  InputState input_state_;

  float tick_accumulator_{0.0f};
  std::string status_notification_;
  float notification_timer_{0.0f};
  bool was_training_{false};

  Action last_action_hint_{Action::None};
  float action_hint_timer_{0.0f};
  Action manual_action_override_{Action::None};
};

using Win32App = App;

}
