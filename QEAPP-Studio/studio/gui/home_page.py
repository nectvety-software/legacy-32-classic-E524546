from __future__ import annotations

import hashlib
from collections.abc import Iterable, Mapping
from datetime import datetime
from pathlib import Path
from typing import Any

from PySide6.QtCore import QPoint, QRect, Qt, Signal
from PySide6.QtGui import QColor, QFont, QFontMetrics, QLinearGradient, QPainter, QPainterPath, QPolygon
from PySide6.QtWidgets import (
    QButtonGroup,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QMenu,
    QPushButton,
    QScrollArea,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

__all__ = ['HOME_STYLE', 'HomePage']

HOME_STYLE = '''
QWidget#HomePage { background:#111122; color:#f1f2f7; }
QFrame#HomeSidebar { background:#151527; border-right:1px solid #24243d; }
QPushButton#SidebarButton { background:transparent; border:0; border-radius:6px;
  color:#9ca2bb; text-align:left; padding:9px 12px; font-size:12px; }
QPushButton#SidebarButton:hover { background:#23233c; color:#f1f2f7; }
QPushButton#SidebarButton:checked { background:#2c2c4e; color:#ffffff; font-weight:700; }
QFrame#SidebarDivider { background:#24243d; max-height:1px; border:0; }
QLabel#SidebarSection { color:#6d7290; font-size:10px; font-weight:700; letter-spacing:1px; padding:6px 12px 2px 12px; }
QPushButton#SidebarUtility { background:transparent; border:0; border-radius:6px;
  color:#9ca2bb; text-align:left; padding:8px 12px; }
QPushButton#SidebarUtility:hover { background:#23233c; color:#f1f2f7; }
QFrame#EngineCard { background:#19192e; border:1px solid #303049; border-radius:8px; }
QFrame#EngineIcon { background:#ff8a00; border-radius:6px; }
QLabel#EngineIconText { color:#1a1a2c; font-size:15px; font-weight:700; background:transparent; }
QLabel#EngineName { color:#f1f2f7; font-size:12px; font-weight:700; background:transparent; }
QLabel#EngineVersion { color:#8b91aa; font-size:10px; background:transparent; }
QLabel#HomeTitle { color:#f1f2f7; font-size:20px; font-weight:700; }
QLabel#HomeSubtitle { color:#9ca2bb; font-size:12px; }
QPushButton#SecondaryAction { background:#1c1c33; border:1px solid #303049; border-radius:6px;
  color:#f1f2f7; padding:8px 16px; }
QPushButton#SecondaryAction:hover { background:#23233c; border-color:#424262; }
QPushButton#PrimaryAction { background:#ff8a00; border:1px solid #ff8a00; border-radius:6px;
  color:#1a1a2c; font-weight:700; padding:8px 16px; }
QPushButton#PrimaryAction:hover { background:#ff9a22; border-color:#ff9a22; }
QPushButton#PrimaryAction:pressed { background:#e87a00; }
QFrame#HomeDivider { background:#24243d; max-height:1px; border:0; }
QLabel#SectionTitle { color:#f1f2f7; font-size:13px; font-weight:700; }
QLabel#SectionCount { color:#8b91aa; font-size:11px; }
QPushButton#SectionAction { background:transparent; border:0; border-radius:4px;
  color:#ff8a00; padding:4px 8px; font-size:11px; }
QPushButton#SectionAction:hover { background:#23233c; }
QFrame#ProjectCard { background:#19192e; border:1px solid #24243d; border-radius:10px; }
QFrame#ProjectCard:hover { border-color:#303049; background:#1a1a31; }
QLabel#ProjectCardTitle { color:#f1f2f7; font-size:13px; font-weight:700; background:transparent; }
QPushButton#CardMenuButton { background:transparent; border:0; border-radius:6px;
  color:#8b91aa; padding:2px 7px; font-size:14px; }
QPushButton#CardMenuButton:hover { background:#2c2c4e; color:#ffffff; }
QLabel#ProjectCardMeta { color:#8b91aa; font-size:10px; background:transparent; }
QLabel#FrameworkBadge { background:#24243d; color:#d8dbe7; border-radius:4px;
  padding:2px 6px; font-size:11px; }
QLabel#NativeBadge { background:#22382f; color:#7bd8ad; border-radius:4px;
  padding:2px 6px; font-size:11px; }
QLabel#BuildBadge[build="built"] { background:#22382f; color:#7bd8ad; }
QLabel#BuildBadge[build="stale"] { background:#3a2f1c; color:#ff9b32; }
QLabel#BuildBadge[build="failed"] { background:#3d2128; color:#ff6375; }
QLabel#BuildBadge { background:#24243d; color:#8b91aa; border-radius:4px;
  padding:2px 6px; font-size:11px; }
QLabel#ProjectCardPath { color:#6d7290; font-size:10px; background:transparent; }
QFrame#EmptyState { background:#19192e; border:1px dashed #303049; border-radius:12px; }
QLabel#EmptyIcon { color:#ff8a00; font-size:38px; font-weight:700; background:transparent; }
QLabel#EmptyTitle { color:#f1f2f7; font-size:17px; font-weight:700; background:transparent; }
QLabel#EmptyText { color:#9ca2bb; font-size:12px; background:transparent; }
QScrollArea { border:0; background:transparent; }
QScrollArea > QWidget > QWidget { background:transparent; }
QScrollBar:vertical { background:#151527; width:10px; margin:2px; }
QScrollBar::handle:vertical { background:#3a3a56; border-radius:5px; min-height:32px; }
QScrollBar::handle:vertical:hover { background:#575774; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; width:0; }
QPushButton::menu-indicator { image:none; }
QMenu { background:#19192e; color:#d8dbe7; border:1px solid #303049; border-radius:6px; padding:5px; }
QMenu::item { padding:6px 24px 6px 14px; border-radius:4px; }
QMenu::item:selected { background:#1c2a3b; color:#ffffff; }
'''

_PREVIEW_PALETTES: tuple[tuple[str, str, str, str, str], ...] = (
    ('#4a6fa5', '#8fc7e8', '#2f4d3a', '#6fae5a', '#ffd45e'),
    ('#7a5aa0', '#d6a3e0', '#3a2f4d', '#5a7d4a', '#ff8a00'),
    ('#2f6f8f', '#9ad9e8', '#274a3f', '#4f9a6a', '#ffe08a'),
    ('#a05a5a', '#f0b48a', '#4d3a2f', '#8a9a4a', '#fff0a0'),
    ('#3a5a8f', '#8ab6f0', '#2a3a5a', '#4a8a6a', '#ffd05e'),
    ('#4f7a4a', '#b8e08a', '#33472a', '#6a9a4a', '#ffcf4a'),
)


def _value(record: Any, name: str, default: Any = '') -> Any:
    if isinstance(record, Mapping):
        return record.get(name, default)
    return getattr(record, name, default)


def _record_path(record: Any) -> str:
    value = _value(record, 'path', _value(record, 'project_path', _value(record, 'folder', '')))
    if isinstance(value, Path):
        return str(value)
    return str(value or '')


def _is_recent(record: Any) -> bool:
    value = _value(record, 'recent', _value(record, 'is_recent', False))
    return bool(value)


def _type_text(record: Any) -> str:
    return str(_value(record, 'type', _value(record, 'project_type', 'unknown')) or 'unknown')


def _bytes_text(value: Any) -> str:
    try:
        size = max(0, int(value))
    except (TypeError, ValueError, OverflowError):
        return '0 B'
    units = ('B', 'KB', 'MB', 'GB')
    number = float(size)
    for unit in units:
        if number < 1024 or unit == units[-1]:
            if unit == 'B':
                return f'{int(number)} {unit}'
            return f'{number:.1f} {unit}'
        number /= 1024
    return f'{size} B'


def _modified_text(value: Any) -> str:
    if isinstance(value, datetime):
        stamp = value
    else:
        try:
            number = float(value)
            stamp = datetime.fromtimestamp(number) if number > 0 else None
        except (TypeError, ValueError, OverflowError, OSError):
            stamp = None
    if stamp is None:
        return 'không rõ'
    return stamp.strftime('%d/%m/%Y · %H:%M')


def _app_version() -> str:
    try:
        text = Path(__file__).resolve().parents[2].joinpath('VERSION').read_text(encoding='utf-8').strip()
    except OSError:
        return '0.7.3'
    return text or '0.7.3'


class ProjectPreview(QWidget):
    """Deterministic pixel scene painted from the project path (no image assets)."""

    def __init__(self, path: str, parent: QWidget | None = None):
        super().__init__(parent)
        self.setObjectName('ProjectPreview')
        self._path = path
        self.setMinimumHeight(108)
        self.setMaximumHeight(118)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        digest = hashlib.md5(path.encode('utf-8', 'ignore')).digest()
        self._colors = _PREVIEW_PALETTES[digest[0] % len(_PREVIEW_PALETTES)]
        self._seed = digest[1]

    def paintEvent(self, event) -> None:  # noqa: N802 - Qt API
        del event
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing, True)
        sky_top, sky_bottom, mountain, ground, accent = self._colors
        rect = QRect(self.rect())
        radius = 9
        clip = QPainterPath()
        clip.addRoundedRect(rect.x(), rect.y(), rect.width(), rect.height(), radius, radius)
        painter.setClipPath(clip)

        gradient = QLinearGradient(0, 0, 0, rect.height())
        gradient.setColorAt(0.0, QColor(sky_top))
        gradient.setColorAt(1.0, QColor(sky_bottom))
        painter.fillRect(rect, gradient)

        horizon = int(rect.height() * 0.66)
        base = rect.height()
        peaks = (
            (0.10, 0.34),
            (0.38, 0.26),
            (0.66, 0.32),
            (0.92, 0.24),
        )
        painter.setPen(Qt.NoPen)
        for index, (center, height) in enumerate(peaks):
            shift = ((self._seed >> index) & 0x0F) / 64.0
            peak_x = int(rect.width() * min(0.96, max(0.04, center + shift * 0.08)))
            peak_y = int(horizon - rect.height() * height)
            width = int(rect.width() * 0.42)
            color = QColor(mountain)
            color.setAlpha(215 + ((self._seed + index * 17) % 40))
            painter.setBrush(color)
            painter.drawPolygon(QPolygon([
                QPoint(peak_x - width // 2, horizon),
                QPoint(peak_x, peak_y),
                QPoint(peak_x + width // 2, horizon),
            ]))

        ground_rect = QRect(rect.left(), horizon, rect.width(), rect.height() - horizon)
        painter.fillRect(ground_rect, QColor(ground))
        painter.setPen(QColor(ground).darker(120))
        painter.drawLine(ground_rect.topLeft(), ground_rect.topRight())

        char_x = int(rect.width() * (0.24 + ((self._seed & 0x07) / 56.0)))
        char_rect = QRect(char_x, horizon - 24, 16, 24)
        painter.setBrush(QColor('#20242f'))
        painter.drawRoundedRect(char_rect, 4, 4)
        painter.setBrush(QColor(accent))
        painter.drawRoundedRect(QRect(char_x + 2, horizon - 24, 12, 7), 3, 3)
        painter.setBrush(QColor('#f5f6fa'))
        painter.drawEllipse(char_x + 4, horizon - 15, 3, 3)
        painter.drawEllipse(char_x + 10, horizon - 15, 3, 3)

        coin_x = int(rect.width() * (0.70 + ((self._seed >> 3) & 0x0F) / 96.0))
        coin_y = int(horizon - 30 - ((self._seed >> 5) & 0x07))
        painter.setBrush(QColor(accent))
        painter.drawEllipse(coin_x, coin_y, 12, 12)
        painter.setPen(QColor('#ffffff').darker(140))
        painter.setBrush(Qt.NoBrush)
        painter.drawEllipse(coin_x + 2, coin_y + 2, 8, 8)


class ProjectCard(QFrame):
    activated = Signal()

    def __init__(self, path: str, parent: QWidget | None = None):
        super().__init__(parent)
        self.setObjectName('ProjectCard')
        self.setMinimumWidth(210)
        self.setMaximumWidth(340)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.setToolTip(path)
        self.setCursor(Qt.PointingHandCursor)

    def mouseDoubleClickEvent(self, event) -> None:  # noqa: N802 - Qt API
        if event.button() == Qt.LeftButton:
            self.activated.emit()
        super().mouseDoubleClickEvent(event)


class HomePage(QWidget):
    new_project_requested = Signal()
    open_project_requested = Signal(str)
    open_folder_requested = Signal(str)
    reveal_requested = Signal(str)
    remove_recent_requested = Signal(str)
    refresh_requested = Signal()

    def __init__(self, parent: QWidget | None = None):
        super().__init__(parent)
        self.setObjectName('HomePage')
        self.setMinimumSize(720, 480)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setStyleSheet(HOME_STYLE)
        self._records: list[Any] = []
        self._mode = 'recent'
        self.cards: list[QFrame] = []
        self.project_cards = self.cards
        self._card_records: dict[QFrame, Any] = {}
        self._build_ui()
        self.set_records([])

    def _build_ui(self) -> None:
        row = QHBoxLayout(self)
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(0)

        sidebar = QFrame()
        sidebar.setObjectName('HomeSidebar')
        sidebar.setFixedWidth(200)
        side = QVBoxLayout(sidebar)
        side.setContentsMargins(16, 18, 16, 18)
        side.setSpacing(6)

        self.recent_button = QPushButton('⌂   Trang chủ')
        self.all_button = QPushButton('▤   Dự án')
        for button in (self.recent_button, self.all_button):
            button.setObjectName('SidebarButton')
            button.setCheckable(True)
            button.setCursor(Qt.PointingHandCursor)
        self.recent_button.setChecked(True)
        self.mode_group = QButtonGroup(self)
        self.mode_group.setExclusive(True)
        self.mode_group.addButton(self.recent_button, 0)
        self.mode_group.addButton(self.all_button, 1)
        self.recent_button.clicked.connect(lambda: self.set_mode('recent'))
        self.all_button.clicked.connect(lambda: self.set_mode('all'))
        side.addWidget(self.recent_button)
        side.addWidget(self.all_button)

        divider = QFrame()
        divider.setObjectName('SidebarDivider')
        divider.setFrameShape(QFrame.HLine)
        side.addWidget(divider)
        side.addWidget(QLabel('CÔNG CỤ'))

        self.refresh_button = QPushButton('Làm mới danh sách')
        self.refresh_button.setObjectName('SidebarUtility')
        self.refresh_button.setCursor(Qt.PointingHandCursor)
        self.refresh_button.clicked.connect(lambda _checked=False: self.refresh_requested.emit())
        side.addWidget(self.refresh_button)
        side.addStretch(1)

        engine = QFrame()
        engine.setObjectName('EngineCard')
        engine_layout = QHBoxLayout(engine)
        engine_layout.setContentsMargins(12, 10, 12, 10)
        engine_layout.setSpacing(10)
        icon = QFrame()
        icon.setObjectName('EngineIcon')
        icon.setFixedSize(28, 28)
        icon_layout = QVBoxLayout(icon)
        icon_layout.setContentsMargins(0, 0, 0, 0)
        icon_text = QLabel('Q')
        icon_text.setObjectName('EngineIconText')
        icon_text.setAlignment(Qt.AlignCenter)
        icon_layout.addWidget(icon_text)
        engine_names = QVBoxLayout()
        engine_names.setSpacing(1)
        engine_name = QLabel('QEAPP Studio')
        engine_name.setObjectName('EngineName')
        engine_version = QLabel(f'Phiên bản {_app_version()}')
        engine_version.setObjectName('EngineVersion')
        engine_names.addWidget(engine_name)
        engine_names.addWidget(engine_version)
        engine_layout.addWidget(icon)
        engine_layout.addLayout(engine_names, 1)
        side.addWidget(engine)
        row.addWidget(sidebar)

        content = QVBoxLayout()
        content.setContentsMargins(38, 28, 38, 34)
        content.setSpacing(16)

        header = QHBoxLayout()
        header.setSpacing(18)
        titles = QVBoxLayout()
        titles.setSpacing(4)
        title = QLabel('Chào mừng trở lại')
        title.setObjectName('HomeTitle')
        subtitle = QLabel('Dự án QEAPP/VQEAF · host VM Lua 240×320 · Virtual Phone')
        subtitle.setObjectName('HomeSubtitle')
        subtitle.setWordWrap(True)
        titles.addWidget(title)
        titles.addWidget(subtitle)
        header.addLayout(titles, 1)

        self.open_button = QPushButton('Mở dự án')
        self.open_button.setObjectName('SecondaryAction')
        self.open_button.setCursor(Qt.PointingHandCursor)
        self.open_button.clicked.connect(lambda: self.open_project_requested.emit(''))
        self.new_button = QPushButton('＋  Dự án mới')
        self.new_button.setObjectName('PrimaryAction')
        self.new_button.setCursor(Qt.PointingHandCursor)
        self.new_button.clicked.connect(lambda _checked=False: self.new_project_requested.emit())
        header.addWidget(self.open_button)
        header.addWidget(self.new_button)
        content.addLayout(header)

        home_divider = QFrame()
        home_divider.setObjectName('HomeDivider')
        home_divider.setFrameShape(QFrame.HLine)
        content.addWidget(home_divider)

        section_row = QHBoxLayout()
        section_row.setSpacing(12)
        self.section_label = QLabel('Dự án gần đây')
        self.section_label.setObjectName('SectionTitle')
        self.see_all_button = QPushButton('Xem tất cả →')
        self.see_all_button.setObjectName('SectionAction')
        self.see_all_button.setCursor(Qt.PointingHandCursor)
        self.see_all_button.clicked.connect(lambda: self.set_mode('all'))
        self.see_all_button.hide()
        self.count_label = QLabel('0 dự án')
        self.count_label.setObjectName('SectionCount')
        section_row.addWidget(self.section_label)
        section_row.addWidget(self.see_all_button)
        section_row.addStretch(1)
        section_row.addWidget(self.count_label)
        content.addLayout(section_row)

        self.scroll = QScrollArea()
        self.scroll.setWidgetResizable(True)
        self.scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.scroll.setFrameShape(QFrame.NoFrame)
        self.grid_host = QWidget()
        self.grid_layout = QGridLayout(self.grid_host)
        self.grid_layout.setContentsMargins(0, 0, 8, 0)
        self.grid_layout.setHorizontalSpacing(12)
        self.grid_layout.setVerticalSpacing(14)
        self.grid_layout.setAlignment(Qt.AlignTop | Qt.AlignLeft)
        self.scroll.setWidget(self.grid_host)
        content.addWidget(self.scroll, 1)

        self.empty_state = QFrame()
        self.empty_state.setObjectName('EmptyState')
        empty_layout = QVBoxLayout(self.empty_state)
        empty_layout.setContentsMargins(30, 42, 30, 42)
        empty_layout.setSpacing(10)
        empty_icon = QLabel('✦')
        empty_icon.setObjectName('EmptyIcon')
        empty_icon.setAlignment(Qt.AlignCenter)
        self.empty_title = QLabel('Chưa có dự án nào')
        self.empty_title.setObjectName('EmptyTitle')
        self.empty_title.setAlignment(Qt.AlignCenter)
        self.empty_text = QLabel('Tạo dự án mới hoặc mở thư mục chứa qeapp.project.json.')
        self.empty_text.setObjectName('EmptyText')
        self.empty_text.setAlignment(Qt.AlignCenter)
        self.empty_text.setWordWrap(True)
        empty_actions = QHBoxLayout()
        empty_actions.setAlignment(Qt.AlignCenter)
        self.empty_new_button = QPushButton('＋  Tạo dự án mới')
        self.empty_new_button.setObjectName('PrimaryAction')
        self.empty_new_button.clicked.connect(lambda _checked=False: self.new_project_requested.emit())
        self.empty_open_button = QPushButton('Mở dự án')
        self.empty_open_button.setObjectName('SecondaryAction')
        self.empty_open_button.clicked.connect(lambda: self.open_project_requested.emit(''))
        empty_actions.addWidget(self.empty_new_button)
        empty_actions.addWidget(self.empty_open_button)
        empty_layout.addWidget(empty_icon)
        empty_layout.addWidget(self.empty_title)
        empty_layout.addWidget(self.empty_text)
        empty_layout.addLayout(empty_actions)
        content.addWidget(self.empty_state)

        row.addLayout(content, 1)

    def set_records(self, records: Iterable[Any] | Any | None) -> None:
        if records is None:
            values: list[Any] = []
        elif hasattr(records, 'scan') and callable(records.scan):
            values = list(records.scan())
        elif isinstance(records, (str, bytes, Mapping)):
            values = [records]
        else:
            try:
                values = list(records)
            except TypeError:
                values = []
        self._records = values
        self._update_view()

    def records(self) -> list[Any]:
        return list(self._records)

    def set_mode(self, mode: str) -> None:
        normalized = str(mode or '').strip().casefold().replace('-', '_').replace(' ', '_')
        if normalized in {'all_projects', 'everything'}:
            normalized = 'all'
        if normalized not in {'recent', 'all'}:
            raise ValueError("mode must be 'recent' or 'all'")
        self._mode = normalized
        self.recent_button.setChecked(normalized == 'recent')
        self.all_button.setChecked(normalized == 'all')
        self._update_view()

    def mode(self) -> str:
        return self._mode

    def _visible_records(self) -> list[Any]:
        if self._mode == 'all':
            return list(self._records)
        explicit = [record for record in self._records if _is_recent(record)]
        return explicit if explicit else list(self._records[:4])

    def _clear_cards(self) -> None:
        while self.grid_layout.count():
            item = self.grid_layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.setParent(None)
                widget.deleteLater()
        self.cards.clear()
        self._card_records.clear()

    def _rebuild_cards(self) -> None:
        for record in self._visible_records():
            card = self._make_card(record)
            self.cards.append(card)
            self._card_records[card] = record
        self._relayout()

    def _make_card(self, record: Any) -> QFrame:
        path = _record_path(record)
        card = ProjectCard(path)
        layout = QVBoxLayout(card)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(9)

        preview = ProjectPreview(path)
        layout.addWidget(preview)

        title_row = QHBoxLayout()
        title_row.setSpacing(6)
        name = QLabel(str(_value(record, 'name', Path(path).name or 'Dự án chưa tên')))
        name.setTextFormat(Qt.TextFormat.PlainText)
        name.setObjectName('ProjectCardTitle')
        name.setToolTip(str(name.text()))
        menu_button = QPushButton('⋮')
        menu_button.setObjectName('CardMenuButton')
        menu_button.setFlat(True)
        menu_button.setFixedWidth(26)
        menu_button.setCursor(Qt.PointingHandCursor)
        menu = QMenu(menu_button)
        menu.addAction('Mở trong trình soạn thảo', lambda: self._open_record(record))
        menu.addAction('Mở thư mục dự án', lambda: self._folder_record(record))
        menu.addAction('Hiện vị trí', lambda: self._reveal_record(record))
        if _is_recent(record):
            menu.addSeparator()
            menu.addAction('Xóa khỏi danh sách', lambda: self._remove_record(record))
        menu_button.setMenu(menu)
        title_row.addWidget(name, 1)
        title_row.addWidget(menu_button, 0, Qt.AlignTop)
        layout.addLayout(title_row)

        meta = QLabel(
            f"Đã sửa {_modified_text(_value(record, 'modified', _value(record, 'modified_at', None)))}"
            f" · {_value(record, 'file_count', _value(record, 'files', 0))} tệp"
            f" · {_bytes_text(_value(record, 'size_bytes', _value(record, 'total_size', _value(record, 'size', 0))))}"
        )
        meta.setTextFormat(Qt.TextFormat.PlainText)
        meta.setObjectName('ProjectCardMeta')
        layout.addWidget(meta)

        chips = QHBoxLayout()
        chips.setSpacing(6)
        type_chip = QLabel(_type_text(record))
        type_chip.setTextFormat(Qt.TextFormat.PlainText)
        type_chip.setObjectName('FrameworkBadge')
        chips.addWidget(type_chip)
        app_id = str(_value(record, 'id', _value(record, 'project_id', '')) or '')
        if app_id:
            id_chip = QLabel(f'AppID {app_id}')
            id_chip.setTextFormat(Qt.TextFormat.PlainText)
            id_chip.setObjectName('NativeBadge')
            chips.addWidget(id_chip)
        build_state = str(_value(record, 'build_status', _value(record, 'status', 'not-built')) or 'not-built').casefold()
        normalized = 'built' if build_state == 'built' else build_state
        if normalized not in {'built', 'stale', 'failed'}:
            normalized = 'pending'
        build_chip = QLabel({
            'built': 'Đã build',
            'stale': 'Cần build lại',
            'failed': 'Lỗi build',
        }.get(normalized, 'Chưa build'))
        build_chip.setTextFormat(Qt.TextFormat.PlainText)
        build_chip.setObjectName('BuildBadge')
        build_chip.setProperty('build', normalized)
        chips.addWidget(build_chip)
        chips.addStretch(1)
        layout.addLayout(chips)

        metrics = QFontMetrics(QFont('Segoe UI', 8))
        elided = metrics.elidedText(path, Qt.ElideMiddle, 300)
        path_label = QLabel(elided)
        path_label.setTextFormat(Qt.TextFormat.PlainText)
        path_label.setObjectName('ProjectCardPath')
        path_label.setToolTip(path)
        layout.addWidget(path_label)

        card.activated.connect(lambda item=record: self._open_record(item))
        return card

    def _open_record(self, record: Any) -> None:
        self.open_project_requested.emit(_record_path(record))

    def _folder_record(self, record: Any) -> None:
        self.open_folder_requested.emit(_record_path(record))

    def _reveal_record(self, record: Any) -> None:
        self.reveal_requested.emit(_record_path(record))

    def _remove_record(self, record: Any) -> None:
        self.remove_recent_requested.emit(_record_path(record))

    def _update_view(self) -> None:
        self._clear_cards()
        self._rebuild_cards()
        visible = self._cards_count()
        self.scroll.setVisible(bool(visible))
        self.empty_state.setVisible(not visible)
        if self._mode == 'recent':
            self.section_label.setText('Dự án gần đây')
            self.empty_title.setText('Chưa có dự án nào')
            self.empty_text.setText('Tạo dự án mới hoặc mở thư mục chứa qeapp.project.json.')
            self.see_all_button.setVisible(len(self._records) > visible)
        else:
            self.section_label.setText('Tất cả dự án')
            self.empty_title.setText('Không tìm thấy dự án')
            self.empty_text.setText('Tạo dự án trong thư mục dự án để xem tại đây.')
            self.see_all_button.hide()
        self.count_label.setText(f'{visible} dự án')

    def _cards_count(self) -> int:
        return len(self.cards)

    def _relayout(self) -> None:
        if not hasattr(self, 'grid_layout'):
            return
        while self.grid_layout.count():
            self.grid_layout.takeAt(0)
        if not self.cards:
            return
        width = self.scroll.viewport().width() if hasattr(self, 'scroll') else self.width()
        columns = max(1, min(4, (width - 76) // 250))
        for index, card in enumerate(self.cards):
            self.grid_layout.addWidget(card, index // columns, index % columns)
        for column in range(columns):
            self.grid_layout.setColumnStretch(column, 1)

    def resizeEvent(self, event) -> None:  # noqa: N802 - Qt API
        super().resizeEvent(event)
        self._relayout()

    def showEvent(self, event) -> None:  # noqa: N802 - Qt API
        super().showEvent(event)
        self._relayout()
