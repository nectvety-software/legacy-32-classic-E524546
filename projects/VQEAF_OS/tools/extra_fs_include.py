# PlatformIO extra script: SD_MMC.cpp does `#include "FS.h"`.
# LDF does not always publish the Arduino core FS library include path to
# sibling core libs (SD_MMC, LittleFS, …) on this toolchain, so add it here.
Import("env")  # type: ignore  # noqa: F821 — provided by SCons/PlatformIO

from pathlib import Path


def _fs_src_dir() -> Path:
    try:
        pkg = env.PioPlatform().get_package_dir("framework-arduinoespressif32")  # type: ignore  # noqa: F821
        if pkg:
            candidate = Path(pkg) / "libraries" / "FS" / "src"
            if (candidate / "FS.h").is_file():
                return candidate
    except Exception:
        pass
    home = Path.home() / ".platformio" / "packages"
    for root in (home, Path("C:/Users/doxuanhop/.platformio/packages")):
        candidate = root / "framework-arduinoespressif32" / "libraries" / "FS" / "src"
        if (candidate / "FS.h").is_file():
            return candidate
    return Path()


_fs = _fs_src_dir()
if _fs.is_dir():
    env.Append(CPPPATH=[str(_fs)])  # type: ignore  # noqa: F821
    print("extra_fs_include: added", _fs)
else:
    print("extra_fs_include: FS.h not found; SD_MMC may fail to compile")
