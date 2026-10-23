#include "padv/CheckpointManager.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

namespace padv {

namespace fs = std::filesystem;

namespace {

auto read_safetensors_params(const std::string& path) -> Size {
  std::ifstream is(path, std::ios::binary);
  if (!is.is_open()) return 0;
  uint64_t header_len = 0;
  is.read(reinterpret_cast<char*>(&header_len), sizeof(header_len));
  if (header_len == 0 || header_len > 65536) return 0;
  std::string header(header_len, '\0');
  is.read(header.data(), static_cast<std::streamsize>(header_len));
  size_t pos = header.find("\"parameters\":\"");
  if (pos != std::string::npos) {
    size_t start = pos + 14;
    size_t end = header.find('"', start);
    if (end != std::string::npos) {
      try {
        return static_cast<Size>(std::stoull(header.substr(start, end - start)));
      } catch (...) {}
    }
  }
  return 0;
}

}

CheckpointManager::CheckpointManager(std::string dir) : checkpoint_dir(std::move(dir)) {
  std::error_code ec;
  fs::create_directories(checkpoint_dir, ec);
  refresh_cache();
}

auto CheckpointManager::set_policy(CheckpointRetention new_policy, I32 k, I32 interval) -> void {
  policy = new_policy;
  top_k = k;
  periodic_interval = std::max(1, interval);
}

auto CheckpointManager::refresh_cache() -> void {
  cache_.clear();
  std::error_code ec;
  if (!fs::exists(checkpoint_dir, ec)) return;

  for (const auto& entry : fs::directory_iterator(checkpoint_dir, ec)) {
    if (!entry.is_regular_file()) continue;

    std::string fname = entry.path().filename().string();
    std::string fpath = entry.path().string();

    int gen = 0;
    long long fit = 0;
    if (sscanf_s(fname.c_str(), "checkpoint_gen_%d_fit_%lld.safetensors", &gen, &fit) == 2 ||
        sscanf_s(fname.c_str(), "checkpoint_gen_%d_fit_%lld.bin", &gen, &fit) == 2) {
      CheckpointMeta meta;
      meta.filename = fname;
      meta.filepath = fpath;
      meta.generation = gen;
      meta.fitness = static_cast<F32>(fit);
      meta.parameter_count = read_safetensors_params(fpath);
      meta.file_size = entry.file_size(ec);
      cache_.push_back(std::move(meta));
    } else if (fname == "best_brain.safetensors" || fname == "best_brain.dat") {
      CheckpointMeta meta;
      meta.filename = fname;
      meta.filepath = fpath;
      meta.generation = 0;
      meta.fitness = 0.0f;
      meta.parameter_count = read_safetensors_params(fpath);
      meta.file_size = entry.file_size(ec);
      cache_.push_back(std::move(meta));
    }
  }

  std::sort(cache_.begin(), cache_.end(), [](const CheckpointMeta& a, const CheckpointMeta& b) {
    if (a.fitness != b.fitness) return a.fitness > b.fitness;
    return a.generation > b.generation;
  });
}

auto CheckpointManager::record_checkpoint(I32 generation, F32 fitness, const ModalPolicy& brain) -> Bool {
  std::error_code ec;
  fs::create_directories(checkpoint_dir, ec);

  if (policy == CheckpointRetention::Periodic) {
    if (generation % periodic_interval != 0) {
      return false;
    }
  } else if (policy == CheckpointRetention::BestOnly) {
    refresh_cache();
    if (!cache_.empty() && fitness <= cache_.front().fitness) {
      return false;
    }
  }

  char buf[128];
  std::snprintf(buf, sizeof(buf), "checkpoint_gen_%d_fit_%lld.safetensors", generation,
                static_cast<long long>(std::round(fitness)));
  std::string filename = buf;
  fs::path full_path = fs::path(checkpoint_dir) / filename;

  if (!brain.save_safetensors(full_path.string())) {
    spdlog::error("CheckpointManager: Failed to write safetensors checkpoint to {}", full_path.string());
    return false;
  }

  spdlog::info("CheckpointManager: Saved safetensors checkpoint '{}' (Fitness: {:.1f})", filename, fitness);
  refresh_cache();
  prune_old_checkpoints();
  return true;
}

auto CheckpointManager::list_checkpoints() -> std::vector<CheckpointMeta> {
  refresh_cache();
  return cache_;
}

auto CheckpointManager::load_checkpoint(const std::string& filepath_or_filename, ModalPolicy& out_policy) -> Bool {
  fs::path p(filepath_or_filename);
  if (!fs::exists(p)) {
    p = fs::path(checkpoint_dir) / p.filename();
  }

  if (!fs::exists(p)) {
    spdlog::error("CheckpointManager: Checkpoint file not found on disk: {}", p.string());
    return false;
  }

  bool ok = out_policy.load(p.string());
  if (ok) {
    spdlog::info("CheckpointManager: Loaded model weights from {}", p.string());
  } else {
    spdlog::error("CheckpointManager: Failed to parse model weights from {}", p.string());
  }
  return ok;
}

auto CheckpointManager::prune_old_checkpoints() -> void {
  std::error_code ec;

  if (policy == CheckpointRetention::TopK) {
    if (static_cast<I32>(cache_.size()) > top_k) {
      for (size_t i = static_cast<size_t>(top_k); i < cache_.size(); ++i) {
        fs::remove(cache_[i].filepath, ec);
        spdlog::debug("CheckpointManager: Pruned old checkpoint {}", cache_[i].filename);
      }
      cache_.resize(static_cast<size_t>(top_k));
    }
  } else if (policy == CheckpointRetention::BestOnly) {
    if (cache_.size() > 1) {
      for (size_t i = 1; i < cache_.size(); ++i) {
        fs::remove(cache_[i].filepath, ec);
      }
      cache_.resize(1);
    }
  } else if (policy == CheckpointRetention::Periodic) {
    std::vector<CheckpointMeta> remaining;
    for (const auto& meta : cache_) {
      if (meta.generation % periodic_interval != 0) {
        fs::remove(meta.filepath, ec);
      } else {
        remaining.push_back(meta);
      }
    }
    cache_ = std::move(remaining);
  }
}

}  // namespace padv
