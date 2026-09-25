# BUILD / UI RECOVERY REPORT — QEAPP Studio

**Baseline commit:** `bed7c0aefdefed43591b7a9938b1ad2b569e946e`  
**Workspace:** `D:\Program\arduino\legacy-32-classic-E524546\QEAPP-Studio`  
**Host:** Windows 10 (19045) · Python 3.12.13 · PySide6 6.11.2 (project `.venv`, offscreen)  
**Scope:** Studio-only. Firmware / trust key / GPIO / Retro-Go UI **not** modified.

---

## 1. Lỗi tái hiện từ video

| ID | Hiện tượng | Phân loại | Nguyên nhân gốc | Sửa |
| --- | --- | --- | --- | --- |
| BUG-01 | Build → `Select local VQEAF-OS source first (not the Studio folder).` | P0 Build config | `_firmware()` chỉ đọc `config.firmware_root`, không discovery env/sibling, không phân biệt Studio vs OS, dialog chỉ `OK` | `tools/studio_firmware.py` resolve ưu tiên + `inspect_firmware_root`; Recovery dialog `Chọn VQEAF-OS / Mở cài đặt / Hủy / Thử lại` |
| BUG-02 | OUTPUT vẫn `Virtual phone started` sau Build fail → dễ hiểu nhầm | P1 Status mix | Status bar / log không gắn operation ID | `_set_operation_status()` — `BUILD FAILED` / `HOST RUNNING` / `BUILD PASSED` theo `BUILD_SIGNED_QEAPP` / `RUN_HOST_LUA` / `VALIDATE_PROJECT` |
| BUG-03 | `T9 guest API not emulated` | P2 Feature limit | Thông báo giống crash | Ghi `[HOST_LIMIT] … not a crash` trong Virtual Phone log |
| BUG-04 | Không Settings Build/Firmware | P1 UX | Thiếu UI cấu hình | `Settings — Build / Firmware…` (Browse / Verify / Save) |
| BUG-05 | Lua stock vẫn có thể thử Build signed | P1 Contract | Không preflight Lua beta | `build_preflight()` → `LUA_BETA_REQUIRED`; **không** chặn Host preview |

---

## 2. Thay đổi source

| File | Nội dung |
| --- | --- |
| `studio/core/errors.py` | **NEW** Error catalog `OS_ROOT_*`, `LUA_BETA_REQUIRED`, `PROJECT_INVALID`, `SIGNER_*`, `VM_CRASH`, `QT_PLUGIN_MISSING`, `HOST_LIMIT`, … + `StudioError.ui_message()` |
| `tools/studio_firmware.py` | Priority env → settings → sibling/embedded; `inspect_firmware_root()`; từ chối Studio root; path Unicode/space; required lỗi rõ code |
| `studio/gui/firmware_settings.py` | **NEW** Settings dialog + Recovery dialog |
| `studio/gui/window.py` | Build preflight, recovery, operation status, Settings/Export menus |
| `studio/gui/virtual_phone.py` | T9 = `HOST_LIMIT` (không crash) |
| `tools/studio_doctor.py` | `--json` environment, `export_bundle()` redact secrets |
| `tests/test_separate_repo.py` | Cập nhật theo API `FirmwareRootError` + inspect |
| `studio/tests/test_build_recovery.py` | **NEW** Ma trận lỗi / separation / doctor bundle |
| `studio/tests/test_firmware_dialog_qt.py` | **NEW** Qt offscreen Settings/Recovery |

UI layout (activity rail, Explorer, editor, OUTPUT/PROBLEMS, AI AGENT, VIRTUAL PHONE) **giữ nguyên**.

---

## 3. Ưu tiên firmware root

1. `QEAPP_FIRMWARE_ROOT`  
2. Settings `firmware_root` (JSON user-local)  
3. `../VQEAF-OS` cạnh Studio  
4. `firmware/VQEAF-OS` (monorepo)

Checkout hợp lệ = có `platformio.ini` **và** `tools/build_qeapp.py`.  
`ready_for_lua_build` cần thêm `src/lua/QeLuaRuntime.cpp`.  
Chọn nhầm Studio → `OS_ROOT_IS_STUDIO`.

---

## 4. Kết quả kiểm thử

```text
python -m unittest discover -s studio/tests     → Ran 138  OK (skipped=11)
python -m unittest discover -s tests            → Ran 51   OK (skipped=17)
python -m unittest studio.tests.test_build_recovery  → Ran 18 OK
.venv Qt offscreen: test_firmware_dialog_qt + test_new_project_dialog_qt → Ran 5 OK
python tools/studio_doctor.py --json            → exit 0 (env + checks)
python tools/qstudio.py init --template lua-standard … + validate → VALID
python -m compileall -q studio tools            → exit 0
```

### Skipped / NOT_RUN (có lý do)

| Gate | Trạng thái | Lý do |
| --- | --- | --- |
| GUI offscreen StudioWindow suite | SKIPPED (11) | `MIMO_PYTHON` không có PySide6; chạy trên `.venv` cho dialog tests |
| Host Lua VM / C++ engine tests | SKIPPED | Thiếu Lua host binary / C++17 toolchain trong runner |
| Signer E2E `.qeapp` | SKIPPED | `QEAPP_FIRMWARE_ROOT` signer fixture không bật trong suite |
| Windows native GUI 1366/1920/2560 + DPI 100–175% | **NOT_RUN** | Không có phiên Windows GUI native trong phiên này |
| Screenshot baseline diff | **NOT_RUN** | Chưa có baseline PNG từ GUI native |
| Flash / board ESP32 | **NOT_RUN** | Ngoài phạm vi Studio; không tự flash |

---

## 5. Ma trận yêu cầu (PROMPT §5)

| # | Kịch bản | Kết quả |
| --- | --- | --- |
| 1 | Lua project, không OS → Validate/Run Host; Build `OS_ROOT_MISSING` | PASS (code + recovery dialog; Host không gọi `_firmware`) |
| 2 | Chọn nhầm Studio / thiếu signer / path Unicode-space | PASS (`OS_ROOT_IS_STUDIO`, `SIGNER_MISSING`, path test) |
| 3 | Text/web build có key; icon/URL sai bị từ chối | PARTIAL — validate path có sẵn; signer E2E SKIPPED (no key fixture) |
| 4 | Lua + stock firmware → Host OK; Build `LUA_BETA_REQUIRED` | PASS (preflight) |
| 5 | Build lặp / Stop / đổi project / đóng GUI | PARTIAL — job queue giữ nguyên; GUI close-orphan **NOT_RUN** native |
| 6 | New Project cấu trúc chuẩn | PASS (`test_new_project_scaffold` + Qt dialog) |
| 7 | Launcher `--diagnose` / `--gui-check` | PARTIAL — `studio_doctor --json` PASS; launcher GUI native **NOT_RUN** |

---

## 6. Error codes trong UI

Mỗi lỗi Build/VM hiện `[CODE]` + mô tả tiếng Việt + bước khắc phục + đường xem log (`logs/`), PROBLEMS có operation ID.  
Status cuối **không** trộn: `BUILD FAILED` ≠ `HOST RUNNING`.

---

## 7. Rollback

```text
git checkout -- studio/gui/window.py studio/gui/virtual_phone.py tools/studio_firmware.py tools/studio_doctor.py tests/test_separate_repo.py
rm studio/core/errors.py studio/gui/firmware_settings.py studio/tests/test_build_recovery.py studio/tests/test_firmware_dialog_qt.py
```

Không commit/push trong báo cáo này (working tree giữ thay đổi người dùng). Message gợi ý:

`fix(studio): recover firmware-root configuration and add UI regression gates`
