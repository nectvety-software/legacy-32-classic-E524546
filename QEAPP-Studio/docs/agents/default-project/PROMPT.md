# PROMPT.md — Chuẩn hóa cấu trúc dự án mặc định QEAPP-Studio

> **Nhiệm vụ giao cho AI Agent:** triển khai chức năng tạo **Standard Lua Application** với cấu trúc nhất quán trong QEAPP-Studio. Đây là **đặc tả triển khai**, không phải tuyên bố rằng phiên bản Studio hiện tại đã có tính năng này.
>
> **Repository đích:** `qeafivels/QEAPP-Studio` (desktop IDE, CLI, Lua host VM, tài liệu, tests). Repository `qeafivels/VQEAF-OS` chỉ dùng đọc hợp đồng QEAPP/2/khả năng firmware; không sửa firmware trừ khi người dùng giao nhiệm vụ riêng.

## 1. Vai trò và nhiệm vụ

Bạn là AI Agent phát triển QEAPP-Studio, gồm kỹ sư Python/PySide6, công cụ dự án và QA. Hãy **triển khai thực tế**, viết mã và chạy kiểm thử để mọi dự án **Lua được tạo mới** từ Studio có cấu trúc chuyên nghiệp. Tương thích với dự án hiện có, không tự thay đổi runtime, định dạng QEAPP/2, chữ ký, khóa và giao diện VQEAF-OS.

**Yêu cầu:** chế độ **New Lua App (Beta)** hiện có phải tạo cấu trúc chuẩn ở mục 2; bổ sung nhãn **Standard Lua Application (Beta)** trong UI nếu cách trình bày hiện hành cho phép. CLI `python tools/qstudio.py init --template lua ...` phải dùng cùng generator với GUI. Có thể thêm alias `lua-standard`; nếu thêm phải giữ `--template lua` tương thích. Giữ nguyên template `text`, `web`, `lua-snake`, `lua-sprite` và các dự án đã tạo trước đây.

Không được chỉ tạo thư mục trên giấy: nối đầy đủ luồng New Project → sinh tệp → mở Workspace/Explorer → F6 validate → F9 host preview khi có Lua host phù hợp; F7 build **chỉ khi** đã có firmware beta và khóa ký đúng.

## 2. Cấu trúc thư mục phải tạo

```text
<MyApplication>/
├── qeapp.project.json       # Manifest v1, tương thích tools/qstudio.py
├── main.lua                 # Entry point Lua 5.4; không require/dofile
├── README.md                # Mục đích, điều khiển, chạy/test, tương thích
├── CHANGELOG.md             # Lịch sử phiên bản
├── assets/
│   ├── icon.png             # PNG chính xác 32×32, được khai báo ở manifest
│   ├── sprites/             # Nguồn sprite phục vụ phát triển (có .gitkeep)
│   ├── fonts/               # Nguồn font phục vụ phát triển (có .gitkeep)
│   └── sounds/              # Nguồn âm thanh phục vụ phát triển (có .gitkeep)
├── tests/
│   ├── input_replay.json    # Format replay đang được Studio hỗ trợ
│   └── test_cases.json      # Chỉ là kịch bản mô tả nếu chưa có runner riêng
└── docs/
    └── DEVELOPMENT.md      # Quy ước dự án và giới hạn thiết bị
```

**Lưu ý quan trọng về assets:** Bản công cụ đóng gói hiện tại xử lý `main.lua` và *icon 32×32 tùy chọn*, không tự đóng gói thư mục `sprites/fonts/sounds`. Các thư mục đó là tài nguyên làm việc/nguồn thiết kế, không được cho ứng dụng Lua sử dụng qua `require`, `dofile` hay API đọc file không tồn tại. Nếu user muốn đóng gói ảnh/âm thanh khác, hãy lập task nâng cấp runtime và định dạng gói riêng, kiểm tra ABI và firmware trước. Thư mục output `build/`, `dist/`, `logs/`, `.venv/`, cache nằm ngoài phạm vi editor; không tự đưa vào project ban đầu.

## 3. Manifest đầu ra chính xác

Tạo `qeapp.project.json` theo schema mà `tools/qstudio.py` **thực sự hỗ trợ**; không thêm khóa metadata tùy ý.

```json
{
  "project_format": 1,
  "id": "my_application",
  "name": "My Application",
  "version": "1.0.0",
  "type": "lua",
  "content": "main.lua",
  "icon": "assets/icon.png"
}
```

- `id`: chữ thường `a-z`, số, `_` hoặc `-`, dài 1–24 ký tự; tuyệt đối không cho ký tự đường dẫn.
- `name`: ASCII in được, dài 1–40 ký tự. Nếu giao diện nhập tên tiếng Việt, giữ tên hiển thị trong tài liệu, nhưng manifest hiện tại không chấp nhận Unicode; UI phải giải thích yêu cầu này.
- `version`: dạng số phân tách bằng dấu chấm như `1.0.0`, không dài quá 19 ký tự.
- `type`: `lua`; `content` là đường dẫn POSIX tương đối đến `main.lua` UTF-8, không chứa byte NUL và không vượt quá **64 KiB**.
- `icon`: PNG **đúng 32×32**; phải có tệp thật và được `validate()` chấp nhận.
- Không dùng symlink, `..`, đường dẫn tuyệt đối hoặc ghi đè thư mục đã tồn tại.
- Với dự án `text`/`web`, tiếp tục dùng schema hiện có; không biến chúng thành gói Lua.

## 4. Skeleton Lua và bài kiểm thử mặc định

`main.lua` chạy được trên Lua host hiện hành, dùng API đúng mẫu có sẵn: `on_key(key, down)`, `on_update(dt)`, `on_draw()`, `engine.clear`, `engine.text`, `engine.rect`, `engine.width`, `engine.height`. Khởi tạo giao diện 240×320 theo diện tích máy ảo hiện có; không tự nhận là firmware OS hoặc MRE/Symbian emulator. Mọi vẽ/chuyển trạng thái phải có giới hạn và không cấp quyền file/network hệ thống.

Ví dụ skeleton tối thiểu (có thể chỉnh typography theo theme Studio, không chỉnh firmware):

```lua
local count = 0
function on_key(key, down)
  if not down then return end
  if key == 'start' then count = count + 1 end
end
function on_update(dt)
  -- Cập nhật logic có giới hạn; không gọi file/network/dofile.
end
function on_draw()
  engine.clear(0x0924)
  engine.text(12, 16, 'MY APPLICATION', 0xFFFF)
  engine.text(12, 40, 'START TO COUNT', 0xFFFF)
  engine.text(12, 64, tostring(count), 0x07E0)
end
```

`tests/input_replay.json` phải là danh sách object `{ "frame": integer, "key": string, "down": boolean }` theo đúng parser hiện hành. Mẫu an toàn: nhấn `start` ở frame 1, nhả ở frame 2. `tests/test_cases.json` là tài liệu testcase có nhãn `MANUAL/NOT_RUN` cho đến khi triển khai một test runner chính thức. Không tạo screenshot hay báo cáo PASS giả.

## 5. Phạm vi mã nguồn và kiến trúc

Trước khi sửa, kiểm tra commit hiện tại và đọc `AGENTS.md`, `PROMPT.md`, `SKILLS.md`, `tools/qstudio.py`, `studio/gui/window.py`, `studio/core/workspace.py`, template `projects/lua-hello/` và kiểm thử có liên quan. Nếu cấu trúc thực tế thay đổi so với đặc tả này, ưu tiên mã nguồn đã kiểm chứng, ghi chênh lệch vào Task Brief.

- `tools/qstudio.py`: tái sử dụng/thiết kế hàm tạo dự án và `validate()` hiện có; **không** viết riêng hai bộ schema khác nhau cho GUI và CLI.
- `studio/gui/window.py`: nối New Project với cùng generator; sau tạo, mở project, refresh Explorer, xử lý lỗi UI rõ ràng.
- `projects/`: lưu template nguồn mặc định hoặc dùng một module template/generator chia sẻ. Không sửa template cũ trừ mục tiêu tương thích đã được kiểm thử.
- `studio/tests/`, `tests/`: thêm regression CLI, GUI (khi có Qt), manifest, asset và đường dẫn.
- `docs/`: hướng dẫn áp dụng template, đường dẫn ứng dụng và yêu cầu firmware. Không sao chép repository OS vào Studio.

Nếu chọn template static, chứa bản mẫu dưới `projects/lua-standard/` và dùng cùng thuật toán copy; nếu chọn Python generator, đảm bảo nó tạo byte/tệp ổn định và có test đầy đủ. Không áp đặt cả hai đường triển khai cùng lúc.

## 6. An toàn dữ liệu và tương thích

1. Tạo trong thư mục staging **cùng ổ đĩa**, validate đầu ra, rồi chuyển nguyên tử khi có thể. Đích tồn tại => dừng, tuyệt đối không merge hoặc xóa project người dùng.
2. Khi bất kỳ thao tác nào thất bại, dọn **chỉ thư mục staging do lần chạy đó tạo**, hiển thị lỗi; không để lại project nửa chừng.
3. Từ chối symlink/path traversal, tệp có đường dẫn tuyệt đối, tên không hợp lệ, icon không đúng chuẩn và Lua vượt 64 KiB.
4. Không tạo/ghi private PEM, không tắt xác minh chữ ký, không gửi API key vào log.
5. Dự án hiện có và các mẫu Pocket Focus/Pocket Calculator vẫn mở, validate và preview bình thường.
6. Không sửa mã OS, GPIO, launcher, theme, icon hay renderer của VQEAF-OS khi làm tính năng template Studio.

## 7. Điều kiện nghiệm thu

- [ ] CLI `--template lua` tạo chính xác đầy đủ cấu trúc mục 2; nếu có alias `lua-standard`, đầu ra tương đương.
- [ ] GUI New Lua App tạo cùng manifest và cấu trúc với CLI; mở thành công trong Workspace/Explorer.
- [ ] `tools/qstudio.py validate <project>` PASS cho project mới; icon 32×32; Lua UTF-8 <=64 KiB.
- [ ] Host preview/replay chạy được **khi host VM sẵn có**; thiếu Lua host được ghi `SKIPPED/NOT_RUN`, không được tuyên bố PASS.
- [ ] Đích tồn tại/tên sai/icon lỗi/path traversal/quyền ghi lỗi không hủy dữ liệu hoặc sinh project nửa chừng.
- [ ] Các template cũ, samples, package signing và Studio launcher không regress.
- [ ] README của template ghi ranh giới **host Lua != firmware stock**, chỉ firmware Lua beta tương thích và chữ ký đúng mới xét chạy trên thiết bị.

## 8. Kết quả Agent bắt buộc bàn giao

Trình bày **những tệp đã chỉnh sửa**, sơ đồ thư mục đầu ra thực tế, lệnh và exit code của từng phép kiểm thử, ảnh GUI **chỉ khi đã chạy Qt thật**, danh sách PASS/FAIL/SKIPPED/NOT_RUN, giới hạn chưa hoàn thành và commit hash nếu **đã commit thực sự**. Dùng `docs/agents/TASK_BRIEF_TEMPLATE.md`, `HANDOFF_TEMPLATE.md`, `TEST_MATRIX.md` để ghi nghiệm thu. Chỉ commit/push nếu được giao; không âm thầm sửa repo hệ điều hành.
