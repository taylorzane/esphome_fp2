#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
namespace esphome::aqara_fp2 {
inline bool radar_traffic_expired(uint32_t now, uint32_t last, uint32_t timeout) {
  return uint32_t(now - last) > timeout;
}
// Matches Nailik's 14x14 card view, which occupies protocol columns 2..15.
inline bool target_cell(int16_t x, int16_t y, uint8_t mounting, float units_per_cell, int &col, int &row) {
  if (!std::isfinite(units_per_cell) || units_per_cell <= 0) return false;
  float gx, gy;
  if (mounting == 2 || mounting == 3) {
    gx = (-float(x) + 400) * 14 / 800;
    gy = float(y) * 14 / 800;
  } else {
    gx = 7 - x / units_per_cell;
    gy = y / units_per_cell;
  }
  if (gx < 0 || gx >= 14 || gy < 0 || gy >= 14) return false;
  col = 2 + int(std::floor(gx));
  row = int(std::floor(gy));
  return true;
}
using TargetGrid = std::array<uint8_t, 40>;
struct DerivedTargets {
  uint8_t count{0};
  bool motion{false};
  std::array<uint8_t, 32> zone_counts{};
  std::array<bool, 32> zone_motion{};
};
inline DerivedTargets derive_targets(const std::vector<uint8_t> &blob, uint8_t count,
                                     uint8_t mounting, float units_per_cell,
                                     const TargetGrid *edge, const TargetGrid *interference,
                                     const TargetGrid *exit, bool require_active,
                                     const std::array<const TargetGrid *, 32> &zones) {
  DerivedTargets result;
  if (blob.size() != 1 + size_t(count) * 14) return result;
  auto has_cell = [](const TargetGrid *grid, int col, int row) {
    return grid && ((uint16_t((*grid)[row * 2]) << 8 | (*grid)[row * 2 + 1]) & (1u << (15 - col)));
  };
  for (size_t i = 0; i < count; ++i) {
    size_t off = 1 + i * 14;
    int16_t x = int16_t(uint16_t(blob[off + 1]) << 8 | blob[off + 2]);
    int16_t y = int16_t(uint16_t(blob[off + 3]) << 8 | blob[off + 4]);
    int16_t velocity = int16_t(uint16_t(blob[off + 7]) << 8 | blob[off + 8]);
    int col, row;
    if ((require_active && blob[off + 13] == 0) || !target_cell(x, y, mounting, units_per_cell, col, row)) continue;
    if (has_cell(edge, col, row) || has_cell(interference, col, row) || has_cell(exit, col, row)) continue;
    ++result.count;
    const bool moving = std::abs(int(velocity)) > 15;
    result.motion |= moving;
    for (size_t id = 0; id < zones.size(); ++id) {
      if (has_cell(zones[id], col, row)) {
        ++result.zone_counts[id];
        result.zone_motion[id] = result.zone_motion[id] || moving;
      }
    }
  }
  return result;
}
}  // namespace esphome::aqara_fp2
