# SKILLS.md — Quy trình AI Agent triển khai Standard Lua Project cho QEAPP-Studio

> Đi kèm [PROMPT.md](PROMPT.md). Thực hiện theo thứ tự; yêu cầu **thử nghiệm thực**, không tự ghi PASS khi chỉ viết code hoặc lập kế hoạch. Đây là skill chuyên đề **APP_TEMPLATE + STUDIO_GUI + QA**; không thay thế `SKILLS.md`/`AGENTS.md` hiện tại ở gốc repository.

## Skill 1 — Khảo sát, khóa baseline và xác định trách nhiệm

**Đầu vào:** checkout `qeafivels/QEAPP-Studio` đúng nhánh, cấu trúc đang tồn tại, khả năng Python, Qt/Lua host, và checkout OS chỉ khi cần build.

1. Đọc `AGENTS.md` → `PROMPT.md` → `SKILLS.md` ở gốc, sau đó prompt chuyên đề này.
2. Thu thập `git status --short`, `git rev-parse HEAD`, danh sách template thực ở `projects/` và CLI thực ở `tools/qstudio.py`.
3. Đọc `studio/gui/window.py::create_project`, `studio/core/workspace.py`, các bài test init/validate, `projects/lua-hello/` và `samples/`.
4. Lập Task Brief: **owner APP_TEMPLATE** cho generator/templates/CLI; **owner STUDIO_GUI** cho menu/wizard; **owner QA** cho negative tests và báo cáo. Khóa tệp `tools/qstudio.py` và `studio/gui/window.py` tránh hai Agent cùng sửa.
5. Ghi baseline test và những công cụ không có (`Python`, `Pillow`, `PySide6`, Lua host, PlatformIO, thiết bị), không đoán.

**Đầu ra:** danh sách file được phép sửa, ma trận kiểm thử và baseline SHA/commit.

## Skill 2 — Sinh cấu trúc mặc định từ một nguồn duy nhất

**Thiết kế:** generator dùng chung được `tools/qstudio.py init --template lua` và GUI New Lua App gọi. Có thể giữ template dưới `projects/lua-standard/` hoặc dùng một module Python riêng; tránh trùng lặp nội dung manifest/skeleton.

**Danh mục bắt buộc:**

```text
MyApplication/
├── qeapp.project.json
├── main.lua
├── README.md
├── CHANGELOG.md
├── assets/
│   ├── icon.png                    # 32×32 PNG hợp lệ
│   ├── sprites/.gitkeep
│   ├── fonts/.gitkeep
│   └── sounds/.gitkeep
├── tests/
│   ├── input_replay.json
│   └── test_cases.json
└── docs/DEVELOPMENT.md
```

**Cách thực hiện:**

1. Xác thực `id`, `name`, tên thư mục trước khi tạo. Từ chối đường dẫn tương đối nguy hiểm/đích đã tồn tại.
2. Sinh vào staging sibling `.<name>.tmp-<random>`; tạo tệp bằng UTF-8 và một icon PNG thật (Pillow đã là dependency cho validate PNG). Không gọi công cụ mạng trong template generator.
3. Điền metadata manifest chỉ bằng 7 khóa schema hỗ trợ: `project_format,id,name,version,type,content,icon` (không có `url` với Lua). Giữ tên dự án printable ASCII.
4. Dùng `qstudio.validate()` để xác nhận các nội dung và mọi đường dẫn trước khi xuất bản thư mục.
5. Chuyển staging sang đích an toàn; khi có lỗi, xóa riêng staging mình tạo. Không sửa/xóa thư mục user có sẵn. Lưu ý thao tác rename chỉ nguyên tử trong phạm vi filesystem phù hợp.
6. Đảm bảo `tests/input_replay.json` có ít nhất sự kiện nhấn/nhả `start`, đúng format; `test_cases.json` được ghi rõ là **dữ liệu tài liệu** nếu runner chưa hỗ trợ.

**Không làm:** không thêm `require`, `dofile`, đọc SD/OS API không có trong Lua host; không tự thêm khóa `assets` hoặc `permissions` vào manifest. Không đưa `build`, `dist`, secret, PEM vào template.

**Điều kiện PASS:** tạo hai project với ID hợp lệ khác nhau; cấu trúc giống nhau, manifest chỉ khác `id/name`; cả hai validate thành công, icon 32×32.

## Skill 3 — Tích hợp GUI New Project và Explorer

1. Cập nhật hành vi mục New Lua App (Beta) để gọi generator dùng chung. Có thể hiển thị tên **Standard Lua Application (Beta)**; giữ các lựa chọn Text, Web, Snake, Sprite hiện có.
2. Form nhập tên ASCII cho manifest, `id` hợp lệ, thư mục đích, cảnh báo **Lua beta**; mặc định `version=1.0.0`.
3. Nếu tạo thành công: mở project và làm mới Explorer; tab đầu tiên mở `main.lua`; đảm bảo `assets/icon.png` preview qua cơ chế PNG an toàn, không cố mở dưới dạng UTF-8.
4. Nếu lỗi tên/đích/permissions: hiển thị thông báo hữu ích, không tạo project nửa chừng và không làm mất project hiện đang mở.
5. Tôn trọng giới hạn Workspace: editor chỉ truy cập tập đuôi văn bản cho phép; `build/`, `dist/`, symlink, private key đều bị chặn.

**Điều kiện PASS:** dùng GUI thật (Qt offscreen khi có điều kiện) tạo project, mở `main.lua`, hiển thị cấu trúc và icon; nếu không có PySide6 báo `NOT_RUN` và chỉ xác minh CLI, không tạo screenshot giả.

## Skill 4 — Host Lua, manifest và ranh giới firmware

1. Gọi `python tools/qstudio.py validate <project>`.
2. Kiểm tra `main.lua` dùng callback thực sự: `on_key`, `on_update`, `on_draw`; `on_draw` chỉ vẽ API đã có. Replay với `tests/input_replay.json` phải có phím `start` down/up hợp lệ.
3. Nếu Lua host được dựng và kiểm chứng, chạy `python tools/qstudio.py lua-preview <project> --frames 8 --replay tests/input_replay.json -o <temp-output>.png`. Chỉ coi ảnh là **kết quả mô phỏng PC**.
4. Nếu cần `.qeapp` thử nghiệm: `build` phải dùng signer thật từ checkout `QEAPP_FIRMWARE_ROOT` bên ngoài Studio, tùy chọn `--experimental-lua`, đúng firmware beta và trust key. Không gắn mặc định F7 build thành công nếu chưa có signer/key. Không yêu cầu người dùng đưa private key vào repository.
5. Trong README template: giải thích stock QEAPP hỗ trợ web/text theo firmware hiện hành; Lua cần firmware beta được xác thực riêng và chạy thiết bị thật chưa xác minh.

**Điều kiện PASS:** validation thành công; preview chỉ PASS khi Lua host chạy thật; build ký gói/device là gate riêng.

## Skill 5 — Tạo bộ test positive/negative

Thực hiện test tự động (`unittest`/pytest tùy repo), ưu tiên `tests/` và `studio/tests/` có sẵn. Mỗi test phải dùng temp directory, không ghi vào thư mục làm việc của người dùng.

| ID | Test | Kỳ vọng |
| --- | --- | --- |
| PRJ-01 | CLI tạo `lua` chuẩn | Có đầy đủ file/dir, validate PASS |
| PRJ-02 | GUI New Lua App | Output giống CLI và Explorer mở được |
| PRJ-03 | Manifest schema | Đúng 7 khóa; ID/name/version/type hợp lệ |
| PRJ-04 | Icon | PNG thật chính xác 32×32 |
| PRJ-05 | Lua + replay | UTF-8 không NUL, <=64 KiB; replay đúng format |
| PRJ-06 | Tạo vào thư mục đã có | Báo lỗi; dữ liệu cũ giữ nguyên |
| PRJ-07 | ID/tên/đường dẫn sai | Từ chối; không ghi file ra ngoài đích |
| PRJ-08 | Tạo lỗi giữa chừng | Không tồn tại project nửa chừng; staging được dọn |
| PRJ-09 | Template cũ | Text/web/snake/sprite và samples không regress |
| PRJ-10 | Lua host preview | Chạy đúng khi host có sẵn; ghi NOT_RUN nếu thiếu |
| PRJ-11 | Bảo mật/build | Không có PEM/trust key; F7 báo rõ khi thiếu firmware beta |
| PRJ-12 | Đối chiếu repo OS | Không có diff firmware/renderer/theme/GPIO |

**Lệnh kiểm thử đề xuất (chỉ dùng khi có toolchain):**

```powershell
python -m compileall -q studio tools
python -m unittest discover -s studio/tests -v
python -m unittest discover -s tests -v
python tools/qstudio.py init --template lua --id sample_app --name "Sample App" -o <temporary-output>
python tools/qstudio.py validate <temporary-output>
python tools/validate_agent_docs.py
```

Dấu `<temporary-output>` là placeholder cần thay bằng thư mục tạm **thực tế**, không sao chép nguyên lệnh. Nếu nâng cấp CLI thêm alias thì test cả `lua` lẫn `lua-standard`.

## Skill 6 — Handoff, tích hợp và commit an toàn

1. So diff với baseline và dùng `git status --short`; chỉ thay đổi phần Studio và tài liệu đúng scope.
2. Mở lại một dự án Lua cũ và hai mẫu Pocket Focus/Pocket Calculator; kiểm tra ít nhất validator và luồng preview khả dụng.
3. Ghi `PASS`, `FAIL`, `SKIPPED`, `NOT_RUN` riêng từng test; đính kèm stdout/stderr hoặc đường dẫn log. Không gọi benchmark host là FPS của ESP32-S3.
4. Điền `docs/agents/HANDOFF_TEMPLATE.md`: mục tiêu, file, commit baseline, giao diện CLI, ảnh UI được đo từ GUI thật, test results, giới hạn và rollback plan.
5. Nếu người dùng yêu cầu commit: message gợi ý `feat(studio): add standard Lua app project scaffold` kèm body mô tả CLI/GUI/template/tests. Để repo OS nguyên vẹn, không force-push hoặc ghi đè project user.
6. Nếu tài liệu chuyên đề được chép vào repo, giữ nguyên `PROMPT.md`/`SKILLS.md` ở **gốc**. Thêm liên kết từ AGENTS.md bằng PR riêng hoặc tích hợp có kiểm thử để tránh ghi đè các quy tắc chung.

**DONE** chỉ khi generator/GUI tương ứng có bằng chứng; không có môi trường Qt/Lua host hoặc thiết bị thì ghi rõ phần chưa được kiểm chứng và các bước thực hiện trên máy người dùng.
