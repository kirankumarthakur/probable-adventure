#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "Types.hpp"

namespace padv {

class SpatialGrid2D {
 public:
  explicit SpatialGrid2D(F32 cell_size = 6.0f);

  auto clear() -> void;
  auto insert(entt::entity entity, glm::vec2 position) -> void;
  auto query_radius(glm::vec2 position, F32 radius, std::vector<entt::entity>& out_results) const -> void;

 private:
  F32 m_cell_size;
  F32 m_inv_cell_size;
  std::unordered_map<I64, std::vector<entt::entity>> m_buckets;

  [[nodiscard]] auto hash_key(I32 cx, I32 cy) const -> I64 {
    return (static_cast<I64>(cx) << 32) ^ (static_cast<I64>(static_cast<std::uint32_t>(cy)));
  }
};

}  // namespace padv
