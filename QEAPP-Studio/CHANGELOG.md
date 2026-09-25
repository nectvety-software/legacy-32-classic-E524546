## v0.5.0 — Lua Pixel Sprite IDE / bounded runtime (host validated)

- New `engine.blit1`: 1-bit sprite mask row-major/MSB-first 1..32 px; clip to 240×270 game area, transparent pixels and 512 draw calls/frame preflight; mirrored source host & firmware beta.
- New `tools/pixel_sprite.py`: bounded PNG→inline Lua packed mask converter, alpha/dark/light threshold, Pillow 10–12 compatible. Assets remain design-time; no runtime file/SD permissions.
- New `projects/lua-sprite`, CLI `init --template lua-sprite`, GUI New Sprite Game and Insert Pixel Sprite PNG.
- New safe .png Explorer preview, with guarded path, max 1 MiB and dimensions ≤512; the binary editor remains inaccessible.
- Added `tests/test_sprite_v05.py`, `studio/tests/test_png_preview.py`, repeatable `tools/verify_v05.py` and detailed workflow documentation.
- Verification covers host Python/C++ VM, actual signer regression and host-stub firmware beta linker. No real PlatformIO build/board screenshot; PySide6 not installed in release host, GUI remains unverified until Windows/offscreen tests.

# Changelog

## v0.2 — M1 portable host engine

- Implement C++17 `qe::Runtime`, `qe::App` lifecycle, fixed-step timer cap, 32-event input queue, reserved OS keys.
- Implement clipped RGB565 draw, bounded sprite lookup and minimal 3x5 text.
- Implement PC HostCanvas 240x320, two demos `Pixel Snake`/`Hello`, deterministic replay and PNG screenshot exporter (Python stdlib).
- Extend `qstudio simulate`; retain strict QEAPP/2 `web/text` signed package workflow and reject `lua-proposal`.
- Add C++/Python tests, 5 golden screenshot hashes, simulated 1.200-step soak and optional actual firmware signer regression.
- Add `docs/07_M1_HOST_ENGINE.md` and M2 GUI implementation plan, update PROMPT.md / SKILLS.md.
- Scope: HOST ONLY. No firmware integration, independent native or Lua `.qeapp` packaging, or hardware test.

## v0.1 — QEAPP Studio Developer Blueprint (documentation + M0 CLI)

- Định nghĩa PROMPT.md, SKILLS.md, quy trình 10 bước, package contract QEAPP/2 và roadmap runtime Lua/QEAPP format mới.
- Tạo project templates `text-notes`/`web-bookmark` (buildable with firmware signer) và `snake-lua-proposal` (explicitly unbuildable).
- M0 `tools/qstudio.py` hỗ trợ doctor/validate/build/inspect; verify chữ ký opt-in với public PEM, không ghi private key.
- Bộ unittest kiểm tra schema, path traversal, HTTPS policy, missing Lua runtime, temporary-key signer end-to-end và tampering.
- Chưa thêm IDE GUI hoặc Lua runtime, chưa build firmware hay nạp ESP32-S3.

## v0.3 — M2 Desktop IDE core alpha (2026-09-25)

- NEW `studio/core/workspace.py`: bounded sandboxed UTF-8 source editing, symlink/key/build path deny rules, atomic save with conflict detection and safe mkdir.
- NEW `studio/core/jobs.py`: streaming subprocess runner; cancellation for process trees on Windows/POSIX, timeout, log secret-path masking and concurrent-job guard.
- NEW `studio/core/config.py`, `studio/core/commands.py`: user-level settings whitelist without signing credentials; typed wrappers for original qstudio CLI.
- NEW `studio/gui/window.py`: PySide6 project Explorer, editor, F6 validate, F7 signed text/web build, inspect, host Snake/Hello preview, output panel, Stop and firmware selector.
- NEW `run_studio.py`, `run_studio.bat`, `requirements-studio.txt`, unit and optional offscreen GUI tests.
- M1 C++17 engine, QEAPP2 signer flow, golden reference tests and M0 project templates preserved unchanged.
- LIMITATION: PySide6 is not installed in release build environment; offscreen tests SKIPPED. Host C++ tests run, but firmware build/device runtime not tested.
