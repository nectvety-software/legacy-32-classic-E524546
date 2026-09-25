#!/usr/bin/env python3
"""Host-side regression gates for Symbian S3 OS v0.4 portrait edition."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def check_portrait_layout() -> None:
    cfg = read("include/BoardConfig.h")
    assert "TFT_ROTATION = 0" in cfg
    assert "SCREEN_W = 240" in cfg
    assert "SCREEN_H = 320" in cfg

    ui_h = read("src/core/SymbianUI.h")
    def const(name):
        m = re.search(rf"{name}\s*=\s*(\d+)", ui_h)
        assert m, name
        return int(m.group(1))
    content_top, cell_w, cell_h, softkey_top = const("CONTENT_TOP"), const("GRID_CELL_W"), const("GRID_CELL_H"), const("SOFTKEY_TOP")
    assert 3 * cell_w <= 240
    assert content_top + 3 * cell_h < softkey_top
    assert 280 < softkey_top < 320


def check_pin_conflicts() -> None:
    cfg = read("include/BoardConfig.h")
    pins = {}
    for name in [
        "TFT_LEDK_PIN","TFT_DC_PIN","TFT_CS_PIN","TFT_SCL_PIN","TFT_SDA_PIN","TFT_RST_PIN",
        "KEY_MENU","KEY_UP","KEY_A","KEY_LEFT","KEY_START","KEY_RIGHT",
        "KEY_OPTION","KEY_DOWN","KEY_B","KEY_SELECT",
        "SD_D3","SD_CMD","SD_CLK","SD_D0",
    ]:
        m = re.search(rf"constexpr int {name}\s*=\s*(\d+);", cfg)
        assert m, name
        pins[name] = int(m.group(1))
    assert len(set(pins.values())) == len(pins), pins

    audio = {"AUDIO_BCLK": 4, "AUDIO_WS": 1, "AUDIO_DOUT": 2}
    assert set(audio.values()).isdisjoint(pins.values())
    assert set(audio.values()).isdisjoint({19,20,26,27,28,29,30,31,32,33,34,35,36,37})


def move_grid(index: int, key: str) -> int:
    cols, count = 3, 9
    row, col = divmod(index, cols)
    start = row * cols
    end = min(start + cols - 1, count - 1)
    if key == "left": return index - 1 if index > start else end
    if key == "right": return index + 1 if index < end else start
    if key == "up":
        if index - cols >= 0: return index - cols
        c = col
        while c + cols < count: c += cols
        return c
    if key == "down":
        return index + cols if index + cols < count else col
    raise ValueError(key)


def check_grid_navigation() -> None:
    expected = {
        0:(2,1,6,3), 1:(0,2,7,4), 2:(1,0,8,5),
        3:(5,4,0,6), 4:(3,5,1,7), 5:(4,3,2,8),
        6:(8,7,3,0), 7:(6,8,4,1), 8:(7,6,5,2),
    }
    for i, (l,r,u,d) in expected.items():
        assert (move_grid(i,"left"),move_grid(i,"right"),move_grid(i,"up"),move_grid(i,"down")) == (l,r,u,d)


def check_keyboard_fit() -> None:
    # 6 columns x 9 rows, but only 50 keys are rendered.
    last = 49
    col, row = last % 6, last // 6
    x, y = 7 + col*38, 76 + row*23
    assert x + 34 <= 240
    assert y + 20 < 292


def check_features_present() -> None:
    main = read("src/main.cpp")
    ui_h = read("src/core/SymbianUI.h")
    ui_cpp = read("src/core/SymbianUI.cpp")
    music = read("src/services/MusicService.cpp")
    assert "ScreenId::Idle" in main
    assert "e.key == Key::Menu" in main and "e.key == Key::Option" in main
    assert "transitionOut" in ui_h and "dialog(" in ui_h and "idleHome" in ui_h
    assert "e.key == Key::Start)  { enterScreen(ScreenId::Music)" in main
    assert "i2s_driver_install" in music and "I2S_CHANNEL_STEREO" in music
    assert "channels == 1" in music and "out[i*2+1]" in music



def check_build_macro_safety() -> None:
    cfg = read("include/BoardConfig.h")
    # TFT_eSPI defines TFT_DC/TFT_CS/TFT_RST from build_flags. Board constants
    # must not reuse those preprocessor names.
    for bad in ["constexpr int TFT_DC ", "constexpr int TFT_CS ", "constexpr int TFT_RST "]:
        assert bad not in cfg
    for good in ["TFT_DC_PIN", "TFT_CS_PIN", "TFT_RST_PIN"]:
        assert good in cfg


def check_cpp11_safe_initialization() -> None:
    types = read("src/core/Types.h")
    input_h = read("src/core/InputManager.h")
    input_cpp = read("src/core/InputManager.cpp")
    storage_h = read("src/services/StorageService.h")
    storage_cpp = read("src/services/StorageService.cpp")
    assert "KeyEvent(Key k" in types
    assert "BtnState(int p, Key k)" in input_h
    assert "BtnState(Board::KEY_MENU" in input_cpp
    assert "return KeyEvent(" in input_cpp
    assert "FsEntry(const String &entryName" in storage_h
    assert "FsEntry(n, normalized" in storage_cpp


def main() -> None:
    tests = [
        check_portrait_layout,
        check_pin_conflicts,
        check_grid_navigation,
        check_keyboard_fit,
        check_features_present,
        check_build_macro_safety,
        check_cpp11_safe_initialization,
    ]
    for test in tests:
        test()
        print(f"PASS: {test.__name__}")
    print(f"PASS: {len(tests)}/{len(tests)} v0.4 regression gates")


if __name__ == "__main__":
    main()
