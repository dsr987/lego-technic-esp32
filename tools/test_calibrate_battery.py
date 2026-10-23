#!/usr/bin/env python3
"""Модульные тесты калькулятора калибровки батареи."""

import argparse
import unittest

from calibrate_battery import (
    calculate_scale_factor,
    calculate_updated_factor,
    parse_pair,
)


class BatteryCalibrationTests(unittest.TestCase):
    def test_single_pair_returns_reference_over_display(self):
        self.assertAlmostEqual(calculate_scale_factor([(7.82, 8.04)]), 8.04 / 7.82)

    def test_multiple_pairs_fit_common_scale(self):
        readings = [(7.0, 7.14), (8.0, 8.16), (6.5, 6.63)]
        self.assertAlmostEqual(calculate_scale_factor(readings), 1.02, places=8)

    def test_updated_factor_multiplies_existing_calibration(self):
        readings = [(7.0, 7.14), (8.0, 8.16)]
        self.assertAlmostEqual(calculate_updated_factor(readings, 1.05), 1.071, places=8)

    def test_parser_rejects_zero_or_invalid_values(self):
        with self.assertRaises(argparse.ArgumentTypeError):
            parse_pair("0,8.0")
        with self.assertRaises(argparse.ArgumentTypeError):
            parse_pair("not-a-number,8.0")

    def test_empty_readings_are_rejected(self):
        with self.assertRaises(ValueError):
            calculate_scale_factor([])


if __name__ == "__main__":
    unittest.main()
