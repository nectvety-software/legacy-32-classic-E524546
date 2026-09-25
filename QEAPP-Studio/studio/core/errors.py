"""User-facing Studio error codes (Vietnamese explanations + recovery hints)."""
from __future__ import annotations

from dataclasses import dataclass

__all__ = ['StudioError', 'ERROR_CATALOG', 'describe', 'make_error']


@dataclass(frozen=True)
class ErrorInfo:
    code: str
    title: str
    detail: str
    fix: str


ERROR_CATALOG: dict[str, ErrorInfo] = {
    'OS_ROOT_MISSING': ErrorInfo(
        code='OS_ROOT_MISSING',
        title='Chưa chọn mã nguồn VQEAF-OS',
        detail='Build signed QEAPP/2 cần checkout VQEAF-OS (không phải thư mục Studio).',
        fix='Chọn thư mục VQEAF-OS có platformio.ini và tools/build_qeapp.py, hoặc đặt QEAPP_FIRMWARE_ROOT.',
    ),
    'OS_ROOT_INVALID': ErrorInfo(
        code='OS_ROOT_INVALID',
        title='Đường dẫn firmware không hợp lệ',
        detail='Thư mục đã cấu hình không phải checkout VQEAF-OS hợp lệ (thiếu platformio.ini hoặc signer).',
        fix='Mở Settings → Build / Firmware, nhấn Browse… và Verify. Không chọn thư mục QEAPP-Studio.',
    ),
    'OS_ROOT_IS_STUDIO': ErrorInfo(
        code='OS_ROOT_IS_STUDIO',
        title='Đã chọn nhầm thư mục Studio',
        detail='Đường dẫn trỏ vào QEAPP-Studio, không phải repository VQEAF-OS.',
        fix='Chọn checkout VQEAF-OS riêng (cạnh Studio hoặc ngoài).',
    ),
    'LUA_BETA_REQUIRED': ErrorInfo(
        code='LUA_BETA_REQUIRED',
        title='Cần firmware Lua beta',
        detail='Gói type=lua chỉ đóng gói được khi có firmware vqeaf_lua_beta và trust key phù hợp.',
        fix='Build host/preview vẫn chạy. Để đóng gói thiết bị: dùng firmware Lua beta + khóa beta. Stock chỉ nhận web/text.',
    ),
    'SIGNER_MISSING': ErrorInfo(
        code='SIGNER_MISSING',
        title='Thiếu công cụ ký QEAPP/2',
        detail='Không tìm thấy tools/build_qeapp.py trong firmware source.',
        fix='Chọn checkout VQEAF-OS đầy đủ hoặc cập nhật đường dẫn trong Settings.',
    ),
    'PROJECT_INVALID': ErrorInfo(
        code='PROJECT_INVALID',
        title='Dự án không hợp lệ',
        detail='qeapp.project.json hoặc nội dung không đạt validate (schema/icon/Lua size).',
        fix='Chạy Validate (F6) và sửa theo PROBLEMS; không đổi runtime/firmware khi lỗi project.',
    ),
    'SIGNER_FAILED': ErrorInfo(
        code='SIGNER_FAILED',
        title='Ký gói thất bại',
        detail='Signer VQEAF-OS trả về lỗi (key, firmware root, hoặc payload).',
        fix='Kiểm tra khóa P-256, --experimental-lua với project Lua, và log OUTPUT. Không tắt signature check.',
    ),
    'VM_CRASH': ErrorInfo(
        code='VM_CRASH',
        title='Máy ảo Lua host lỗi',
        detail='Lua host VM dừng hoặc crash trong preview.',
        fix='Xem OUTPUT/PROBLEMS dòng Lua; thử lại F8/F9. Đây là VM PC, không phải ESP32.',
    ),
    'QT_PLUGIN_MISSING': ErrorInfo(
        code='QT_PLUGIN_MISSING',
        title='Thiếu Qt platform plugin',
        detail='PySide6 không tải được platform plugin (offscreen/windows).',
        fix='Chạy run_studio.bat --repair hoặc cài lại PySide6 trong .venv.',
    ),
    'HOST_LIMIT': ErrorInfo(
        code='HOST_LIMIT',
        title='Giới hạn máy ảo PC',
        detail='Một số guest API/chức năng chưa mô phỏng trên host (ví dụ T9).',
        fix='Đây là giới hạn tính năng, không phải crash. Dùng API engine được hỗ trợ trong main.lua.',
    ),
    'JOB_BUSY': ErrorInfo(
        code='JOB_BUSY',
        title='Đang có tiến trình chạy',
        detail='Một job/VM khác đang hoạt động.',
        fix='Nhấn STOP (Shift+F5) rồi thử lại.',
    ),
    'USER_CANCELLED': ErrorInfo(
        code='USER_CANCELLED',
        title='Đã hủy thao tác',
        detail='Người dùng hủy hộp thoại hoặc Stop giữa chừng.',
        fix='Chạy lại thao tác khi sẵn sàng. Project nguồn không bị thay đổi.',
    ),
}


class StudioError(Exception):
    """Exception carrying a stable error code for UI/PROBLEMS/status."""

    def __init__(self, code: str, detail: str = '', *, operation: str = '') -> None:
        info = ERROR_CATALOG.get(code)
        if info is None:
            info = ErrorInfo(code=code, title=code, detail=detail or code, fix='')
        self.code = info.code
        self.info = info
        self.detail = detail or info.detail
        self.operation = operation
        super().__init__(f'{self.code}: {self.detail}')

    def ui_message(self) -> str:
        parts = [f'[{self.code}] {self.info.title}', self.detail]
        if self.info.fix:
            parts.append('Khắc phục: ' + self.info.fix)
        return '\n'.join(parts)


def describe(code: str) -> ErrorInfo:
    return ERROR_CATALOG.get(code, ErrorInfo(code=code, title=code, detail=code, fix=''))


def make_error(code: str, detail: str = '', *, operation: str = '') -> StudioError:
    return StudioError(code, detail, operation=operation)
