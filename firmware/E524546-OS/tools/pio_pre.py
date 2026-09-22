# pio_pre.py — Fix portable cho pioarduino/framework-arduinoespressif32 v3.x:
# các lib framework (FS/LittleFS/SD_MMC/...) không có library.json nên LDF
# biên dịch chúng với include-path biệt lập -> LittleFS.cpp/SD_MMC.cpp báo
# "FS.h: No such file or directory". Script này thêm CPPPATH toàn cục cho
# mọi lần biên dịch (kể cả lib), tương đương hành vi của platform-espressif32 gốc.
Import("env")
import os

try:
    pkg = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    for lib in ("FS", "LittleFS", "SD_MMC", "SPI", "Wire"):
        p = os.path.join(pkg, "libraries", lib, "src")
        if os.path.isdir(p):
            env.Append(CPPPATH=[p])
    print("E524546: framework CPPPATH fix OK -> " + pkg)
except Exception as e:
    print("E524546: framework CPPPATH fix skipped: " + str(e))
