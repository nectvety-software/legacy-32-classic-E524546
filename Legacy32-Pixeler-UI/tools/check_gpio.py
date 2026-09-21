from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOARD_CONFIG = ROOT / "src" / "config" / "board_config.h"

EXPECTED = {
    "BOARD_TFT_BL": 39,
    "BOARD_TFT_DC": 47,
    "BOARD_TFT_CS": 14,
    "BOARD_TFT_SCLK": 48,
    "BOARD_TFT_MOSI": 12,
    "BOARD_TFT_RST": 3,
    "BOARD_KEY_UP": 7,
    "BOARD_KEY_DOWN": 46,
    "BOARD_KEY_LEFT": 45,
    "BOARD_KEY_RIGHT": 6,
    "BOARD_KEY_MENU": 18,
    "BOARD_KEY_OPTION": 8,
    "BOARD_KEY_SELECT": 16,
    "BOARD_KEY_START": 17,
    "BOARD_KEY_A": 15,
    "BOARD_KEY_B": 5,
    "BOARD_SD_CD_DAT3": 10,
    "BOARD_SD_CMD": 11,
    "BOARD_SD_CLK": 13,
    "BOARD_SD_DAT0": 9,
}


def parse_numeric_defines(text: str) -> dict[str, int]:
    found: dict[str, int] = {}
    pattern = re.compile(r"^\s*#define\s+([A-Z0-9_]+)\s+(-?\d+)\s*$", re.MULTILINE)
    for name, value in pattern.findall(text):
        found[name] = int(value)
    return found


def main() -> int:
    if not BOARD_CONFIG.exists():
        print(f"[LOI] Khong tim thay: {BOARD_CONFIG}")
        return 1

    defines = parse_numeric_defines(BOARD_CONFIG.read_text(encoding="utf-8"))
    errors: list[str] = []

    for name, expected in EXPECTED.items():
        actual = defines.get(name)
        if actual is None:
            errors.append(f"Thieu {name}")
        elif actual != expected:
            errors.append(f"{name}: dang la GPIO{actual}, phai la GPIO{expected}")

    reverse: dict[int, list[str]] = {}
    for name, value in EXPECTED.items():
        reverse.setdefault(value, []).append(name)
    duplicates = {gpio: names for gpio, names in reverse.items() if len(names) > 1}
    if duplicates:
        for gpio, names in duplicates.items():
            errors.append(f"GPIO{gpio} bi trung trong cau hinh mong doi: {', '.join(names)}")

    if errors:
        print("[LOI] Kiem tra GPIO that bai:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print("[OK] Tat ca GPIO Legacy-32-Classic dung nhu cau hinh.")
    print(f"[OK] Da kiem tra {len(EXPECTED)} tin hieu, khong co pin trung.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
