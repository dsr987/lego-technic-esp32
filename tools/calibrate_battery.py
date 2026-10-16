#!/usr/bin/env python3
"""Calculate the battery voltage calibration multiplier from multimeter readings.

Pass one or more pairs as --pair CONTROLLER_V,MULTIMETER_V.
The controller value must be the voltage currently shown by the firmware.
The script fits a multiplicative correction through the origin, which is suitable
for correcting a scale error. It cannot remove ADC non-linearity or an offset error.
"""

from __future__ import annotations

import argparse
import math
from typing import Sequence

Reading = tuple[float, float]


def parse_pair(value: str) -> Reading:
    """Parse one CONTROLLER_V,MULTIMETER_V pair."""
    parts = value.split(",")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(
            "expected CONTROLLER_V,MULTIMETER_V, for example 7.82,8.04"
        )

    try:
        controller_v, multimeter_v = (float(part.strip()) for part in parts)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("both readings must be numbers") from exc

    if not all(math.isfinite(v) and v > 0 for v in (controller_v, multimeter_v)):
        raise argparse.ArgumentTypeError("both readings must be finite values above 0 V")

    return controller_v, multimeter_v


def calculate_scale_factor(readings: Sequence[Reading]) -> float:
    """Find the least-squares scale factor mapping controller readings to the meter."""
    if not readings:
        raise ValueError("at least one reading pair is required")

    denominator = sum(controller_v * controller_v for controller_v, _ in readings)
    if denominator <= 0:
        raise ValueError("controller readings must be above 0 V")

    numerator = sum(controller_v * multimeter_v for controller_v, multimeter_v in readings)
    factor = numerator / denominator

    if not math.isfinite(factor) or factor <= 0:
        raise ValueError("calculated scale factor is invalid")

    return factor


def calculate_updated_factor(
    readings: Sequence[Reading], current_factor: float = 1.0
) -> float:
    """Return the new firmware multiplier, starting from the currently installed value."""
    if not math.isfinite(current_factor) or current_factor <= 0:
        raise ValueError("current calibration factor must be finite and above 0")
    return current_factor * calculate_scale_factor(readings)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Calculate BATT_CALIBRATION_FACTOR from voltage pairs. "
            "Each --pair is CURRENT_CONTROLLER_READING,MULTIMETER_REFERENCE."
        )
    )
    parser.add_argument(
        "--pair",
        action="append",
        type=parse_pair,
        required=True,
        metavar="DISPLAY,METER",
        help="reading pair in volts; repeat for multiple measurements",
    )
    parser.add_argument(
        "--current-factor",
        type=float,
        default=1.0,
        help="BATT_CALIBRATION_FACTOR currently in firmware (default: 1.0)",
    )
    args = parser.parse_args()

    try:
        correction = calculate_scale_factor(args.pair)
        updated_factor = calculate_updated_factor(args.pair, args.current_factor)
    except ValueError as exc:
        parser.error(str(exc))

    print(f"Measurement pairs: {len(args.pair)}")
    print(f"Correction multiplier for current readings: {correction:.8f}")
    print(f"Current firmware calibration factor:       {args.current_factor:.8f}")
    print(f"Recommended BATT_CALIBRATION_FACTOR:        {updated_factor:.8f}")
    print()
    print("Paste this value into battery_monitor.cpp:")
    print(f"static const float BATT_CALIBRATION_FACTOR = {updated_factor:.8f}f;")
    print()
    print("Per-reading comparison after applying the correction:")
    for index, (controller_v, multimeter_v) in enumerate(args.pair, start=1):
        corrected_v = controller_v * correction
        error_v = corrected_v - multimeter_v
        error_pct = error_v / multimeter_v * 100.0
        print(
            f"  {index}: display={controller_v:.3f} V, meter={multimeter_v:.3f} V, "
            f"corrected={corrected_v:.3f} V, residual={error_v:+.3f} V ({error_pct:+.2f}%)"
        )

    print()
    print(
        "Note: this corrects a multiplicative scale error. If residuals vary a lot "
        "between readings, inspect the divider, wiring, ADC range, and measurement method."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
