#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Config.hpp"
#include "ModalPolicy.hpp"
#include "Types.hpp"

namespace padv {

struct CheckpointMeta {
  std::string filename;
  std::string filepath;
  I32 generation{0};
  F32 fitness{0.0f};
  Size parameter_count{0};
  uintmax_t file_size{0};
  std::string timestamp;
};

class CheckpointManager {
 public:
  std::string checkpoint_dir = "checkpoints";
  CheckpointRetention policy = CheckpointRetention::TopK;
  I32 top_k = 10;
  I32 periodic_interval = 5;

  explicit CheckpointManager(std::string dir = "checkpoints");

  auto set_policy(CheckpointRetention new_policy, I32 k = 10, I32 interval = 5) -> void;
  auto record_checkpoint(I32 generation, F32 fitness, const ModalPolicy& policy) -> Bool;
  [[nodiscard]] auto list_checkpoints() -> std::vector<CheckpointMeta>;
  [[nodiscard]] auto load_checkpoint(const std::string& filepath_or_filename, ModalPolicy& out_policy) -> Bool;
  auto prune_old_checkpoints() -> void;

 private:
  std::vector<CheckpointMeta> cache_;
  auto refresh_cache() -> void;
};

}  // namespace padv
