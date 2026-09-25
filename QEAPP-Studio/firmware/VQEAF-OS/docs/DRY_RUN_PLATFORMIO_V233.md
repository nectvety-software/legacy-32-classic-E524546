# VQEAF OS v2.3.3 — Offline Builder: chế độ dry-run

**Phạm vi:** bổ sung `--dry-run` cho `tools/build_offline.py` và các launcher Windows/Unix. Mục tiêu là kiểm tra luồng xây dựng firmware ESP32-S3-WROOM-1 N16R8, phân tích thiếu cache hoặc lỗi giả lập và thử hệ thống log **không cần PlatformIO, g++, Xtensa toolchain, PSRAM hay mạch thật**. Chỉ yêu cầu Python 3 (nên dùng Python 3.8+).

> Kết quả `DRY_RUN_COMPLETE` **không có nghĩa firmware build thành công**. Script tuyệt đối không tạo `firmware.bin` giả, không giả mạo hash firmware, không chạy `pio --version`, `pio run`, trình biên dịch, SCons, trình liên kết hoặc host C++ tests. Có **một bước kiểm tra thật** chạy bằng Python: `tools/check_board_config.py` đọc board JSON, PlatformIO INI, mapping GPIO của firmware và partition CSV. Nếu mã nguồn không hợp lệ, dry-run cũng sẽ fail.

## 1. Lệnh chạy

Windows CMD:

```bat
cd VQEAF-OS
tools\build_offline.bat --dry-run
rem Mô phỏng cả test PC lẫn đóng gói LittleFS (không yêu cầu data/ thật)
tools\build_offline.bat --dry-run --host-tests --buildfs
rem Chỉ kiểm tra cấu hình board và mô phỏng kiểm tra cache
tools\build_offline.bat --dry-run --check-only
```

PowerShell:

```powershell
cd VQEAF-OS
.\tools\build_offline.ps1 -DryRun -HostTests -BuildFS
.\tools\build_offline.ps1 -DryRun -SimulateFailure linker
```

Linux/macOS (hoặc Python trực tiếp trên Windows):

```bash
python3 tools/build_offline.py --dry-run --buildfs --host-tests
python3 tools/build_offline.py --dry-run --report-dir ./my_dry_reports
```

`--incremental`: mô phỏng bỏ qua `clean` giống chế độ build tăng dần. `--check-only`: dừng sau các bước board + cache; không sinh log `build.log`. `--pio <path>` và `--pio-home <path>` **không** dẫn đến thực thi PIO trong dry-run; `--pio-home` chỉ dùng để chụp trạng thái filesystem (nếu có). `--report-dir` dùng để đổi thư mục báo cáo.

**Lưu ý:** `--buildfs` mô phỏng khả năng tạo LittleFS ngay cả khi chưa có `data/`; `observed_cache.data_folder_present` phản ánh trạng thái thật. Ở chế độ build thực, thiếu `data/` hoặc `tool-mklittlefs` sẽ bị chặn bởi preflight.

## 2. Những gì chạy thật và mô phỏng

| Bước | Dry-run thực hiện | Thực thi PIO / toolchain? |
|---|---|---|
| Board + GPIO + PSRAM config + partitions | **Kiểm tra tĩnh THẬT** bằng Python | Không |
| Quan sát thư mục cache | Chỉ đọc trạng thái hiện tại (không yêu cầu phải có) | Không |
| Cache version/pinned dependencies | **Mô phỏng** quy trình xác minh; không tuyên bố cache hợp lệ | Không |
| `pio --version` | Ghi lệnh dự kiến | Không |
| Host C++ test (`--host-tests`) | Ghi lệnh dự kiến | Không |
| Clean | Mô phỏng | Không |
| Compile / Link / `pio run -e vqeaf_os -v` | Mô phỏng từng bước, ghi log riêng | Không |
| Kiểm tra kích thước/tính mới/SHA-256 của `.bin` | Mô phỏng; **không tính hash giả** | Không |
| `buildfs` (`--buildfs`) | Mô phỏng; không tạo filesystem image | Không |
| Báo cáo Markdown + JSON | **Sinh ra thật** và phân loại lỗi | Không |

## 3. Log và báo cáo

Mặc định dry-run dùng `build_reports/offline/dry_run/`, **tách khỏi báo cáo offline build thật** tại `build_reports/offline/`.

```text
build_reports/offline/dry_run/
├── board_preflight.log        # kiểm tra board thật (Python)
├── preflight.log              # thông tin khởi động và kết quả quan sát cache
├── preflight_simulated.log    # preflight PIO mô phỏng
├── platformio_version.log     # giả lập truy vấn phiên bản
├── steps.log                  # timeline từng bước
├── dry_run_plan.log           # các lệnh dự kiến; tất cả gắn nhãn NOT EXECUTED
├── clean.log                  # không có nếu --incremental / --check-only
├── compile.log                # mô phỏng compile
├── link.log                   # mô phỏng link nếu compile không lỗi
├── build.log                  # tóm tắt pio run mô phỏng
├── artifact_check.log         # KHÔNG kiểm tra file nhị phân thật
├── host_cpp.log               # chỉ nếu --host-tests
├── buildfs.log                # chỉ nếu --buildfs
├── report.json                # cho CI/tooling
└── report.md                  # tiếng Việt, dễ đọc
```

`report.json` có `dry_run=true`, `mode=dry_run_simulation`, `execution_policy`, `observed_cache`, `planned_commands`, `simulated_steps` (mỗi bước `executed=false`), `diagnostics`, `would_generate`, `firmware_created=false`, và **`artifacts=[]`**. Cột `board_preflight=PASS_STATIC` là kiểm tra tĩnh **thật**; `dependency_cache=SIMULATED_PASS_NOT_VERIFIED` không bảo đảm có cache thật.

## 4. Mô phỏng lỗi có kiểm soát

Kích hoạt bằng `--dry-run --simulate-failure <case>`. Không sửa mã nguồn thật và không nạp firmware:

| Case | Điểm dừng | Gợi ý kiểm tra |
|---|---|---|
| `board` | sau khi preflight tĩnh PASS | Kiểm tra cảnh báo GPIO giả lập |
| `platformio` | dependency cache | Không tìm thấy PIO CLI |
| `toolchain` | dependency cache | Thiếu Xtensa ESP32-S3 |
| `library` | dependency cache | Thiếu thư viện pinned |
| `clean` | clean | Không xóa được output tạm |
| `compiler` | compile | Cú pháp C++ / compiler error |
| `linker` | link | `undefined reference` |
| `network` | dependency resolution | Kết nối bị từ chối bởi proxy |
| `missing-bin` | xác minh artifact | PIO trả exit 0 nhưng không có firmware |
| `buildfs` | filesystem image | LittleFS build thất bại; cần `--buildfs` |

Ví dụ:

```bat
tools\build_offline.bat --dry-run --simulate-failure compiler
tools\build_offline.bat --dry-run --simulate-failure linker
tools\build_offline.bat --dry-run --simulate-failure missing-bin
tools\build_offline.bat --dry-run --buildfs --simulate-failure buildfs
```

`--check-only` chỉ hỗ trợ lỗi `board`, `platformio`, `toolchain`, `library`. `clean` không tương thích `--incremental`. Trong dry-run, lỗi **giả lập** đều có nhãn `[SIMULATED]`; lỗi cấu hình nguồn thực luôn có `BOARD_CONFIG` với `simulated=false`.

| Exit | Status | Ý nghĩa |
|---:|---|---|
| `0` | `DRY_RUN_COMPLETE` | Quy trình mô phỏng hoàn thành, KHÔNG build |
| `0` | `DRY_RUN_CHECK_ONLY` | Chỉ thực hiện kiểm tra board tĩnh và mô phỏng preflight |
| `1` | `DRY_RUN_SIMULATED_FAILURE` | Lỗi giả lập ở clean/compile/link/artifact/buildfs |
| `2` | `DRY_RUN_SIMULATED_FAILURE` | Lỗi giả lập ở board hoặc preflight |
| `2` | `DRY_RUN_STATIC_BOARD_FAILURE` | **Lỗi tĩnh thật** từ mã nguồn hiện tại |
| `3` | `DRY_RUN_*` / `internal_error.log` | Script có lỗi nội bộ |

## 5. Kiểm thử tự động

```bash
python3 tools/test_offline_build.py
```

Bộ test kiểm tra chính chế độ build offline và dry-run bằng một dự án tạm, CLI PIO giả; riêng dry-run **theo dõi và khẳng định PIO giả chưa từng bị gọi**. Bao gồm thiếu cache, không tạo `.bin`, mô phỏng lỗi compiler/linker và phát hiện GPIO thực bị đổi trong fixture.

Muốn build thật: chuẩn bị PIO, ESP32-S3 toolchain và pinned libs đầy đủ, chạy `tools/build_offline.bat` **không có** `--dry-run`, rồi kiểm tra firmware và nạp mạch thật. Không dùng báo cáo mô phỏng làm bằng chứng firmware chạy được trên thiết bị.
