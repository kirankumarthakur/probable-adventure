#include "padv/SpatialGrid2D.hpp"

#include <cmath>

namespace padv {

SpatialGrid2D::SpatialGrid2D(F32 cell_size)
    : m_cell_size(cell_size), m_inv_cell_size(1.0f / cell_size) {}

auto SpatialGrid2D::clear() -> void {
  m_buckets.clear();
}

auto SpatialGrid2D::insert(entt::entity entity, glm::vec2 position) -> void {
  I32 cx  = static_cast<I32>(std::floor(position.x * m_inv_cell_size));
  I32 cy  = static_cast<I32>(std::floor(position.y * m_inv_cell_size));
  I64 key = hash_key(cx, cy);
  m_buckets[key].push_back(entity);
}

auto SpatialGrid2D::query_radius(glm::vec2 position, F32 radius,
                                 std::vector<entt::entity>& out_results) const -> void {
  out_results.clear();

  I32 min_cx = static_cast<I32>(std::floor((position.x - radius) * m_inv_cell_size));
  I32 max_cx = static_cast<I32>(std::floor((position.x + radius) * m_inv_cell_size));
  I32 min_cy = static_cast<I32>(std::floor((position.y - radius) * m_inv_cell_size));
  I32 max_cy = static_cast<I32>(std::floor((position.y + radius) * m_inv_cell_size));

  for (I32 cx = min_cx; cx <= max_cx; ++cx) {
    for (I32 cy = min_cy; cy <= max_cy; ++cy) {
      I64  key = hash_key(cx, cy);
      auto it  = m_buckets.find(key);
      if (it != m_buckets.end()) {
        for (auto entity : it->second) {
          out_results.push_back(entity);
        }
      }
    }
  }
}

}  // namespace padv
