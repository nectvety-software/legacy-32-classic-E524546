"""New Project dialog (LuaS30-IDE style): name, display name, unique App ID, template."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QComboBox, QDialog, QDialogButtonBox, QFormLayout, QFrame, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QVBoxLayout, QWidget,
)

from studio.core.app_id import collect_app_ids, is_valid_app_id, suggested_app_id
from studio.core.paths import project_dirname
from studio.core.project_scaffold import (
    ScaffoldError, create_standard_lua_project, validate_display_name,
    validate_project_name,
)

__all__ = ['NewProjectDialog', 'NewProjectResult', 'TEMPLATE_OPTIONS']

# (template_key, label, kind, description)
TEMPLATE_OPTIONS = (
    ('lua-standard', 'Blank — Standard Lua Application (Beta)', 'lua-standard',
     'Cấu trúc chuẩn: main.lua, assets, tests, docs'),
    ('lua', 'Lua App (Beta)', 'lua',
     'Mẫu Lua hello có sẵn (vẫn mở rộng cấu trúc chuẩn)'),
    ('lua-snake', 'Lua Snake (Beta)', 'lua-snake', 'Game rắn mẫu'),
    ('lua-sprite', 'Lua Sprite (Beta)', 'lua-sprite', 'Pixel sprite 1-bit'),
    ('lua-chess', 'Lua Chess (Beta)', 'lua-chess', 'Game cờ mẫu'),
    ('text', 'Text App', 'text', 'Gói văn bản QEAPP/2'),
    ('web', 'Web App', 'web', 'Bookmark HTTPS QEAPP/2'),
)


@dataclass(frozen=True)
class NewProjectResult:
    template: str
    project_name: str
    display_name: str
    app_id: str
    destination: Path


class NewProjectDialog(QDialog):
    """Collect project identity and scaffold a new QEAPP project."""

    def __init__(
        self,
        parent=None,
        *,
        projects_root: Path,
        default_template: str = 'lua-standard',
    ) -> None:
        super().__init__(parent)
        self.setWindowTitle('New QEAPP Project')
        self.setModal(True)
        self.setMinimumWidth(520)
        self._projects_root = Path(projects_root)
        self.result_config: NewProjectResult | None = None

        root = QVBoxLayout(self)
        root.setContentsMargins(18, 16, 18, 12)
        root.setSpacing(10)

        title = QLabel('Create project')
        title.setStyleSheet('font-size:16px; font-weight:600;')
        subtitle = QLabel(
            'App ID is random and unique — edit it or press Random. '
            'Blank/Standard Lua follows the default project structure.'
        )
        subtitle.setWordWrap(True)
        subtitle.setObjectName('Subtle')
        subtitle.setStyleSheet('color:#9ca2bb;')
        root.addWidget(title)
        root.addWidget(subtitle)

        form = QFormLayout()
        form.setLabelAlignment(Qt.AlignRight)
        form.setHorizontalSpacing(12)
        form.setVerticalSpacing(8)

        self.template = QComboBox()
        for key, label, _kind, description in TEMPLATE_OPTIONS:
            self.template.addItem(f'{label} — {description}', key)
        index = self.template.findData(default_template)
        self.template.setCurrentIndex(index if index >= 0 else 0)
        form.addRow('Template', self.template)

        self.project_name = QLineEdit()
        self.project_name.setPlaceholderText('MyApplication (folder name)')
        form.addRow('Project name', self.project_name)

        self.display_name = QLineEdit()
        self.display_name.setPlaceholderText('My Application')
        form.addRow('Display name', self.display_name)

        id_row = QHBoxLayout()
        self.app_id = QLineEdit()
        self.app_id.setPlaceholderText('app_xxxxxxxx')
        self.random_btn = QPushButton('Random')
        self.random_btn.setToolTip('Sinh App ID ngẫu nhiên, không trùng dự án khác')
        self.random_btn.clicked.connect(self._reroll_app_id)
        id_row.addWidget(self.app_id, 1)
        id_row.addWidget(self.random_btn)
        form.addRow('App ID', id_row)

        self.version = QLineEdit('1.0.0')
        self.version.setReadOnly(True)
        form.addRow('Version', self.version)

        self.path_preview = QLabel()
        self.path_preview.setWordWrap(True)
        self.path_preview.setStyleSheet('color:#9ca2bb;')
        form.addRow('Location', self.path_preview)

        self.status = QLabel('')
        self.status.setWordWrap(True)
        self.status.setStyleSheet('color:#ff9b32;')
        form.addRow('', self.status)

        wrap = QWidget()
        wrap.setLayout(form)
        root.addWidget(wrap)

        note = QFrame()
        note.setFrameShape(QFrame.StyledPanel)
        note.setStyleSheet('background:#18392f; border:1px solid #2a5a48; border-radius:6px; padding:8px;')
        note_l = QLabel(
            'Lua apps run in the PC host VM first. Device install of type=lua still needs '
            'vqeaf_lua_beta firmware and a matching trust key.'
        )
        note_l.setWordWrap(True)
        note_l.setStyleSheet('color:#7bd8ad;')
        note_box = QVBoxLayout(note)
        note_box.setContentsMargins(10, 8, 10, 8)
        note_box.addWidget(note_l)
        root.addWidget(note)

        buttons = QDialogButtonBox(QDialogButtonBox.Cancel | QDialogButtonBox.Ok)
        buttons.button(QDialogButtonBox.Ok).setText('Create')
        buttons.accepted.connect(self._accept)
        buttons.rejected.connect(self.reject)
        root.addWidget(buttons)

        self.display_name.textChanged.connect(self._sync_project_name)
        self.project_name.textChanged.connect(self._on_project_name_edited)
        self.app_id.textChanged.connect(self._update_preview)
        self.template.currentIndexChanged.connect(self._update_preview)
        self._sync_project_name()
        self._reroll_app_id()
        self._update_preview()

    def _on_project_name_edited(self) -> None:
        self.project_name.setProperty('userEdited', True)
        self._update_preview()

    def _sync_project_name(self) -> None:
        if self.project_name.property('userEdited'):
            self._update_preview()
            return
        raw = self.display_name.text().strip()
        folder = ''.join(ch if ch.isalnum() or ch in '-_' else '_' for ch in raw)
        folder = folder.strip('_') or 'MyApplication'
        self.project_name.blockSignals(True)
        self.project_name.setText(folder[:40])
        self.project_name.blockSignals(False)
        self._update_preview()

    def _reroll_app_id(self) -> None:
        try:
            self.app_id.setText(suggested_app_id(self._projects_root))
        except ValueError as exc:
            self.status.setText(str(exc))
        self._update_preview()

    def _update_preview(self) -> None:
        name = self.project_name.text().strip() or '<project>'
        self.path_preview.setText(str(self._projects_root / name))
        app_id = self.app_id.text().strip()
        if app_id and not is_valid_app_id(app_id):
            self.status.setText('App ID phải khớp [a-z0-9_-]{1,24}')
        elif app_id and app_id in collect_app_ids(self._projects_root):
            self.status.setText(f'App ID đã trùng: {app_id}')
        else:
            self.status.setText('')

    def _accept(self) -> None:
        try:
            template = str(self.template.currentData() or 'lua-standard')
            project_name = validate_project_name(self.project_name.text())
            display_name = validate_display_name(self.display_name.text())
            app_id = self.app_id.text().strip()
            if not is_valid_app_id(app_id):
                raise ScaffoldError('App ID phải khớp [a-z0-9_-]{1,24}')
            used = collect_app_ids(self._projects_root)
            if app_id in used:
                raise ScaffoldError(f'App ID đã tồn tại: {app_id}')
            # Ensure folder name stays aligned with project_name.
            try:
                project_dirname(project_name)
            except ValueError as exc:
                raise ScaffoldError(str(exc)) from exc
            destination = self._projects_root / project_name
            if destination.exists():
                raise ScaffoldError('Thư mục dự án đã tồn tại: ' + str(destination))
            self.result_config = NewProjectResult(
                template=template,
                project_name=project_name,
                display_name=display_name,
                app_id=app_id,
                destination=destination,
            )
        except ScaffoldError as exc:
            self.status.setText(str(exc))
            return
        self.accept()

    def get_result(self) -> NewProjectResult:
        if self.result_config is None:
            raise RuntimeError('Dialog was not accepted')
        return self.result_config
