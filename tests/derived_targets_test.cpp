#include <array>
#include <vector>

#include <cassert>
#include "components/aqara_fp2/target_geometry.h"
using namespace esphome::aqara_fp2;
int main() {
  std::vector<uint8_t> blob(1 + 2 * 14, 0);
  blob[0] = 2;
  blob[1 + 4] = 100;  // y=100 => protocol column 9, row 2 at 50 units/cell
  blob[1 + 13] = 1;
  blob[15 + 4] = 100;
  blob[15 + 8] = 20;  // second target moving, but activity flag clear
  TargetGrid grid{};
  grid[4] = 0;
  grid[5] = 0x40;
  std::array<const TargetGrid *, 32> zones{};
  zones[7] = &grid;
  auto all = derive_targets(blob, 2, 1, 50, nullptr, nullptr, nullptr, false, zones);
  assert(all.count == 2 && all.motion && all.zone_counts[7] == 2 && all.zone_motion[7]);
  auto active = derive_targets(blob, 2, 1, 50, nullptr, nullptr, nullptr, true, zones);
  assert(active.count == 1 && !active.motion && active.zone_counts[7] == 1 && !active.zone_motion[7]);
  assert(derive_targets(blob, 2, 1, 50, &grid, nullptr, nullptr, false, zones).count == 0);
  assert(derive_targets(blob, 2, 1, 50, nullptr, &grid, nullptr, false, zones).count == 0);
  assert(derive_targets(blob, 2, 1, 50, nullptr, nullptr, &grid, false, zones).count == 0);
  assert(derive_targets(blob, 2, 1, 20, nullptr, nullptr, nullptr, false, zones).zone_counts[7] == 0);
  blob.resize(15);  // truncated second record is rejected, not partially published
  assert(derive_targets(blob, 2, 1, 50, nullptr, nullptr, nullptr, false, zones).count == 0);
}
