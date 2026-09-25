# VQEAF OS v2.1 verification and board-test checklist

**Source**: the provided `SymbianS3_OS_v2.0.1_board_test_kit(1).zip`, changed as a self-contained `VQEAF-OS/` project. No writes have been made to your remote GitHub repositories or to your actual hardware.

## Run on developer PC

Install Python 3, PlatformIO and compatible board toolchains on your Windows development host. This execution environment could run GNU C++ host compilers and synthetic stubs **but could not fetch/install PlatformIO**. Full firmware image, actual screen appearance and physical board behavior remain unverified here.

```powershell
cd VQEAF-OS
python tools/test_v21.py
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

`test_v21.py` includes the host-only all-unit compile/link, a standalone native `.vqeaf` Theme Studio-compatible test (large embedded asset + short/8-digit colors), runtime launcher raster bounds/partial redraw/icon/popup checks, keypad Game/T9 tests, signed QEAPP/2 cryptographic regression and retained WiFi/board diagnostics tests. Some old historical structural tests intentionally assert the previous 3×4 S60 grid and old version strings; they are superseded rather than being silently rewritten.

## Manual hardware checks

1. Disconnect peripheral power, back up the original SD and preserve the supplied v2.0.1 firmware package to allow rollback. Build + flash the new env. No GPIO, SDMMC or I2S pin changes were made.
2. Boot normally: the splash says **VQEAF OS** and next screen is the dark 240×320 **Home launcher**, not the green grid. Use LEFT/RIGHT through six tabs; UP/DOWN navigate items and scrollbar; START opens the actual built-in service.
3. Press OPTION: a centered custom launcher dialog appears. Press A to close, B for a selected item detail. MENU from any noneditor app returns Home. For recovery, hold **DOWN** (or the older A chord) while powering on and verify the Recovery screen opens.
4. Open WiFi/BLE/Files/Music/Gallery/Qeafbrowser to ensure the existing functionality still works after return to the new launcher. NTP clock should display `--:--` until synchronized; battery outline never pretends to show an unknown charge percentage.
5. On a FAT SD card copy `sd/Themes/vqeaf_night.vqeaf` and `sd/Themes/vqeaf_day.vqeaf` to `/System/Themes/`. Apply both, revisit launcher and confirm colors are propagated. Export a simple third palette directly from VQEAF Theme Studio and import it. Test missing SD and corrupt `.vqeaf`; launcher should fall back safely.
6. Provision your publisher key **outside the firmware repository**, rebuild with the resulting public key, sign a test `.qeapp` text/web package and copy to `/System/Apps/Inbox/`. Install via App installer, visit Applications tab, check signed app's 32×32 icon appears in preview, launch, return via MENU. Invalid signature must be rejected.
7. Open URL entry; hold SELECT >650ms to enable T9 and type a word; switch back and confirm directional controls work. Watch `pio device monitor -b 115200` for errors while navigating and after removing/reinserting SD (stop audio before removal tests).
8. Before sharing a firmware BIN, inspect TLS/cert validity and storage hotplug diagnostics in `docs/BOARD_TEST_V201.md`. No actual bench FPS or runtime heap numbers are claimed by these host tests.

## Limitations

- New launcher visually modernized, but **inner app pages remain largely the v2.0.1 code with new palette colors**; they are not fully reflowed as Retro-Go UI.
- Image/resource/animation effects in Studio `.vqeaf` are retained only in the file, not rendered on the MCU.
- QEAPP/2 is *not* an interpreter for arbitrary Lua/native app payloads.
- The older signed demo `.qeapp` binaries were intentionally kept byte-identical to maintain their signatures; loose `welcome.txt` was rebranded. Regenerate/sign demo packages with **your own** key before product release.
- Raw host screenshots/mockups are not proof of on-device display quality or performance.
