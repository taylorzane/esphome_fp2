#include <cassert>
#include <cmath>
#include "components/aqara_fp2/target_geometry.h"
using namespace esphome::aqara_fp2;
int main() {
  assert(!radar_traffic_expired(1000, 500, 15000));
  assert(radar_traffic_expired(16001, 500, 15000));
  assert(!radar_traffic_expired(10, 0xfffffff0u, 15000));
  int col, row;
  assert(target_cell(0, 100, 1, 50, col, row) && col == 9 && row == 2);
  assert(target_cell(0, 100, 1, 20, col, row) && col == 9 && row == 5);
  assert(target_cell(0, 400, 2, 50, col, row) && col == 9 && row == 7);
  assert(!target_cell(0, -1, 1, 50, col, row));
  assert(!target_cell(2000, 0, 1, 50, col, row));
  assert(!target_cell(0, 100, 1, 0, col, row));
}
