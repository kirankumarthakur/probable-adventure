#include "padv/ui/App.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include <spdlog/spdlog.h>

#include "padv/TextureManager.hpp"
#include "padv/ui/ClayRenderer.hpp"
#include "padv/ui/ClayWidgets.hpp"

namespace padv::ui {

auto App::instance() -> App& {
  static App s_instance;
  return s_instance;
}

auto App::init(const AppConfig& cfg) -> bool {
  config = cfg;

  trainer.population_size   = config.population_size;
  trainer.generations       = config.generations;
  trainer.mutation_rate     = config.mutation_rate;
  trainer.mutation_strength = config.mutation_strength;
  trainer.tournament_size   = config.tournament_size;
  trainer.elitism_count     = config.elitism_count;
  trainer.max_eval_ticks    = config.max_eval_ticks;
  trainer.checkpoint_mgr.set_policy(config.checkpoint_policy, config.top_k_count, config.periodic_interval);

  TextureManager::instance().load_all_assets();

  debug_flags.show_sprites  = true;
  debug_flags.show_hitboxes = false;
  debug_flags.show_dir_vecs = false;
  debug_flags.show_raycasts = false;

  window_ = std::make_unique<sf::RenderWindow>(
      sf::VideoMode({1600, 1050}),
      "Portable Adventure",
      sf::Style::Default
  );
  window_->setFramerateLimit(60);

  if (!ClayRenderer::instance().init(*window_)) {
    return false;
  }

  working_configs = {
    {64, Activation::GELU},
    {64, Activation::GELU},
    {32, Activation::GELU}
  };
  selected_layer_idx = 0;

  refresh_checkpoints();
  selected_checkpoint_idx = -1;
  is_checkpoint_loaded_ = false;
  is_sim_playing = false;
  live_registry.clear();
  live_arena = Arena{};

  return true;
}

auto App::reset_live_simulation(const ModalPolicy& policy) -> void {
  live_registry.clear();
  live_arena  = Arena{};
  live_policy = policy;
  build_adventurer(live_registry, live_policy);
}

auto App::step_live_simulation(float dt) -> void {
  if (!is_checkpoint_loaded_) {
    return;
  }

  auto run_tick = [this]() {
    Action act = manual_action_override_;
    manual_action_override_ = Action::None;
    step_simulation(live_registry, live_arena, act);
    auto adv_view = live_registry.view<Adventurer, ActionState>(entt::exclude<Dead>);
    for (auto e : adv_view) {
      const auto& a = adv_view.get<ActionState>(e);
      if (a.action != Action::None) {
        last_action_hint_ = a.action;
        action_hint_timer_ = 0.35f;
      }
    }
  };

  if (step_one_tick) {
    run_tick();
    step_one_tick = false;
    return;
  }

  if (!is_sim_playing || live_arena.game_over) {
    return;
  }

  if (sim_speed == 99) {
    for (int i = 0; i < 20; ++i) {
      if (live_arena.game_over) break;
      run_tick();
    }
  } else {
    float tick_interval = (1.0f / 60.0f) / static_cast<float>(sim_speed);
    tick_accumulator_ += dt;
    int ticks_run = 0;
    while (tick_accumulator_ >= tick_interval) {
      run_tick();
      tick_accumulator_ -= tick_interval;
      ++ticks_run;
      if (ticks_run >= 30 || live_arena.game_over) {
        tick_accumulator_ = 0.0f;
        break;
      }
    }
  }
}

auto App::refresh_checkpoints() -> void {
  checkpoints = trainer.checkpoint_mgr.list_checkpoints();
  if (selected_checkpoint_idx >= static_cast<int>(checkpoints.size())) {
    selected_checkpoint_idx = -1;
  }
}

auto App::load_selected_checkpoint() -> void {
  refresh_checkpoints();
  if (selected_checkpoint_idx < 0 || selected_checkpoint_idx >= static_cast<int>(checkpoints.size())) {
    status_notification_ = "No valid checkpoint selected";
    notification_timer_ = 3.0f;
    return;
  }
  const auto& meta = checkpoints[selected_checkpoint_idx];
  ModalPolicy loaded;
  if (trainer.checkpoint_mgr.load_checkpoint(meta.filepath, loaded)) {
    reset_live_simulation(loaded);
    is_checkpoint_loaded_ = true;
    is_sim_playing = false;
    working_configs = loaded.model.get_architecture();
    selected_layer_idx = 0;
    status_notification_ = std::format("Loaded checkpoint '{}'", meta.filename);
    notification_timer_ = 3.0f;
  } else {
    status_notification_ = "Checkpoint file no longer on disk";
    notification_timer_ = 3.0f;
  }
}

auto App::process_events() -> bool {
  input_state_.mouse_clicked = false;
  input_state_.scroll_delta  = 0.0f;

  while (const auto event = window_->pollEvent()) {
    if (event->is<sf::Event::Closed>()) {
      window_->close();
      return false;
    }

    if (const auto* resized = event->getIf<sf::Event::Resized>()) {
      window_->setView(sf::View(sf::FloatRect({0.0f, 0.0f},
                                              {static_cast<float>(resized->size.x),
                                               static_cast<float>(resized->size.y)})));
    }

    if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
      if (mb->button == sf::Mouse::Button::Left) {
        input_state_.mouse_down    = true;
        input_state_.mouse_clicked = true;
      }
    }

    if (const auto* mb = event->getIf<sf::Event::MouseButtonReleased>()) {
      if (mb->button == sf::Mouse::Button::Left) {
        input_state_.mouse_down = false;
      }
    }

    if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
      input_state_.scroll_delta += wheel->delta;
    }

    if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
      if (kp->code == sf::Keyboard::Key::Num1) set_view(ViewMode::Train);
      if (kp->code == sf::Keyboard::Key::Num2) set_view(ViewMode::Analyze);
      if (kp->code == sf::Keyboard::Key::Num3) set_view(ViewMode::Configure);
      if (kp->code == sf::Keyboard::Key::Space) {
        if (!is_checkpoint_loaded_) {
          status_notification_ = "Please pick a model checkpoint first";
          notification_timer_ = 2.5f;
        } else {
          is_sim_playing = !is_sim_playing;
        }
      }

      if (current_view_ == ViewMode::Analyze) {
        if (kp->code == sf::Keyboard::Key::Q) {
          manual_action_override_ = Action::Cleave;
          last_action_hint_ = Action::Cleave;
          action_hint_timer_ = 0.35f;
        }
        if (kp->code == sf::Keyboard::Key::W) {
          manual_action_override_ = Action::Shield;
          last_action_hint_ = Action::Shield;
          action_hint_timer_ = 0.35f;
        }
        if (kp->code == sf::Keyboard::Key::E) {
          manual_action_override_ = Action::Warcry;
          last_action_hint_ = Action::Warcry;
          action_hint_timer_ = 0.35f;
        }
      }
    }
  }

  auto mpos = sf::Mouse::getPosition(*window_);
  input_state_.mouse_pos = sf::Vector2f(static_cast<float>(mpos.x), static_cast<float>(mpos.y));
  return true;
}

auto App::update(float dt) -> void {
  if (notification_timer_ > 0.0f) {
    notification_timer_ -= dt;
    if (notification_timer_ <= 0.0f) {
      status_notification_.clear();
    }
  }

  if (action_hint_timer_ > 0.0f) {
    action_hint_timer_ -= dt;
    if (action_hint_timer_ <= 0.0f) {
      last_action_hint_ = Action::None;
    }
  }

  bool is_training = trainer.is_training.load();
  if (was_training_ && !is_training) {
    refresh_checkpoints();
  }
  was_training_ = is_training;

  if (current_view_ == ViewMode::Analyze) {
    step_live_simulation(dt);
  }
}

auto App::build_top_nav(const InputState& input) -> void {
  CLAY(CLAY_ID("TopNav"), {
    .layout = {
      .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(56.0f) },
      .padding = { 16, 16, 10, 10 },
      .childGap = 12,
      .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
      .layoutDirection = CLAY_LEFT_TO_RIGHT
    },
    .backgroundColor = { 26, 29, 36, 255 },
    .border = { .color = { 44, 49, 60, 255 }, .width = { 0, 0, 0, 1, 0 } }
  }) {
    CLAY_AUTO_ID({
      .layout = { .sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() }, .padding = { 0, 16, 0, 0 } }
    }) {
      CLAY_TEXT(intern_str("PORTABLE ADVENTURE"), CLAY_TEXT_CONFIG({
        .textColor = { 255, 255, 255, 255 },
        .fontSize = 17,
        .wrapMode = CLAY_TEXT_WRAP_NONE
      }));
    }

    if (button(CLAY_ID("NavTrain"), "Train", current_view_ == ViewMode::Train, input, 82.0f)) {
      set_view(ViewMode::Train);
    }
    if (button(CLAY_ID("NavAnalyze"), "Analyze", current_view_ == ViewMode::Analyze, input, 96.0f)) {
      set_view(ViewMode::Analyze);
    }
    if (button(CLAY_ID("NavConfigure"), "Configure", current_view_ == ViewMode::Configure, input, 108.0f)) {
      set_view(ViewMode::Configure);
    }

    if (current_view_ == ViewMode::Analyze) {
      CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT() } } }) {}

      if (button(CLAY_ID("SimPlay"), is_sim_playing ? "Pause" : "Play", is_sim_playing, input, 68.0f)) {
        if (!is_checkpoint_loaded_) {
          status_notification_ = "Please pick a model checkpoint first";
          notification_timer_ = 2.5f;
        } else {
          is_sim_playing = !is_sim_playing;
        }
      }
      if (button(CLAY_ID("SimStep"), "Step", false, input, 58.0f)) {
        if (!is_checkpoint_loaded_) {
          status_notification_ = "Please pick a model checkpoint first";
          notification_timer_ = 2.5f;
        } else {
          step_one_tick = true;
        }
      }
      if (button(CLAY_ID("SimRestart"), "Restart", false, input, 74.0f)) {
        if (is_checkpoint_loaded_) {
          reset_live_simulation(live_policy);
          is_sim_playing = false;
        }
      }
      if (button(CLAY_ID("SimSpd1"), "1x", sim_speed == 1, input, 38.0f)) sim_speed = 1;
      if (button(CLAY_ID("SimSpd2"), "2x", sim_speed == 2, input, 38.0f)) sim_speed = 2;
      if (button(CLAY_ID("SimSpd5"), "5x", sim_speed == 5, input, 38.0f)) sim_speed = 5;
      if (button(CLAY_ID("SimSpdMax"), "Max", sim_speed == 99, input, 48.0f)) sim_speed = 99;
      if (button(CLAY_ID("SimFollow"), camera_follow ? "Cam: Follow" : "Cam: Fixed", camera_follow, input, 110.0f)) {
        camera_follow = !camera_follow;
      }
    }
  }
}

auto App::build_train_view(const InputState& input) -> void {
  CLAY(CLAY_ID("TrainViewRoot"), {
    .layout = {
      .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
      .childGap = 16,
      .layoutDirection = CLAY_LEFT_TO_RIGHT
    }
  }) {
    CLAY(CLAY_ID("TrainSidebar"), {
      .layout = {
        .sizing = { CLAY_SIZING_FIXED(310.0f), CLAY_SIZING_GROW() },
        .childGap = 16,
        .layoutDirection = CLAY_TOP_TO_BOTTOM
      }
    }) {
      CLAY(CLAY_ID("CardRunner"), {
        .layout = {
          .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT() },
          .padding = { 16, 16, 16, 16 },
          .childGap = 12,
          .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = { 26, 29, 36, 255 },
        .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
        .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
      }) {
        card_header("Simulation Runner");

        bool is_training = trainer.is_training.load();
        if (button(CLAY_ID("BtnTrainToggle"), is_training ? "Stop Simulation" : "Start Simulation", is_training, input, -1.0f, 36.0f)) {
          if (is_training) {
            trainer.stop_training();
          } else {
            trainer.population_size   = config.population_size;
            trainer.generations       = config.generations;
            trainer.mutation_rate     = config.mutation_rate;
            trainer.mutation_strength = config.mutation_strength;
            trainer.max_eval_ticks    = config.max_eval_ticks;
            trainer.initialize(config.population_size, config.generations);
            trainer.start_background_training();
          }
        }

        stat_row("Best Score", std::format("{:.0f}", trainer.current_best_fitness.load()));
        stat_row("Average Score", std::format("{:.0f}", trainer.current_avg_fitness.load()), {140, 180, 255, 255});
      }

      CLAY(CLAY_ID("CardParams"), {
        .layout = {
          .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT() },
          .padding = { 16, 16, 16, 16 },
          .childGap = 10,
          .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = { 26, 29, 36, 255 },
        .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
        .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
      }) {
        card_header("Hyperparameters");

        auto make_stepper = [&](Clay_ElementId dec_id, Clay_ElementId inc_id, std::string_view label, std::string_view val, auto on_dec, auto on_inc) {
          CLAY_AUTO_ID({
            .layout = {
              .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(28.0f) },
              .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
              .layoutDirection = CLAY_LEFT_TO_RIGHT
            }
          }) {
            CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT() } } }) {
              CLAY_TEXT(intern_str(label), CLAY_TEXT_CONFIG({ .textColor = { 160, 165, 175, 255 }, .fontSize = 13, .wrapMode = CLAY_TEXT_WRAP_NONE }));
            }
            if (button(dec_id, "-", false, input, 28.0f, 26.0f)) on_dec();
            CLAY_AUTO_ID({
              .layout = { .sizing = { CLAY_SIZING_FIXED(60.0f), CLAY_SIZING_FIT() },
                          .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER } }
            }) {
              CLAY_TEXT(intern_str(val), CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontSize = 13, .wrapMode = CLAY_TEXT_WRAP_NONE }));
            }
            if (button(inc_id, "+", false, input, 28.0f, 26.0f)) on_inc();
          }
        };

        make_stepper(CLAY_ID("DecPop"), CLAY_ID("IncPop"), "Population", std::to_string(config.population_size),
                     [&]() { config.population_size = std::max(5, config.population_size - 5); config.save_to_file("config.json"); },
                     [&]() { config.population_size += 5; config.save_to_file("config.json"); });

        make_stepper(CLAY_ID("DecGen"), CLAY_ID("IncGen"), "Generations", std::to_string(config.generations),
                     [&]() { config.generations = std::max(5, config.generations - 5); config.save_to_file("config.json"); },
                     [&]() { config.generations += 5; config.save_to_file("config.json"); });

        make_stepper(CLAY_ID("DecTicks"), CLAY_ID("IncTicks"), "Max Ticks", std::to_string(config.max_eval_ticks),
                     [&]() { config.max_eval_ticks = std::max<int64_t>(250, config.max_eval_ticks - 250); config.save_to_file("config.json"); },
                     [&]() { config.max_eval_ticks = std::min<int64_t>(20000, config.max_eval_ticks + 250); config.save_to_file("config.json"); });

        make_stepper(CLAY_ID("DecMutR"), CLAY_ID("IncMutR"), "Mutation Rate", std::format("{:.2f}", config.mutation_rate),
                     [&]() { config.mutation_rate = std::max(0.01f, config.mutation_rate - 0.02f); config.save_to_file("config.json"); },
                     [&]() { config.mutation_rate = std::min(1.0f, config.mutation_rate + 0.02f); config.save_to_file("config.json"); });

        make_stepper(CLAY_ID("DecMutS"), CLAY_ID("IncMutS"), "Mutation Strength", std::format("{:.2f}", config.mutation_strength),
                     [&]() { config.mutation_strength = std::max(0.05f, config.mutation_strength - 0.05f); config.save_to_file("config.json"); },
                     [&]() { config.mutation_strength = std::min(2.0f, config.mutation_strength + 0.05f); config.save_to_file("config.json"); });
      }
    }

    CLAY(CLAY_ID("TrainChartsPanel"), {
      .layout = {
        .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
        .childGap = 10,
        .layoutDirection = CLAY_TOP_TO_BOTTOM
      }
    }) {
      CLAY(CLAY_ID("ChartsLegendBar"), {
        .layout = {
          .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(28.0f) },
          .padding = { 14, 14, 0, 0 },
          .childGap = 28,
          .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
          .layoutDirection = CLAY_LEFT_TO_RIGHT
        },
        .backgroundColor = { 26, 29, 36, 255 },
        .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
        .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
      }) {
        CLAY_AUTO_ID({
          .layout = { .sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() }, .childGap = 8, .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER }, .layoutDirection = CLAY_LEFT_TO_RIGHT }
        }) {
          CLAY_AUTO_ID({
            .layout = { .sizing = { CLAY_SIZING_FIXED(22.0f), CLAY_SIZING_FIXED(3.0f) } },
            .backgroundColor = { 0, 200, 80, 255 },
            .cornerRadius = CLAY_CORNER_RADIUS(2.0f)
          }) {}
          CLAY_TEXT(intern_str("Best"), CLAY_TEXT_CONFIG({ .textColor = { 210, 215, 225, 255 }, .fontSize = 13, .wrapMode = CLAY_TEXT_WRAP_NONE }));
        }

        CLAY_AUTO_ID({
          .layout = { .sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() }, .childGap = 8, .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER }, .layoutDirection = CLAY_LEFT_TO_RIGHT }
        }) {
          CLAY_AUTO_ID({
            .layout = { .sizing = { CLAY_SIZING_FIXED(22.0f), CLAY_SIZING_FIXED(3.0f) } },
            .backgroundColor = { 50, 150, 255, 255 },
            .cornerRadius = CLAY_CORNER_RADIUS(2.0f)
          }) {}
          CLAY_TEXT(intern_str("Mean"), CLAY_TEXT_CONFIG({ .textColor = { 210, 215, 225, 255 }, .fontSize = 13, .wrapMode = CLAY_TEXT_WRAP_NONE }));
        }

        CLAY_AUTO_ID({
          .layout = { .sizing = { CLAY_SIZING_FIT(), CLAY_SIZING_FIT() }, .childGap = 8, .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER }, .layoutDirection = CLAY_LEFT_TO_RIGHT }
        }) {
          CLAY_AUTO_ID({
            .layout = { .sizing = { CLAY_SIZING_FIXED(22.0f), CLAY_SIZING_FIXED(3.0f) } },
            .backgroundColor = { 245, 180, 50, 255 },
            .cornerRadius = CLAY_CORNER_RADIUS(2.0f)
          }) {}
          CLAY_TEXT(intern_str("Std Dev"), CLAY_TEXT_CONFIG({ .textColor = { 210, 215, 225, 255 }, .fontSize = 13, .wrapMode = CLAY_TEXT_WRAP_NONE }));
        }
      }

      CLAY(CLAY_ID("ChartRow1"), {
        .layout = {
          .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
          .childGap = 10,
          .layoutDirection = CLAY_LEFT_TO_RIGHT
        }
      }) {
        CLAY(CLAY_ID("CardChartFitness"), {
          .layout = {
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
            .padding = { 12, 12, 8, 8 },
            .childGap = 4,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
          },
          .backgroundColor = { 26, 29, 36, 255 },
          .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
          .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
        }) {
          card_header("Fitness Score");
          CLAY(CLAY_ID("ChartFitnessBox"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() } }
          }) {}
        }

        CLAY(CLAY_ID("CardChartError"), {
          .layout = {
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
            .padding = { 12, 12, 8, 8 },
            .childGap = 4,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
          },
          .backgroundColor = { 26, 29, 36, 255 },
          .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
          .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
        }) {
          card_header("Optimization Error");
          CLAY(CLAY_ID("ChartErrorBox"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() } }
          }) {}
        }
      }

      CLAY(CLAY_ID("ChartRow2"), {
        .layout = {
          .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
          .childGap = 10,
          .layoutDirection = CLAY_LEFT_TO_RIGHT
        }
      }) {
        CLAY(CLAY_ID("CardChartSurvival"), {
          .layout = {
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
            .padding = { 12, 12, 8, 8 },
            .childGap = 4,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
          },
          .backgroundColor = { 26, 29, 36, 255 },
          .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
          .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
        }) {
          card_header("Survival Duration");
          CLAY(CLAY_ID("ChartSurvivalBox"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() } }
          }) {}
        }

        CLAY(CLAY_ID("CardChartKills"), {
          .layout = {
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
            .padding = { 12, 12, 8, 8 },
            .childGap = 4,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
          },
          .backgroundColor = { 26, 29, 36, 255 },
          .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
          .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
        }) {
          card_header("Slimes Defeated");
          CLAY(CLAY_ID("ChartKillsBox"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() } }
          }) {}
        }
      }

      CLAY(CLAY_ID("ChartRow3"), {
        .layout = {
          .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
          .childGap = 10,
          .layoutDirection = CLAY_LEFT_TO_RIGHT
        }
      }) {
        CLAY(CLAY_ID("CardChartDamage"), {
          .layout = {
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
            .padding = { 12, 12, 8, 8 },
            .childGap = 4,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
          },
          .backgroundColor = { 26, 29, 36, 255 },
          .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
          .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
        }) {
          card_header("Damage Output");
          CLAY(CLAY_ID("ChartDamageBox"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() } }
          }) {}
        }

        CLAY(CLAY_ID("CardChartMlp"), {
          .layout = {
            .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
            .padding = { 12, 12, 8, 8 },
            .childGap = 4,
            .layoutDirection = CLAY_TOP_TO_BOTTOM
          },
          .backgroundColor = { 26, 29, 36, 255 },
          .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
          .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
        }) {
          card_header("Weight Norm");
          CLAY(CLAY_ID("ChartMlpBox"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() } }
          }) {}
        }
      }
    }
  }
}

auto App::build_inspect_view(const InputState& input) -> void {
  CLAY(CLAY_ID("InspectViewRoot"), {
    .layout = {
      .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
      .childGap = 16,
      .layoutDirection = CLAY_LEFT_TO_RIGHT
    }
  }) {
    CLAY(CLAY_ID("InspectSidebar"), {
      .layout = {
        .sizing = { CLAY_SIZING_FIXED(300.0f), CLAY_SIZING_GROW() },
        .childGap = 14,
        .layoutDirection = CLAY_TOP_TO_BOTTOM
      }
    }) {
      CLAY(CLAY_ID("CardCheckpoints"), {
        .layout = {
          .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(270.0f) },
          .padding = { 14, 14, 12, 12 },
          .childGap = 6,
          .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = { 26, 29, 36, 255 },
        .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
        .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
      }) {
        CLAY_AUTO_ID({
          .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT() },
                      .padding = { 0, 0, 0, 6 },
                      .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                      .layoutDirection = CLAY_LEFT_TO_RIGHT }
        }) {
          CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT() } } }) {
            CLAY_TEXT(intern_str("Saved Checkpoints"), CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontSize = 17, .wrapMode = CLAY_TEXT_WRAP_NONE }));
          }
          if (button(CLAY_ID("BtnRefreshCp"), "Refresh", false, input, 70.0f, 26.0f)) {
            refresh_checkpoints();
          }
        }

        if (checkpoints.empty()) {
          stat_row("Status", "No checkpoints yet", {160, 165, 175, 255});
        } else {
          for (size_t i = 0; i < checkpoints.size() && i < 6; ++i) {
            const auto& cp = checkpoints[i];
            std::string title = cp.generation >= 0 ? std::format("Gen {:02d} (Score: {:.0f})", cp.generation, cp.fitness)
                                                   : cp.filename;
            if (button(CLAY_IDI("CpItem", static_cast<uint32_t>(i)), title, selected_checkpoint_idx == static_cast<int>(i), input, -1.0f, 30.0f)) {
              if (selected_checkpoint_idx != static_cast<int>(i)) {
                selected_checkpoint_idx = static_cast<int>(i);
                load_selected_checkpoint();
              }
            }
          }
        }
      }

      CLAY(CLAY_ID("CardCpDetails"), {
        .layout = {
          .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT() },
          .padding = { 14, 14, 14, 14 },
          .childGap = 8,
          .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = { 26, 29, 36, 255 },
        .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
        .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
      }) {
        card_header("Selected Model Info");
        if (selected_checkpoint_idx >= 0 && selected_checkpoint_idx < static_cast<int>(checkpoints.size())) {
          const auto& sel = checkpoints[selected_checkpoint_idx];
          stat_row("Generation", sel.generation >= 0 ? std::to_string(sel.generation) : "N/A");
          stat_row("Benchmark Score", std::format("{:.1f}", sel.fitness), {100, 220, 140, 255});
          stat_row("File Size", std::format("{:.1f} KB", static_cast<float>(sel.file_size) / 1024.0f));
          stat_row("Parameters", sel.parameter_count > 0 ? std::to_string(sel.parameter_count) : "Dynamic");
          stat_row("Weight Format", "Safetensors");

          if (button(CLAY_ID("BtnLoadCp"), "Simulate with Weights", false, input, -1.0f, 32.0f)) {
            load_selected_checkpoint();
          }
        } else {
          stat_row("Generation", "-");
          stat_row("Benchmark Score", "-");
          stat_row("File Size", "-");
          stat_row("Parameters", "-");
          stat_row("Weight Format", "Safetensors");
        }
      }
    }

    CLAY(CLAY_ID("SimViewport"), {
      .layout = {
        .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() }
      },
      .backgroundColor = { 18, 20, 24, 255 },
      .cornerRadius = CLAY_CORNER_RADIUS(8.0f),
      .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
    }) {}
  }
}

auto App::build_model_view(const InputState& input) -> void {
  CLAY(CLAY_ID("ModelViewRoot"), {
    .layout = {
      .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
      .childGap = 16,
      .layoutDirection = CLAY_LEFT_TO_RIGHT
    }
  }) {
    CLAY(CLAY_ID("CardLayersList"), {
      .layout = {
        .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
        .padding = { 16, 16, 16, 16 },
        .childGap = 8,
        .layoutDirection = CLAY_TOP_TO_BOTTOM
      },
      .backgroundColor = { 26, 29, 36, 255 },
      .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
      .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
    }) {
      card_header("Layer Architecture");

      stat_row("[Input Layer]", "28 Features (Raycasts & Vitals)", {120, 200, 255, 255});

      for (size_t i = 0; i < working_configs.size(); ++i) {
        const auto& cfg = working_configs[i];
        std::string label = std::format("[Layer {}]    {} Units    |    Act: {}", i + 1, cfg.units, activation_to_string(cfg.activation));
        if (button(CLAY_IDI("LayerItem", static_cast<uint32_t>(i)), label, selected_layer_idx == static_cast<int>(i), input, -1.0f, 34.0f)) {
          selected_layer_idx = static_cast<int>(i);
        }
      }

      stat_row("[Movement Outputs]", "2 Outputs (Move X, Y) | Tanh", {255, 180, 80, 255});
      stat_row("[Combat Action Outputs]", "3 Outputs (Cleave, Shield, Warcry) | Sigmoid", {255, 120, 120, 255});
    }

    CLAY(CLAY_ID("ModelRightCol"), {
      .layout = {
        .sizing = { CLAY_SIZING_FIXED(380.0f), CLAY_SIZING_GROW() },
        .childGap = 16,
        .layoutDirection = CLAY_TOP_TO_BOTTOM
      }
    }) {
      CLAY(CLAY_ID("CardLayerConfig"), {
        .layout = {
          .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT() },
          .padding = { 16, 16, 16, 16 },
          .childGap = 12,
          .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = { 26, 29, 36, 255 },
        .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
        .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
      }) {
        card_header("Layer Configuration");

        if (selected_layer_idx >= 0 && selected_layer_idx < static_cast<int>(working_configs.size())) {
          auto& cur = working_configs[selected_layer_idx];
          stat_row("Selected Layer", std::format("Hidden Layer {}", selected_layer_idx + 1));

          CLAY_AUTO_ID({
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(30.0f) },
                        .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT }
          }) {
            CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT() } } }) {
              CLAY_TEXT(intern_str("Units"), CLAY_TEXT_CONFIG({ .textColor = { 160, 165, 175, 255 }, .fontSize = 13, .wrapMode = CLAY_TEXT_WRAP_NONE }));
            }
            if (button(CLAY_ID("DecU16"), "-16", false, input, 38.0f, 26.0f)) cur.units = std::max(4, cur.units - 16);
            if (button(CLAY_ID("DecU1"), "-1", false, input, 28.0f, 26.0f)) cur.units = std::max(4, cur.units - 1);
            CLAY_AUTO_ID({
              .layout = { .sizing = { CLAY_SIZING_FIXED(56.0f), CLAY_SIZING_FIT() },
                          .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER } }
            }) {
              CLAY_TEXT(intern_str(std::to_string(cur.units)), CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 }, .fontSize = 14, .wrapMode = CLAY_TEXT_WRAP_NONE }));
            }
            if (button(CLAY_ID("IncU1"), "+1", false, input, 28.0f, 26.0f)) cur.units = std::min(512, cur.units + 1);
            if (button(CLAY_ID("IncU16"), "+16", false, input, 38.0f, 26.0f)) cur.units = std::min(512, cur.units + 16);
          }

          CLAY_AUTO_ID({
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(30.0f) },
                        .childGap = 8,
                        .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT }
          }) {
            CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_FIXED(76.0f), CLAY_SIZING_FIT() } } }) {
              CLAY_TEXT(intern_str("Activation"), CLAY_TEXT_CONFIG({ .textColor = { 160, 165, 175, 255 }, .fontSize = 13, .wrapMode = CLAY_TEXT_WRAP_NONE }));
            }
            if (button(CLAY_ID("ActGELU"), "GELU", cur.activation == Activation::GELU, input, 55.0f, 26.0f)) cur.activation = Activation::GELU;
            if (button(CLAY_ID("ActReLU"), "ReLU", cur.activation == Activation::ReLU, input, 55.0f, 26.0f)) cur.activation = Activation::ReLU;
            if (button(CLAY_ID("ActTanh"), "Tanh", cur.activation == Activation::Tanh, input, 55.0f, 26.0f)) cur.activation = Activation::Tanh;
            if (button(CLAY_ID("ActSigmoid"), "Sigmoid", cur.activation == Activation::Sigmoid, input, 65.0f, 26.0f)) cur.activation = Activation::Sigmoid;
          }

          CLAY_AUTO_ID({
            .layout = { .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(32.0f) },
                        .childGap = 8,
                        .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT }
          }) {
            bool can_add = working_configs.size() < 10;
            bool can_rem = working_configs.size() > 1;
            bool can_up = selected_layer_idx > 0;
            bool can_down = selected_layer_idx + 1 < static_cast<int>(working_configs.size());

            if (button(CLAY_ID("BtnAddLayer"), "+ Add Layer", false, input, 106.0f, 30.0f, can_add)) {
              if (can_add) {
                working_configs.insert(working_configs.begin() + selected_layer_idx + 1, {32, Activation::GELU});
                selected_layer_idx++;
              }
            }
            if (button(CLAY_ID("BtnRemLayer"), "- Remove", false, input, 86.0f, 30.0f, can_rem)) {
              if (can_rem) {
                working_configs.erase(working_configs.begin() + selected_layer_idx);
                selected_layer_idx = std::clamp(selected_layer_idx, 0, static_cast<int>(working_configs.size() - 1));
              }
            }
            if (button(CLAY_ID("BtnMoveUp"), "Up", false, input, 46.0f, 30.0f, can_up)) {
              if (can_up) {
                std::swap(working_configs[selected_layer_idx], working_configs[selected_layer_idx - 1]);
                selected_layer_idx--;
              }
            }
            if (button(CLAY_ID("BtnMoveDown"), "Down", false, input, 56.0f, 30.0f, can_down)) {
              if (can_down) {
                std::swap(working_configs[selected_layer_idx], working_configs[selected_layer_idx + 1]);
                selected_layer_idx++;
              }
            }
          }
        }
      }

      CLAY(CLAY_ID("CardModelSummary"), {
        .layout = {
          .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_FIT() },
          .padding = { 16, 16, 16, 16 },
          .childGap = 10,
          .layoutDirection = CLAY_TOP_TO_BOTTOM
        },
        .backgroundColor = { 26, 29, 36, 255 },
        .cornerRadius = CLAY_CORNER_RADIUS(6.0f),
        .border = { .color = { 44, 49, 60, 255 }, .width = { 1, 1, 1, 1, 0 } }
      }) {
        card_header("Architecture Summary");

        size_t total_p = 0;
        int in_dim = MLP::INPUT_DIM;
        for (const auto& l : working_configs) {
          total_p += static_cast<size_t>(in_dim * l.units + l.units);
          in_dim = l.units;
        }
        total_p += static_cast<size_t>(in_dim * MLP::LOCO_DIM + MLP::LOCO_DIM);
        total_p += static_cast<size_t>(in_dim * MLP::COMBAT_DIM + MLP::COMBAT_DIM);

        stat_row("Hidden Layers", std::to_string(working_configs.size()));
        stat_row("Total Parameters", std::to_string(total_p), {100, 220, 140, 255});
        stat_row("Estimated Memory", std::format("{:.1f} KB", static_cast<float>(total_p * sizeof(float)) / 1024.0f));

        if (button(CLAY_ID("BtnApplyModel"), "Apply Architecture to Model", false, input, -1.0f, 34.0f)) {
          live_policy.model.set_architecture(working_configs);
          live_policy.model.initialize(trainer.rng);
          reset_live_simulation(live_policy);
          trainer.best_sample = live_policy;
          for (auto& p : trainer.population) {
            p = live_policy;
            p.initialize(trainer.rng);
          }
          trainer.save_best_sample("best_brain.safetensors");
          status_notification_ = "Architecture applied to live arena & trainer";
          notification_timer_ = 3.0f;
        }

        if (button(CLAY_ID("BtnDefaultModel"), "Reset to Defaults", false, input, -1.0f, 28.0f)) {
          working_configs = {
            {64, Activation::GELU},
            {64, Activation::GELU},
            {32, Activation::GELU}
          };
          selected_layer_idx = 0;
        }
      }
    }
  }
}

auto App::render() -> void {
  window_->clear(sf::Color(18, 20, 24));

  sf::Vector2f win_size(static_cast<float>(window_->getSize().x),
                        static_cast<float>(window_->getSize().y));

  ClayRenderer::instance().begin_frame(win_size, input_state_.mouse_pos, input_state_.mouse_down, input_state_.scroll_delta);

  CLAY(CLAY_ID("Root"), {
    .layout = {
      .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
      .layoutDirection = CLAY_TOP_TO_BOTTOM
    },
    .backgroundColor = { 18, 20, 24, 255 }
  }) {
    build_top_nav(input_state_);

    CLAY(CLAY_ID("MainContent"), {
      .layout = {
        .sizing = { CLAY_SIZING_GROW(), CLAY_SIZING_GROW() },
        .padding = { 16, 16, 16, 16 }
      }
    }) {
      if (current_view_ == ViewMode::Train) {
        build_train_view(input_state_);
      } else if (current_view_ == ViewMode::Analyze) {
        build_inspect_view(input_state_);
      } else if (current_view_ == ViewMode::Configure) {
        build_model_view(input_state_);
      }
    }
  }

  ClayRenderer::instance().end_frame(*window_);

  if (current_view_ == ViewMode::Train) {
    auto hist = trainer.get_history_metrics();

    std::vector<float> best_fit, avg_fit, std_fit;
    std::vector<float> best_err, avg_err, std_err;
    std::vector<float> best_surv, avg_surv, std_surv;
    std::vector<float> best_kills, avg_kills, std_kills;
    std::vector<float> best_dmg, avg_dmg, std_dmg;
    std::vector<float> best_wnorm, avg_wnorm, std_wnorm;

    best_fit.reserve(hist.size());
    avg_fit.reserve(hist.size());
    std_fit.reserve(hist.size());
    best_err.reserve(hist.size());
    avg_err.reserve(hist.size());
    std_err.reserve(hist.size());
    best_surv.reserve(hist.size());
    avg_surv.reserve(hist.size());
    std_surv.reserve(hist.size());
    best_kills.reserve(hist.size());
    avg_kills.reserve(hist.size());
    std_kills.reserve(hist.size());
    best_dmg.reserve(hist.size());
    avg_dmg.reserve(hist.size());
    std_dmg.reserve(hist.size());
    best_wnorm.reserve(hist.size());
    avg_wnorm.reserve(hist.size());
    std_wnorm.reserve(hist.size());

    for (const auto& m : hist) {
      best_fit.push_back(m.best_fitness);
      avg_fit.push_back(m.avg_fitness);
      std_fit.push_back(m.std_fitness);

      best_err.push_back(m.best_error);
      avg_err.push_back(m.avg_error);
      std_err.push_back(m.std_error);

      best_surv.push_back(m.best_survival_ticks);
      avg_surv.push_back(m.avg_survival_ticks);
      std_surv.push_back(m.std_survival_ticks);

      best_kills.push_back(m.best_kills);
      avg_kills.push_back(m.avg_kills);
      std_kills.push_back(m.std_kills);

      best_dmg.push_back(m.best_damage);
      avg_dmg.push_back(m.avg_damage);
      std_dmg.push_back(m.std_damage);

      best_wnorm.push_back(m.best_weight_norm);
      avg_wnorm.push_back(m.avg_weight_norm);
      std_wnorm.push_back(m.std_weight_norm);
    }

    auto fit_box = Clay_GetElementData(CLAY_ID("ChartFitnessBox")).boundingBox;
    draw_sparkline(*window_, sf::FloatRect({fit_box.x, fit_box.y}, {fit_box.width, fit_box.height}),
                   best_fit, sf::Color(0, 200, 80), avg_fit, sf::Color(50, 150, 255),
                   std_fit, sf::Color(245, 180, 50), 0.0f,
                   input_state_.mouse_pos, "Best", "Mean", "Std", config.generations);

    auto err_box = Clay_GetElementData(CLAY_ID("ChartErrorBox")).boundingBox;
    draw_sparkline(*window_, sf::FloatRect({err_box.x, err_box.y}, {err_box.width, err_box.height}),
                   best_err, sf::Color(0, 200, 80), avg_err, sf::Color(50, 150, 255),
                   std_err, sf::Color(245, 180, 50), 0.0f,
                   input_state_.mouse_pos, "Best", "Mean", "Std", config.generations);

    auto surv_box = Clay_GetElementData(CLAY_ID("ChartSurvivalBox")).boundingBox;
    draw_sparkline(*window_, sf::FloatRect({surv_box.x, surv_box.y}, {surv_box.width, surv_box.height}),
                   best_surv, sf::Color(0, 200, 80), avg_surv, sf::Color(50, 150, 255),
                   std_surv, sf::Color(245, 180, 50), static_cast<float>(config.max_eval_ticks),
                   input_state_.mouse_pos, "Best", "Mean", "Std", config.generations);

    auto kill_box = Clay_GetElementData(CLAY_ID("ChartKillsBox")).boundingBox;
    draw_sparkline(*window_, sf::FloatRect({kill_box.x, kill_box.y}, {kill_box.width, kill_box.height}),
                   best_kills, sf::Color(0, 200, 80), avg_kills, sf::Color(50, 150, 255),
                   std_kills, sf::Color(245, 180, 50), 0.0f,
                   input_state_.mouse_pos, "Best", "Mean", "Std", config.generations);

    auto dmg_box = Clay_GetElementData(CLAY_ID("ChartDamageBox")).boundingBox;
    draw_sparkline(*window_, sf::FloatRect({dmg_box.x, dmg_box.y}, {dmg_box.width, dmg_box.height}),
                   best_dmg, sf::Color(0, 200, 80), avg_dmg, sf::Color(50, 150, 255),
                   std_dmg, sf::Color(245, 180, 50), 0.0f,
                   input_state_.mouse_pos, "Best", "Mean", "Std", config.generations);

    auto mlp_box = Clay_GetElementData(CLAY_ID("ChartMlpBox")).boundingBox;
    draw_sparkline(*window_, sf::FloatRect({mlp_box.x, mlp_box.y}, {mlp_box.width, mlp_box.height}),
                   best_wnorm, sf::Color(0, 200, 80), avg_wnorm, sf::Color(50, 150, 255),
                   std_wnorm, sf::Color(245, 180, 50), 0.0f,
                   input_state_.mouse_pos, "Best", "Mean", "Std", config.generations);
  } else if (current_view_ == ViewMode::Analyze) {
    auto sim_box = Clay_GetElementData(CLAY_ID("SimViewport")).boundingBox;
    if (sim_box.width > 20.0f && sim_box.height > 20.0f) {
      sf::FloatRect vport({sim_box.x / win_size.x, sim_box.y / win_size.y},
                          {sim_box.width / win_size.x, sim_box.height / win_size.y});

      sf::View game_view(sf::FloatRect({0.0f, 0.0f}, {sim_box.width, sim_box.height}));
      game_view.setViewport(vport);

      if (is_checkpoint_loaded_) {
        bool found_adv = false;
        if (camera_follow) {
          auto adv_view = live_registry.view<Adventurer, Transform>(entt::exclude<Dead>);
          auto it = adv_view.begin();
          if (it != adv_view.end()) {
            const auto& pos = adv_view.get<Transform>(*it).position;
            game_view.setCenter(sf::Vector2f(pos.x * 15.0f, pos.y * 15.0f));
            found_adv = true;
          }
        }
        if (!camera_follow || !found_adv) {
          game_view.setCenter(sf::Vector2f(0.0f, 0.0f));
        }

        window_->setView(game_view);
        render_world(*window_, live_registry, game_view, debug_flags, 0.016f);
        render_floating_combat_text(*window_, live_registry, game_view);

        window_->setView(window_->getDefaultView());

        sf::Font& font = ClayRenderer::instance().get_font();
        float hud_x = sim_box.x + 12.0f;
        float hud_y = sim_box.y + 12.0f;
        float live_score = compute_fitness(live_arena);

        sf::RectangleShape hud_bg(sf::Vector2f(390.0f, 36.0f));
        hud_bg.setPosition(sf::Vector2f(hud_x, hud_y));
        hud_bg.setFillColor(sf::Color(16, 18, 24, 230));
        hud_bg.setOutlineColor(live_arena.game_over ? sf::Color(180, 50, 50) : sf::Color(50, 100, 160));
        hud_bg.setOutlineThickness(1.0f);
        window_->draw(hud_bg);

        std::string status_badge = live_arena.game_over ? "[DEFEATED]" : (is_sim_playing ? "[LIVE]" : "[PAUSED]");
        sf::Color status_col = live_arena.game_over ? sf::Color(255, 80, 80) : (is_sim_playing ? sf::Color(80, 240, 120) : sf::Color(240, 180, 50));

        sf::Text t_status(font, status_badge, 14);
        t_status.setFillColor(status_col);
        t_status.setPosition(sf::Vector2f(hud_x + 10.0f, hud_y + 8.0f));
        window_->draw(t_status);

        std::string stats_str = std::format("Score: {:.0f}   Ticks: {}   Kills: {}", live_score, live_arena.ticks, live_arena.slimes_killed);
        sf::Text t_stats(font, stats_str, 14);
        t_stats.setFillColor(sf::Color(220, 225, 235));
        t_stats.setPosition(sf::Vector2f(hud_x + 105.0f, hud_y + 8.0f));
        window_->draw(t_stats);

        if (live_arena.game_over) {
          float cx = sim_box.x + sim_box.width * 0.5f;
          float cy = sim_box.y + sim_box.height * 0.5f;

          sf::RectangleShape banner(sf::Vector2f(420.0f, 80.0f));
          banner.setOrigin(sf::Vector2f(210.0f, 40.0f));
          banner.setPosition(sf::Vector2f(cx, cy));
          banner.setFillColor(sf::Color(20, 15, 18, 240));
          banner.setOutlineColor(sf::Color(220, 60, 60));
          banner.setOutlineThickness(1.5f);
          window_->draw(banner);

          sf::Text over_txt(font, "GAME OVER - ADVENTURER DEFEATED", 16);
          over_txt.setFillColor(sf::Color(255, 90, 90));
          auto ob = over_txt.getLocalBounds();
          over_txt.setOrigin(sf::Vector2f(ob.size.x * 0.5f, 0.0f));
          over_txt.setPosition(sf::Vector2f(cx, cy - 28.0f));
          window_->draw(over_txt);

          std::string fin_str = std::format("Final Score: {:.0f}  |  Ticks Lived: {}  |  Kills: {}", live_score, live_arena.ticks, live_arena.slimes_killed);
          sf::Text fin_txt(font, fin_str, 14);
          fin_txt.setFillColor(sf::Color(220, 225, 235));
          auto fb = fin_txt.getLocalBounds();
          fin_txt.setOrigin(sf::Vector2f(fb.size.x * 0.5f, 0.0f));
          fin_txt.setPosition(sf::Vector2f(cx, cy + 4.0f));
          window_->draw(fin_txt);
        }

        if (!is_sim_playing && live_arena.ticks == 0 && !live_arena.game_over) {
          float cx = sim_box.x + sim_box.width * 0.5f;
          float cy = sim_box.y + sim_box.height * 0.5f;

          sf::RectangleShape banner(sf::Vector2f(380.0f, 46.0f));
          banner.setOrigin(sf::Vector2f(190.0f, 23.0f));
          banner.setPosition(sf::Vector2f(cx, cy));
          banner.setFillColor(sf::Color(20, 24, 32, 230));
          banner.setOutlineColor(sf::Color(60, 140, 220));
          banner.setOutlineThickness(1.0f);
          window_->draw(banner);

          sf::Text prompt_txt(font, "READY - PRESS PLAY OR SPACE TO RUN", 14);
          prompt_txt.setFillColor(sf::Color(140, 210, 255));
          auto pb = prompt_txt.getLocalBounds();
          prompt_txt.setOrigin(sf::Vector2f(pb.size.x * 0.5f, pb.size.y * 0.5f));
          prompt_txt.setPosition(sf::Vector2f(cx, cy - 2.0f));
          window_->draw(prompt_txt);
        }

        struct SkillSlotInfo {
          std::string name;
          std::string key;
          std::string sub_info;
          float cd_current;
          float cd_max;
          bool is_active;
          Action action_type;
        };

        SkillSlotInfo slots[3] = {
          {"Cleave", "Q", "20 st", 0.0f, 30.0f, false, Action::Cleave},
          {"Shield", "W", "50 st", 0.0f, 50.0f, false, Action::Shield},
          {"Warcry", "E", "40 st", 0.0f, 300.0f, false, Action::Warcry}
        };

        auto adv_ents = live_registry.view<Adventurer>(entt::exclude<Dead>);
        if (adv_ents.begin() != adv_ents.end()) {
          auto adv = *adv_ents.begin();
          if (auto* act = live_registry.try_get<ActionState>(adv)) {
            if (act->action == Action::Cleave) slots[0].is_active = true;
            if (act->action == Action::Shield) slots[1].is_active = true;
            if (act->action == Action::Warcry) slots[2].is_active = true;
          }
          if (auto* cd = live_registry.try_get<Cooldown<CleaveSkill>>(adv)) {
            slots[0].cd_current = cd->time_left;
          }
          if (auto* sk = live_registry.try_get<CleaveSkill>(adv)) {
            slots[0].cd_max = sk->cooldown;
          }
          if (auto* cd = live_registry.try_get<Cooldown<ShieldSkill>>(adv)) {
            slots[1].cd_current = cd->time_left;
          }
          if (auto* sk = live_registry.try_get<ShieldSkill>(adv)) {
            slots[1].cd_max = sk->cooldown;
          }
          if (auto* cd = live_registry.try_get<Cooldown<WarcrySkill>>(adv)) {
            slots[2].cd_current = cd->time_left;
          }
          if (auto* sk = live_registry.try_get<WarcrySkill>(adv)) {
            slots[2].cd_max = sk->cooldown;
          }
        }

        if (action_hint_timer_ > 0.0f) {
          if (last_action_hint_ == Action::Cleave) slots[0].is_active = true;
          if (last_action_hint_ == Action::Shield) slots[1].is_active = true;
          if (last_action_hint_ == Action::Warcry) slots[2].is_active = true;
        }

        float slot_w = 92.0f;
        float slot_h = 62.0f;
        float slot_gap = 10.0f;
        float total_bar_w = 3.0f * slot_w + 2.0f * slot_gap;
        float bar_padding = 8.0f;
        float bar_box_w = total_bar_w + bar_padding * 2.0f;
        float bar_box_h = slot_h + bar_padding * 2.0f;

        float bar_x = sim_box.x + (sim_box.width - bar_box_w) * 0.5f;
        float bar_y = sim_box.y + sim_box.height - (bar_box_h + 12.0f);

        sf::RectangleShape bar_bg(sf::Vector2f(bar_box_w, bar_box_h));
        bar_bg.setPosition(sf::Vector2f(bar_x, bar_y));
        bar_bg.setFillColor(sf::Color(16, 18, 24, 230));
        bar_bg.setOutlineColor(sf::Color(45, 52, 65, 220));
        bar_bg.setOutlineThickness(1.0f);
        window_->draw(bar_bg);

        for (int i = 0; i < 3; ++i) {
          float sx = bar_x + bar_padding + i * (slot_w + slot_gap);
          float sy = bar_y + bar_padding;
          sf::FloatRect slot_rect({sx, sy}, {slot_w, slot_h});

          bool hovered = slot_rect.contains(input_state_.mouse_pos);
          if (hovered && input_state_.mouse_clicked && !live_arena.game_over) {
            manual_action_override_ = slots[i].action_type;
            last_action_hint_ = slots[i].action_type;
            action_hint_timer_ = 0.35f;
          }

          sf::RectangleShape slot_box(sf::Vector2f(slot_w, slot_h));
          slot_box.setPosition(sf::Vector2f(sx, sy));

          if (slots[i].is_active) {
            slot_box.setFillColor(sf::Color(55, 45, 18, 240));
            slot_box.setOutlineColor(sf::Color(255, 215, 50));
            slot_box.setOutlineThickness(2.0f);
          } else if (slots[i].cd_current > 0.0f) {
            slot_box.setFillColor(hovered ? sf::Color(28, 30, 38) : sf::Color(20, 22, 28));
            slot_box.setOutlineColor(sf::Color(55, 60, 72));
            slot_box.setOutlineThickness(1.0f);
          } else {
            slot_box.setFillColor(hovered ? sf::Color(32, 42, 40) : sf::Color(24, 28, 34));
            slot_box.setOutlineColor(hovered ? sf::Color(70, 180, 120) : sf::Color(45, 130, 80));
            slot_box.setOutlineThickness(1.0f);
          }
          window_->draw(slot_box);

          if (slots[i].cd_current > 0.0f && slots[i].cd_max > 0.0f) {
            float cd_pct = std::clamp(slots[i].cd_current / slots[i].cd_max, 0.0f, 1.0f);
            float sweep_h = slot_h * cd_pct;
            sf::RectangleShape sweep(sf::Vector2f(slot_w, sweep_h));
            sweep.setPosition(sf::Vector2f(sx, sy + (slot_h - sweep_h)));
            sweep.setFillColor(sf::Color(0, 0, 0, 160));
            window_->draw(sweep);
          }

          sf::Text t_name(font, slots[i].name, 13);
          t_name.setFillColor(sf::Color(220, 225, 235));
          t_name.setPosition(sf::Vector2f(sx + 6.0f, sy + 4.0f));
          window_->draw(t_name);

          sf::Text t_key(font, slots[i].key, 12);
          t_key.setFillColor(slots[i].is_active ? sf::Color(255, 220, 80) : sf::Color(130, 145, 165));
          auto kb = t_key.getLocalBounds();
          t_key.setPosition(sf::Vector2f(sx + slot_w - kb.size.x - 6.0f, sy + 4.0f));
          window_->draw(t_key);

          if (slots[i].is_active) {
            sf::Text t_stat(font, "PRESSED", 13);
            t_stat.setStyle(sf::Text::Bold);
            t_stat.setFillColor(sf::Color(255, 225, 70));
            auto sb = t_stat.getLocalBounds();
            t_stat.setPosition(sf::Vector2f(sx + (slot_w - sb.size.x) * 0.5f, sy + 23.0f));
            window_->draw(t_stat);
          } else if (slots[i].cd_current > 0.0f) {
            float secs = slots[i].cd_current / 60.0f;
            std::string cd_str = std::format("{:.1f}s", secs);
            sf::Text t_stat(font, cd_str, 14);
            t_stat.setStyle(sf::Text::Bold);
            t_stat.setFillColor(sf::Color(255, 110, 110));
            auto sb = t_stat.getLocalBounds();
            t_stat.setPosition(sf::Vector2f(sx + (slot_w - sb.size.x) * 0.5f, sy + 22.0f));
            window_->draw(t_stat);
          } else {
            sf::Text t_stat(font, "READY", 13);
            t_stat.setFillColor(sf::Color(80, 230, 130));
            auto sb = t_stat.getLocalBounds();
            t_stat.setPosition(sf::Vector2f(sx + (slot_w - sb.size.x) * 0.5f, sy + 23.0f));
            window_->draw(t_stat);
          }

          sf::Text t_sub(font, slots[i].sub_info, 11);
          t_sub.setFillColor(sf::Color(120, 130, 145));
          auto sub_b = t_sub.getLocalBounds();
          t_sub.setPosition(sf::Vector2f(sx + (slot_w - sub_b.size.x) * 0.5f, sy + 43.0f));
          window_->draw(t_sub);
        }
      } else {
        window_->setView(window_->getDefaultView());
        sf::Font& font = ClayRenderer::instance().get_font();

        float cx = sim_box.x + sim_box.width * 0.5f;
        float cy = sim_box.y + sim_box.height * 0.5f;

        sf::RectangleShape banner(sf::Vector2f(380.0f, 48.0f));
        banner.setOrigin(sf::Vector2f(190.0f, 24.0f));
        banner.setPosition(sf::Vector2f(cx, cy));
        banner.setFillColor(sf::Color(20, 24, 34, 230));
        banner.setOutlineColor(sf::Color(55, 65, 80));
        banner.setOutlineThickness(1.0f);
        window_->draw(banner);

        sf::Text prompt_txt(font, "Select a checkpoint to run simulation", 15);
        prompt_txt.setFillColor(sf::Color(160, 175, 195));
        auto pb = prompt_txt.getLocalBounds();
        prompt_txt.setOrigin(sf::Vector2f(pb.size.x * 0.5f, pb.size.y * 0.5f));
        prompt_txt.setPosition(sf::Vector2f(cx, cy - 2.0f));
        window_->draw(prompt_txt);
      }
    }
  }

  if (!status_notification_.empty()) {
    sf::RectangleShape banner(sf::Vector2f(win_size.x, 28.0f));
    banner.setPosition(sf::Vector2f(0.0f, win_size.y - 28.0f));
    banner.setFillColor(sf::Color(30, 110, 200, 230));
    window_->draw(banner);

    sf::Text notif_text(ClayRenderer::instance().get_font(), status_notification_, 15);
    notif_text.setPosition(sf::Vector2f(16.0f, win_size.y - 23.0f));
    notif_text.setFillColor(sf::Color::White);
    window_->draw(notif_text);
  }

  window_->display();
}

auto App::run() -> int {
  sf::Clock clock;
  while (window_->isOpen()) {
    float dt = clock.restart().asSeconds();
    if (!process_events()) {
      break;
    }
    update(dt);
    render();
  }
  return 0;
}

}
