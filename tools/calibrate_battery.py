#!/usr/bin/env python3
"""Рассчитывает коэффициент калибровки напряжения батареи по показаниям мультиметра.

Передай одну или несколько пар через --pair ПОКАЗАНИЕ_КОНТРОЛЛЕРА,ПОКАЗАНИЕ_МУЛЬТИМЕТРА.
Значение контроллера должно совпадать с напряжением, которое сейчас показывает прошивка.
Скрипт вычисляет мультипликативную поправку через начало координат, подходящую для
исправления ошибки масштаба. Она не устраняет нелинейность АЦП или ошибку смещения.
"""

from __future__ import annotations

import argparse
import math
from typing import Sequence

Reading = tuple[float, float]


def parse_pair(value: str) -> Reading:
    """Разбирает одну пару ПОКАЗАНИЕ_КОНТРОЛЛЕРА,ПОКАЗАНИЕ_МУЛЬТИМЕТРА."""
    parts = value.split(",")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(
            "ожидается ПОКАЗАНИЕ_КОНТРОЛЛЕРА,ПОКАЗАНИЕ_МУЛЬТИМЕТРА, например 7.82,8.04"
        )

    try:
        controller_v, multimeter_v = (float(part.strip()) for part in parts)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("оба показания должны быть числами") from exc

    if not all(math.isfinite(v) and v > 0 for v in (controller_v, multimeter_v)):
        raise argparse.ArgumentTypeError("оба показания должны быть конечными значениями выше 0 В")

    return controller_v, multimeter_v


def calculate_scale_factor(readings: Sequence[Reading]) -> float:
    """Находит методом наименьших квадратов коэффициент перевода показаний контроллера в показания мультиметра."""
    if not readings:
        raise ValueError("требуется хотя бы одна пара показаний")

    denominator = sum(controller_v * controller_v for controller_v, _ in readings)
    if denominator <= 0:
        raise ValueError("показания контроллера должны быть выше 0 В")

    numerator = sum(controller_v * multimeter_v for controller_v, multimeter_v in readings)
    factor = numerator / denominator

    if not math.isfinite(factor) or factor <= 0:
        raise ValueError("вычисленный коэффициент масштаба некорректен")

    return factor


def calculate_updated_factor(
    readings: Sequence[Reading], current_factor: float = 1.0
) -> float:
    """Возвращает новый коэффициент прошивки на основе уже установленного значения."""
    if not math.isfinite(current_factor) or current_factor <= 0:
        raise ValueError("текущий коэффициент калибровки должен быть конечным и выше 0")
    return current_factor * calculate_scale_factor(readings)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Рассчитывает BATT_CALIBRATION_FACTOR по парам напряжений. "
            "Каждый --pair задаёт ПОКАЗАНИЕ_КОНТРОЛЛЕРА,ЭТАЛОН_МУЛЬТИМЕТРА."
        )
    )
    parser.add_argument(
        "--pair",
        action="append",
        type=parse_pair,
        required=True,
        metavar="DISPLAY,METER",
        help="пара показаний в вольтах; повторяй параметр для нескольких измерений",
    )
    parser.add_argument(
        "--current-factor",
        type=float,
        default=1.0,
        help="текущее значение BATT_CALIBRATION_FACTOR в прошивке (по умолчанию: 1.0)",
    )
    args = parser.parse_args()

    try:
        correction = calculate_scale_factor(args.pair)
        updated_factor = calculate_updated_factor(args.pair, args.current_factor)
    except ValueError as exc:
        parser.error(str(exc))

    print(f"Пар измерений: {len(args.pair)}")
    print(f"Поправочный коэффициент для текущих показаний: {correction:.8f}")
    print(f"Текущий коэффициент калибровки прошивки: {args.current_factor:.8f}")
    print(f"Рекомендуемый BATT_CALIBRATION_FACTOR:     {updated_factor:.8f}")
    print()
    print("Вставь это значение в battery_monitor.cpp:")
    print(f"static const float BATT_CALIBRATION_FACTOR = {updated_factor:.8f}f;")
    print()
    print("Сравнение каждого измерения после коррекции:")
    for index, (controller_v, multimeter_v) in enumerate(args.pair, start=1):
        corrected_v = controller_v * correction
        error_v = corrected_v - multimeter_v
        error_pct = error_v / multimeter_v * 100.0
        print(
            f"  {index}: на дисплее={controller_v:.3f} V, мультиметр={multimeter_v:.3f} V, "
            f"после коррекции={corrected_v:.3f} V, остаток={error_v:+.3f} V ({error_pct:+.2f}%)"
        )

    print()
    print(
        "Примечание: это исправляет мультипликативную ошибку масштаба. Если остатки сильно различаются "
        "между измерениями, проверь делитель напряжения, проводку, диапазон АЦП и метод измерения."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
