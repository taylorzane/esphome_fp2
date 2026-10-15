import unittest
from esphome import config_validation as cv
from components.aqara_fp2 import validate_zone_ids, parse_ascii_grid

class ZoneSchemaTests(unittest.TestCase):
    def test_duplicate_explicit_ids_are_rejected(self):
        with self.assertRaises(cv.Invalid):
            validate_zone_ids([{'zone_id': 7}, {'zone_id': 7}])

    def test_explicit_and_implicit_collision_is_rejected(self):
        with self.assertRaises(cv.Invalid):
            validate_zone_ids([{'zone_id': 2}, {}])

    def test_protocol_capacity_is_enforced(self):
        with self.assertRaises(cv.Invalid):
            validate_zone_ids([{}] * 32)

    def test_sparse_ids_are_preserved(self):
        zones = [{'zone_id': 20}, {'zone_id': 3}]
        self.assertEqual(validate_zone_ids(zones), zones)

    def test_corner_grid_matches_protocol_cell_coordinates(self):
        rows = ['.' * 14 for _ in range(14)]
        rows[2] = '.' * 7 + 'X' + '.' * 6
        grid = parse_ascii_grid('\n'.join(rows))
        self.assertEqual(len(grid), 40)
        self.assertEqual(grid[4:6], [0x00, 0x40])  # protocol column 9
