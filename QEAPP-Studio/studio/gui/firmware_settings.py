"""Settings → Build / Firmware and OS_ROOT recovery dialogs."""
from __future__ import annotations

from pathlib import Path

from PySide6.QtWidgets import (
    QDialog, QDialogButtonBox, QFileDialog, QHBoxLayout, QLabel, QLineEdit,
    QPushButton, QVBoxLayout, QWidget,
)

from studio.core.errors import ERROR_CATALOG, StudioError

__all__ = ['FirmwareSettingsDialog', 'FirmwareRecoveryDialog']


def _refresh_verdict(path_edit: QLineEdit, verdict: QLabel) -> dict:
    from tools.studio_firmware import inspect_firmware_root

    report = inspect_firmware_root(path_edit.text().strip())
    code = report.get('error_code') or ('OK' if report.get('ready_for_build') else 'OS_ROOT_INVALID')
    if report.get('ready_for_build'):
        lua = ' · Lua beta build ready' if report.get('ready_for_lua_build') else ' · Lua beta NOT in this checkout (host preview still works)'
        verdict.setStyleSheet('color:#7bd8ad;')
        verdict.setText(f'[{code}] {report.get("resolved", "")}{lua}')
    else:
        verdict.setStyleSheet('color:#ff9b32;')
        info = ERROR_CATALOG.get(code)
        title = info.title if info else code
        verdict.setText(f'[{code}] {title}\n{report.get("message", "")}')
    return report


class FirmwareSettingsDialog(QDialog):
    """Configure VQEAF-OS firmware source used by signed Build."""

    def __init__(self, parent=None, *, current: str = '') -> None:
        super().__init__(parent)
        self.setWindowTitle('Settings — Build / Firmware')
        self.setModal(True)
        self.setMinimumWidth(560)
        self.selected_path: str | None = None

        root = QVBoxLayout(self)
        root.setContentsMargins(16, 14, 16, 12)
        root.setSpacing(10)

        title = QLabel('Firmware source folder (VQEAF-OS)')
        title.setStyleSheet('font-size:14px; font-weight:600;')
        hint = QLabel(
            'Chọn checkout VQEAF-OS — KHÔNG chọn thư mục QEAPP-Studio.\n'
            'Cần platformio.ini + tools/build_qeapp.py. Ưu tiên: biến môi trường '
            'QEAPP_FIRMWARE_ROOT → Settings → thư mục VQEAF-OS cạnh Studio.'
        )
        hint.setWordWrap(True)
        hint.setStyleSheet('color:#9ca2bb;')
        root.addWidget(title)
        root.addWidget(hint)

        row = QHBoxLayout()
        self.path_edit = QLineEdit(current or '')
        self.path_edit.setPlaceholderText(r'D:\Projects\VQEAF-OS')
        browse = QPushButton('Browse…')
        browse.clicked.connect(self._browse)
        row.addWidget(self.path_edit, 1)
        row.addWidget(browse)
        root.addLayout(row)

        verify_row = QHBoxLayout()
        verify = QPushButton('Verify')
        verify.clicked.connect(self._verify)
        self.verdict = QLabel('')
        self.verdict.setWordWrap(True)
        verify_row.addWidget(verify)
        verify_row.addWidget(self.verdict, 1)
        root.addLayout(verify_row)

        note = QLabel(
            'Build signed .qeapp cần firmware source. Validate / Run Host / Virtual Phone '
            'vẫn hoạt động khi chưa có firmware. Không lưu khóa ký trong Settings.'
        )
        note.setWordWrap(True)
        note.setStyleSheet('color:#9ca2bb;')
        root.addWidget(note)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.button(QDialogButtonBox.Ok).setText('Save')
        buttons.accepted.connect(self._accept)
        buttons.rejected.connect(self.reject)
        root.addWidget(buttons)

        if current:
            _refresh_verdict(self.path_edit, self.verdict)

    def _browse(self) -> None:
        folder = QFileDialog.getExistingDirectory(self, 'Choose VQEAF-OS firmware source')
        if folder:
            self.path_edit.setText(folder)
            _refresh_verdict(self.path_edit, self.verdict)

    def _verify(self) -> dict:
        return _refresh_verdict(self.path_edit, self.verdict)

    def _accept(self) -> None:
        report = self._verify()
        if report.get('is_studio'):
            self.verdict.setText('[OS_ROOT_IS_STUDIO] ' + ERROR_CATALOG['OS_ROOT_IS_STUDIO'].detail)
            return
        if not report.get('ready_for_build'):
            # Allow saving an empty path to clear; reject clearly invalid non-empty paths.
            if self.path_edit.text().strip():
                self.verdict.setText(
                    f'[{report.get("error_code") or "OS_ROOT_INVALID"}] '
                    'Đường dẫn chưa phải checkout VQEAF-OS hợp lệ. Sửa rồi Verify lại.'
                )
                return
        self.selected_path = self.path_edit.text().strip()
        self.accept()


class FirmwareRecoveryDialog(QDialog):
    """Actionable recovery for OS_ROOT_MISSING / OS_ROOT_INVALID during Build."""

    def __init__(self, parent=None, *, error: StudioError | str, current: str = '') -> None:
        super().__init__(parent)
        if isinstance(error, StudioError):
            self.error = error
        else:
            code = str(error)
            self.error = StudioError(code if code in ERROR_CATALOG else 'OS_ROOT_MISSING')
        self.setWindowTitle(self.error.info.title)
        self.setModal(True)
        self.setMinimumWidth(520)
        self.selected_path: str | None = None
        self.open_settings = False

        root = QVBoxLayout(self)
        root.setContentsMargins(16, 14, 16, 12)
        root.setSpacing(10)

        code_label = QLabel(f'[{self.error.code}]')
        code_label.setStyleSheet('color:#ff9b32; font-weight:600;')
        body = QLabel(self.error.ui_message())
        body.setWordWrap(True)
        root.addWidget(code_label)
        root.addWidget(body)

        self.path_edit = QLineEdit(current or '')
        self.path_edit.setPlaceholderText('VQEAF-OS folder…')
        root.addWidget(self.path_edit)

        row = QHBoxLayout()
        pick = QPushButton('Chọn thư mục VQEAF-OS…')
        pick.clicked.connect(self._pick)
        settings_btn = QPushButton('Mở cài đặt')
        settings_btn.clicked.connect(self._open_settings)
        cancel = QPushButton('Hủy')
        cancel.clicked.connect(self.reject)
        retry = QPushButton('Thử lại sau khi chọn')
        retry.clicked.connect(self._retry)
        row.addWidget(pick)
        row.addWidget(settings_btn)
        row.addStretch(1)
        row.addWidget(cancel)
        row.addWidget(retry)
        root.addLayout(row)

        self.verdict = QLabel('')
        self.verdict.setWordWrap(True)
        root.addWidget(self.verdict)

    def _pick(self) -> None:
        folder = QFileDialog.getExistingDirectory(self, 'Choose VQEAF-OS firmware source')
        if folder:
            self.path_edit.setText(folder)
            _refresh_verdict(self.path_edit, self.verdict)

    def _open_settings(self) -> None:
        self.open_settings = True
        self.reject()

    def _retry(self) -> None:
        report = _refresh_verdict(self.path_edit, self.verdict)
        if report.get('ready_for_build'):
            self.selected_path = report['resolved']
            self.accept()
        else:
            self.verdict.setText(
                f'[{report.get("error_code") or "OS_ROOT_INVALID"}] Chưa chọn được firmware hợp lệ.'
            )
