from __future__ import annotations

import html
from collections.abc import Mapping
from pathlib import Path
from typing import Any

from PySide6.QtCore import QObject, QThread, Qt, Signal, Slot
from PySide6.QtGui import QFont, QTextCursor
from PySide6.QtWidgets import (
    QButtonGroup,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPlainTextEdit,
    QPushButton,
    QScrollArea,
    QStackedWidget,
    QTextBrowser,
    QVBoxLayout,
    QWidget,
)

from studio.core.ai_agent import (
    DEFAULT_MAX_TOKENS,
    MAX_MESSAGE_CHARS,
    PROVIDER_DEFAULTS,
    AgentResponse,
    AIAgent,
    ToolResult,
    normalize_provider,
)

AI_STYLE = '''
QWidget#AIPanel { background:#111122; color:#f1f2f7; }
QFrame#AIChatHeader { background:#19192e; border-bottom:1px solid #24243d; }
QLabel#AISpark { color:#ff8a00; font-size:16px; background:transparent; }
QLabel#AIChatTitle { color:#f1f2f7; font-size:18px; font-weight:700; background:transparent; }
QPushButton#AIChatToolButton { background:transparent; border:0; border-radius:6px;
  color:#8b91aa; padding:4px; font-size:14px; min-width:26px; min-height:26px; }
QPushButton#AIChatToolButton:hover { background:#2c2c4e; color:#ffffff; }
QFrame#AIChatTabBar { background:#111122; border-bottom:1px solid #24243d; }
QPushButton#AIChatTab { background:transparent; border:0; border-radius:6px;
  color:#8b91aa; padding:6px 14px; font-size:12px; }
QPushButton#AIChatTab:hover { background:#23233c; color:#d8dbe7; }
QPushButton#AIChatTab:checked { background:#2c2c4e; color:#ff8a00; font-weight:700; }
QFrame#AIWelcomeCard { background:#202039; border:1px solid #303049; border-radius:12px; }
QFrame#AIAvatar { background:#ff8a00; border-radius:14px; }
QLabel#AIAvatarText { color:#1a1a2c; font-size:13px; font-weight:700; background:transparent; }
QLabel#AIWelcomeName { color:#f1f2f7; font-size:13px; font-weight:700; background:transparent; }
QLabel#AIModelBadge { background:#24243d; color:#ff8a00; border:1px solid #303049;
  border-radius:6px; padding:2px 8px; font-size:11px; font-family:Consolas; }
QLabel#AIWelcomeText { color:#9ca2bb; font-size:12px; background:transparent; }
QPushButton#AIQuickAction { background:#1c1c33; border:1px solid #303049; border-radius:8px;
  color:#d8dbe7; padding:8px 10px; font-size:12px; text-align:left; min-height:30px; }
QPushButton#AIQuickAction:hover { background:#23233c; border-color:#575774; color:#ffffff; }
QTextBrowser#AIChatTranscript { background:#111122; border:0; border-radius:6px; padding:4px; }
QFrame#AIContextCard { background:#19192e; border:1px solid #303049; border-radius:8px; }
QLabel#AIContextIcon { color:#ff8a00; font-size:13px; background:transparent; }
QLabel#AIContextTitle { color:#f1f2f7; font-size:12px; font-weight:700; background:transparent; }
QLabel#AIContextText { color:#9ca2bb; font-size:11px; background:transparent; }
QLabel#AIContextBadge { background:#24243d; color:#ff8a00; border:1px solid #303049;
  border-radius:8px; padding:3px 8px; font-size:11px; }
QFrame#AIChatComposer { background:#19192e; border:1px solid #303049; border-radius:10px; }
QPlainTextEdit#AIChatPrompt { background:#19192e; border:0; border-radius:10px;
  padding:10px 12px; color:#f1f2f7; font-size:13px; selection-background-color:#41415b; }
QPlainTextEdit#AIChatPrompt:focus { border:0; }
QLabel#AIComposerHint { color:#6d7290; font-size:10px; background:transparent; }
QPushButton#AIComposerIcon { background:transparent; border:0; border-radius:5px;
  color:#8b91aa; padding:3px 7px; font-size:12px; }
QPushButton#AIComposerIcon:hover { background:#2c2c4e; color:#ffffff; }
QPushButton#AIProviderCompact { background:#1c1c33; border:1px solid #303049; border-radius:6px;
  color:#9ca2bb; padding:6px 10px; font-size:11px; max-width:170px; }
QPushButton#AIProviderCompact:hover { background:#23233c; border-color:#424262; color:#ffffff; }
QPushButton#AIChatSendIcon { background:#ff8a00; border:1px solid #ff8a00; border-radius:10px;
  color:#1a1a2c; font-weight:700; padding:8px 16px; min-height:30px; }
QPushButton#AIChatSendIcon:hover { background:#ff9a22; }
QPushButton#AIChatSendIcon[running="true"] { background:#e85d75; border-color:#e85d75; color:#ffffff; }
QPushButton#AIChatSendIcon[running="true"]:hover { background:#f2738a; }
QFrame#AIChatStatusBar { background:#151527; border-top:1px solid #24243d; }
QLabel#AIStatusDot { color:#56c990; font-size:11px; background:transparent; }
QLabel#AIStatusDot[state="busy"] { color:#ff9b32; }
QLabel#AIStatusDot[state="error"] { color:#ff6375; }
QLabel#AIStatusText { color:#8b91aa; font-size:11px; background:transparent; }
QLabel#AISectionTitle { color:#f1f2f7; font-size:13px; font-weight:700; }
QFrame#AIDivider { background:#24243d; max-height:1px; border:0; }
QLabel#AIToolName { color:#ff8a00; font-size:12px; font-weight:700; font-family:Consolas; background:transparent; }
QLabel#AIToolDesc { color:#9ca2bb; font-size:11px; background:transparent; }
QFrame#AIToolRow { background:#19192e; border:1px solid #24243d; border-radius:8px; }
QScrollArea { border:0; background:transparent; }
QScrollBar:vertical { background:#151527; width:10px; margin:2px; }
QScrollBar::handle:vertical { background:#3a3a56; border-radius:5px; min-height:32px; }
QScrollBar::handle:vertical:hover { background:#575774; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; width:0; }
'''

QUICK_ACTIONS: tuple[tuple[str, str], ...] = (
    ('Giải thích code', 'Giải thích mã nguồn hiện tại trong dự án giúp tôi.'),
    ('Sửa lỗi', 'Phát hiện và sửa lỗi trong dự án giúp tôi.'),
    ('Chạy thử game/app', 'Hướng dẫn chạy thử game/app trên host VM và Virtual Phone.'),
    ('Tạo hàm mới', 'Tạo một hàm Lua mới trong dự án giúp tôi.'),
    ('Tóm tắt file', 'Tóm tắt tệp đang mở giúp tôi.'),
    ('Tối ưu code', 'Gợi ý tối ưu mã nguồn trong dự án.'),
    ('Hướng dẫn', 'Hướng dẫn tôi các bước tiếp theo cho dự án này.'),
)

TOOL_ROWS: tuple[tuple[str, str], ...] = (
    ('read', 'Đọc tệp bên trong dự án, giới hạn dung lượng và chặn thoát thư mục.'),
    ('grep', 'Tìm chuỗi trong các tệp của dự án, trả về dòng và vị trí.'),
    ('glob', 'Liệt kê tệp theo mẫu, có ngân sách số kết quả.'),
    ('problems', 'Đọc danh sách lỗi build/Problems hiện tại của IDE.'),
)

_BUBBLE_ROLES: dict[str, tuple[str, str, str, str, str]] = {
    'You': ('Bạn', 'B', '#272743', '#c9cdf0', 'user'),
    'Assistant': ('QEAPP AI', '◆', '#3a2a12', '#ff8a00', 'agent'),
    'Tool': ('Công cụ', '⚙', '#18392f', '#7bd8ad', 'tool'),
    'Tool error': ('Công cụ', '⚙', '#3d2128', '#ff6375', 'tool-error'),
    'Error': ('Lỗi', '!', '#3d2128', '#ff6375', 'error'),
}


class _AIWorker(QObject):
    completed = Signal(object)
    failed = Signal(str)

    def __init__(self, agent: AIAgent, history: list[Mapping[str, Any]], context: str, api_key: str | None):
        super().__init__()
        self.agent = agent
        self.history = [dict(item) for item in history]
        self.context = context
        self.api_key = api_key

    @Slot()
    def run(self) -> None:
        key = self.api_key
        try:
            response = self.agent.respond(
                self.history,
                context=self.context,
                api_key=key,
                max_tokens=DEFAULT_MAX_TOKENS,
            )
        except Exception as exc:
            self.failed.emit(self.agent.safe_error(exc, key))
        else:
            self.completed.emit(response)
        finally:
            self.api_key = ''


class _PromptEditor(QPlainTextEdit):
    submitted = Signal()

    def __init__(self, parent: QWidget | None = None):
        super().__init__(parent)
        self.setObjectName('AIChatPrompt')
        self.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.setFixedHeight(86)
        self.setMaximumHeight(180)
        self.textChanged.connect(self._sync_height)

    def keyPressEvent(self, event) -> None:  # noqa: N802 - Qt API
        if event.key() in (Qt.Key_Return, Qt.Key_Enter) and not (
            event.modifiers() & Qt.ShiftModifier
        ):
            self.submitted.emit()
            return
        super().keyPressEvent(event)

    def _sync_height(self) -> None:
        height = int(self.document().size().height()) + 18
        self.setFixedHeight(max(86, min(180, height)))


class AIPanel(QWidget):
    status_changed = Signal(str)
    context_changed = Signal(str)
    response_received = Signal(str)
    tool_result = Signal(object)
    tool_error = Signal(str)
    error = Signal(str)
    busy_changed = Signal(bool)
    message_sent = Signal(str)

    def __init__(
        self,
        parent: QWidget | None = None,
        *,
        agent: AIAgent | None = None,
        settings_file: str | None = None,
    ):
        super().__init__(parent)
        self.setObjectName('AIPanel')
        self.setMinimumWidth(300)
        self.agent = agent or AIAgent(settings_file=settings_file)
        self._history: list[dict[str, str]] = []
        self._thread: QThread | None = None
        self._worker: _AIWorker | None = None
        self._settings_loading = True
        self.setStyleSheet(AI_STYLE)
        self._build_ui()
        self._load_settings()
        self._settings_loading = False
        self._refresh_context('Không có dự án đang mở')

    # ------------------------------------------------------------------ UI
    def _build_ui(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        header = QFrame()
        header.setObjectName('AIChatHeader')
        head_row = QHBoxLayout(header)
        head_row.setContentsMargins(14, 10, 10, 10)
        head_row.setSpacing(8)
        spark = QLabel('✦')
        spark.setObjectName('AISpark')
        title = QLabel('AI Agent')
        title.setObjectName('AIChatTitle')
        head_row.addWidget(spark)
        head_row.addWidget(title)
        head_row.addStretch(1)
        self.new_chat_button = QPushButton('⟳')
        self.new_chat_button.setObjectName('AIChatToolButton')
        self.new_chat_button.setToolTip('Cuộc trò chuyện mới')
        self.new_chat_button.setCursor(Qt.PointingHandCursor)
        self.settings_button = QPushButton('⚙')
        self.settings_button.setObjectName('AIChatToolButton')
        self.settings_button.setToolTip('Cài đặt nhà cung cấp AI')
        self.settings_button.setCursor(Qt.PointingHandCursor)
        head_row.addWidget(self.new_chat_button)
        head_row.addWidget(self.settings_button)
        root.addWidget(header)

        tabbar = QFrame()
        tabbar.setObjectName('AIChatTabBar')
        tab_row = QHBoxLayout(tabbar)
        tab_row.setContentsMargins(10, 7, 10, 7)
        tab_row.setSpacing(6)
        self.chat_tab_button = QPushButton('Chat')
        self.context_tab_button = QPushButton('Context')
        self.tools_tab_button = QPushButton('Tools')
        self.tab_group = QButtonGroup(self)
        self.tab_group.setExclusive(True)
        for index, button in enumerate(
            (self.chat_tab_button, self.context_tab_button, self.tools_tab_button)
        ):
            button.setObjectName('AIChatTab')
            button.setCheckable(True)
            button.setCursor(Qt.PointingHandCursor)
            self.tab_group.addButton(button, index)
            tab_row.addWidget(button)
        self.chat_tab_button.setChecked(True)
        tab_row.addStretch(1)
        root.addWidget(tabbar)

        self.pages = QStackedWidget()
        root.addWidget(self.pages, 1)

        self.pages.addWidget(self._build_chat_page())
        self.pages.addWidget(self._build_context_page())
        self.pages.addWidget(self._build_tools_page())

        self.chat_tab_button.clicked.connect(lambda: self.pages.setCurrentIndex(0))
        self.context_tab_button.clicked.connect(lambda: self.pages.setCurrentIndex(1))
        self.tools_tab_button.clicked.connect(lambda: self.pages.setCurrentIndex(2))
        self.settings_button.clicked.connect(self._open_settings)

        status = QFrame()
        status.setObjectName('AIChatStatusBar')
        status_row = QHBoxLayout(status)
        status_row.setContentsMargins(12, 0, 12, 0)
        status_row.setSpacing(6)
        self.status_dot = QLabel('●')
        self.status_dot.setObjectName('AIStatusDot')
        self.status_dot.setProperty('state', 'ready')
        self.status_label = QLabel('Sẵn sàng')
        self.status_label.setObjectName('AIStatusText')
        status_row.addWidget(self.status_dot)
        status_row.addWidget(self.status_label)
        status_row.addStretch(1)
        tagline = QLabel('QEAPP AI · phiên làm việc')
        tagline.setObjectName('AIStatusText')
        status_row.addWidget(tagline)
        root.addWidget(status)

        self.provider_field = self.provider_combo
        self.model_field = self.model_edit
        self.base_url_field = self.base_url_edit
        self.api_key_field = self.api_key_edit
        self.chat_transcript = self.transcript
        self.context_status = self.context_label
        self.status_display = self.status_label

        self.provider_combo.currentIndexChanged.connect(self._provider_changed)
        self.model_edit.editingFinished.connect(self._save_ui_settings)
        self.base_url_edit.editingFinished.connect(self._save_ui_settings)
        self.send_button.clicked.connect(self._on_send_clicked)
        self.cancel_button.clicked.connect(lambda: self.cancel())
        self.new_chat_button.clicked.connect(lambda: self.new_chat())
        self.input_edit.submitted.connect(lambda: self.send_message())
        self._set_busy(False)

    def _build_chat_page(self) -> QWidget:
        page = QWidget()
        page.setObjectName('AIChatPage')
        layout = QVBoxLayout(page)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(10)

        self.welcome = QFrame()
        self.welcome.setObjectName('AIWelcomeCard')
        welcome_layout = QVBoxLayout(self.welcome)
        welcome_layout.setContentsMargins(14, 14, 14, 14)
        welcome_layout.setSpacing(10)

        identity = QHBoxLayout()
        identity.setSpacing(10)
        avatar = QFrame()
        avatar.setObjectName('AIAvatar')
        avatar.setFixedSize(28, 28)
        avatar_layout = QVBoxLayout(avatar)
        avatar_layout.setContentsMargins(0, 0, 0, 0)
        avatar_text = QLabel('◆')
        avatar_text.setObjectName('AIAvatarText')
        avatar_text.setAlignment(Qt.AlignCenter)
        avatar_layout.addWidget(avatar_text)
        identity.addWidget(avatar)
        name_column = QVBoxLayout()
        name_column.setSpacing(3)
        name_row = QHBoxLayout()
        name_row.setSpacing(8)
        welcome_name = QLabel('QEAPP AI Agent')
        welcome_name.setObjectName('AIWelcomeName')
        name_row.addWidget(welcome_name)
        self.model_badge = QLabel('model')
        self.model_badge.setObjectName('AIModelBadge')
        name_row.addWidget(self.model_badge)
        name_row.addStretch(1)
        name_column.addLayout(name_row)
        greeting = QLabel(
            'Xin chào! Tôi có thể đọc dự án, tìm lỗi và đề xuất thay đổi. '
            'Mô tả những gì bạn muốn xây dựng.'
        )
        greeting.setObjectName('AIWelcomeText')
        greeting.setWordWrap(True)
        name_column.addWidget(greeting)
        identity.addLayout(name_column, 1)
        welcome_layout.addLayout(identity)

        quick_grid = QGridLayout()
        quick_grid.setHorizontalSpacing(8)
        quick_grid.setVerticalSpacing(8)
        for index, (label, prompt) in enumerate(QUICK_ACTIONS):
            button = QPushButton(label)
            button.setObjectName('AIQuickAction')
            button.setCursor(Qt.PointingHandCursor)
            button.clicked.connect(lambda _checked=False, text=prompt: self._use_quick_action(text))
            quick_grid.addWidget(button, index // 2, index % 2)
        quick_grid.setColumnStretch(0, 1)
        quick_grid.setColumnStretch(1, 1)
        welcome_layout.addLayout(quick_grid)
        layout.addWidget(self.welcome)

        self.transcript = QTextBrowser()
        self.transcript.setObjectName('AIChatTranscript')
        self.transcript.setOpenExternalLinks(False)
        self.transcript.document().setMaximumBlockCount(400)
        transcript_font = QFont('Segoe UI')
        transcript_font.setPointSize(10)
        self.transcript.setFont(transcript_font)
        layout.addWidget(self.transcript, 1)

        context_card = QFrame()
        context_card.setObjectName('AIContextCard')
        context_row = QHBoxLayout(context_card)
        context_row.setContentsMargins(10, 8, 10, 8)
        context_row.setSpacing(8)
        context_icon = QLabel('⌾')
        context_icon.setObjectName('AIContextIcon')
        context_column = QVBoxLayout()
        context_column.setSpacing(2)
        context_title = QLabel('Ngữ cảnh')
        context_title.setObjectName('AIContextTitle')
        self.context_label = QLabel('Không có dự án đang mở')
        self.context_label.setObjectName('AIContextText')
        self.context_label.setWordWrap(True)
        context_column.addWidget(context_title)
        context_column.addWidget(self.context_label)
        context_row.addWidget(context_icon)
        context_row.addLayout(context_column, 1)
        self.context_chip = QLabel('No project')
        self.context_chip.setObjectName('AIContextBadge')
        context_row.addWidget(self.context_chip)
        layout.addWidget(context_card)

        composer = QFrame()
        composer.setObjectName('AIChatComposer')
        composer_layout = QVBoxLayout(composer)
        composer_layout.setContentsMargins(8, 8, 8, 8)
        composer_layout.setSpacing(6)
        self.input_edit = _PromptEditor()
        self.input_edit.setPlaceholderText('Mô tả những gì bạn muốn xây dựng...')
        composer_layout.addWidget(self.input_edit)

        hint_row = QHBoxLayout()
        hint_row.setSpacing(4)
        attach = QPushButton('@')
        attach.setObjectName('AIComposerIcon')
        attach.setToolTip('Chèn tham chiếu ngữ cảnh')
        attach.clicked.connect(lambda: self._insert_text('@'))
        code = QPushButton('```')
        code.setObjectName('AIComposerIcon')
        code.setToolTip('Chèn khối mã Lua')
        code.clicked.connect(lambda: self._insert_code_block())
        hint_row.addWidget(attach)
        hint_row.addWidget(code)
        hint = QLabel('Shift + Enter để xuống dòng')
        hint.setObjectName('AIComposerHint')
        hint_row.addWidget(hint)
        hint_row.addStretch(1)
        composer_layout.addLayout(hint_row)

        footer = QHBoxLayout()
        footer.setSpacing(6)
        self.provider_button = QPushButton('model  ▾')
        self.provider_button.setObjectName('AIProviderCompact')
        self.provider_button.setCursor(Qt.PointingHandCursor)
        self.provider_button.clicked.connect(self._open_settings)
        footer.addWidget(self.provider_button)
        footer.addStretch(1)
        self.cancel_button = QPushButton('Huỷ')
        self.cancel_button.setEnabled(False)
        footer.addWidget(self.cancel_button)
        self.send_button = QPushButton('Gửi')
        self.send_button.setObjectName('AIChatSendIcon')
        self.send_button.setCursor(Qt.PointingHandCursor)
        footer.addWidget(self.send_button)
        composer_layout.addLayout(footer)
        layout.addWidget(composer)
        return page

    def _build_context_page(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(14, 14, 14, 14)
        layout.setSpacing(10)

        provider_title = QLabel('Nhà cung cấp AI')
        provider_title.setObjectName('AISectionTitle')
        layout.addWidget(provider_title)

        provider_grid = QGridLayout()
        provider_grid.setHorizontalSpacing(8)
        provider_grid.setVerticalSpacing(8)
        provider_grid.addWidget(QLabel('Provider'), 0, 0)
        self.provider_combo = QComboBox()
        self.provider_combo.addItem('OpenAI-compatible', 'openai')
        self.provider_combo.addItem('Ollama', 'ollama')
        self.provider_combo.addItem('Anthropic', 'anthropic')
        self.provider_combo.addItem('Gemini', 'gemini')
        provider_grid.addWidget(self.provider_combo, 0, 1)
        provider_grid.addWidget(QLabel('Model'), 1, 0)
        self.model_edit = QLineEdit()
        self.model_edit.setPlaceholderText('Tên model')
        provider_grid.addWidget(self.model_edit, 1, 1)
        provider_grid.addWidget(QLabel('Base URL'), 2, 0)
        self.base_url_edit = QLineEdit()
        self.base_url_edit.setPlaceholderText('https://provider.example/v1')
        provider_grid.addWidget(self.base_url_edit, 2, 1)
        provider_grid.addWidget(QLabel('API key'), 3, 0)
        self.api_key_edit = QLineEdit()
        self.api_key_edit.setEchoMode(QLineEdit.Password)
        self.api_key_edit.setPlaceholderText('Chỉ giữ trong phiên; môi trường cũng được hỗ trợ')
        provider_grid.addWidget(self.api_key_edit, 3, 1)
        provider_grid.setColumnStretch(1, 1)
        layout.addLayout(provider_grid)

        key_note = QLabel('API key không bao giờ được lưu vào cấu hình IDE.')
        key_note.setObjectName('AIContextText')
        key_note.setWordWrap(True)
        layout.addWidget(key_note)

        divider = QFrame()
        divider.setObjectName('AIDivider')
        divider.setFrameShape(QFrame.HLine)
        layout.addWidget(divider)

        report_title = QLabel('Ngữ cảnh dự án')
        report_title.setObjectName('AISectionTitle')
        layout.addWidget(report_title)
        self.context_report_label = QLabel('Chưa có dự án')
        self.context_report_label.setObjectName('AIContextText')
        self.context_report_label.setWordWrap(True)
        layout.addWidget(self.context_report_label)
        layout.addStretch(1)
        return page

    def _build_tools_page(self) -> QWidget:
        page = QWidget()
        outer = QVBoxLayout(page)
        outer.setContentsMargins(14, 14, 14, 14)
        outer.setSpacing(10)
        title = QLabel('Công cụ an toàn')
        title.setObjectName('AISectionTitle')
        outer.addWidget(title)
        note = QLabel('Agent chỉ được phép đọc dự án; không chạy shell và không tự sửa tệp.')
        note.setObjectName('AIContextText')
        note.setWordWrap(True)
        outer.addWidget(note)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        host = QWidget()
        tools_layout = QVBoxLayout(host)
        tools_layout.setContentsMargins(0, 0, 6, 0)
        tools_layout.setSpacing(8)
        for name, description in TOOL_ROWS:
            row = QFrame()
            row.setObjectName('AIToolRow')
            row_layout = QVBoxLayout(row)
            row_layout.setContentsMargins(10, 8, 10, 8)
            row_layout.setSpacing(3)
            label = QLabel(name)
            label.setObjectName('AIToolName')
            desc = QLabel(description)
            desc.setObjectName('AIToolDesc')
            desc.setWordWrap(True)
            row_layout.addWidget(label)
            row_layout.addWidget(desc)
            tools_layout.addWidget(row)
        tools_layout.addStretch(1)
        scroll.setWidget(host)
        outer.addWidget(scroll, 1)
        return page

    # ------------------------------------------------------------- settings
    def _open_settings(self) -> None:
        self.context_tab_button.setChecked(True)
        self.pages.setCurrentIndex(1)

    def _use_quick_action(self, prompt: str) -> None:
        self.chat_tab_button.setChecked(True)
        self.pages.setCurrentIndex(0)
        self.input_edit.setPlainText(prompt)
        self.input_edit.setFocus()

    def _insert_text(self, text: str) -> None:
        self.input_edit.insertPlainText(text)
        self.input_edit.setFocus()

    def _insert_code_block(self) -> None:
        self._insert_text('\n```lua\n\n```\n')

    def _on_send_clicked(self) -> None:
        if self.is_busy:
            self.cancel()
        else:
            self.send_message()

    def _update_provider_button(self) -> None:
        model = self.model_edit.text().strip() or self.provider_combo.currentText()
        metrics = self.provider_button.fontMetrics()
        text = metrics.elidedText(f'{model}  ▾', Qt.ElideMiddle, 150)
        self.provider_button.setText(text)
        self.provider_button.setToolTip(f'{self.provider_combo.currentText()} · {model}')
        if hasattr(self, 'model_badge'):
            self.model_badge.setText(model)

    def _load_settings(self) -> None:
        settings = self.agent.settings
        index = self.provider_combo.findData(settings.provider)
        if index >= 0:
            self.provider_combo.setCurrentIndex(index)
        self.model_edit.setText(settings.model)
        self.base_url_edit.setText(settings.base_url)
        self.api_key_edit.clear()
        self._settings_loading = False
        self._update_provider_button()
        self._set_busy(False)

    def _provider_changed(self, *_args: Any) -> None:
        if self._settings_loading:
            return
        provider = self.provider_combo.currentData()
        try:
            provider = normalize_provider(provider)
            model, base_url = PROVIDER_DEFAULTS[provider]
            self.model_edit.setText(model)
            self.base_url_edit.setText(base_url)
            self._save_ui_settings()
        except Exception as exc:
            self._show_error(self.agent.safe_error(exc))

    def _save_ui_settings(self) -> bool:
        if self._settings_loading:
            return True
        try:
            provider = normalize_provider(self.provider_combo.currentData())
            self.agent.configure(
                provider=provider,
                model=self.model_edit.text().strip(),
                base_url=self.base_url_edit.text().strip(),
            )
            self._update_provider_button()
            return True
        except Exception as exc:
            self._show_error(self.agent.safe_error(exc))
            return False

    # -------------------------------------------------------------- context
    def set_project(self, workspace: Any | None) -> None:
        try:
            self.agent.set_project(workspace)
        except Exception as exc:
            self._show_error(self.agent.safe_error(exc))
            return
        if workspace is None:
            self._refresh_context('Không có dự án đang mở')
        else:
            self._refresh_context('Ngữ cảnh dự án đã sẵn sàng')

    def append_context(self, text: str, label: str = 'integration', *, source: str | None = None) -> str | None:
        try:
            self.agent.append_context(text, label, source=source)
        except Exception as exc:
            self._show_error(self.agent.safe_error(exc))
            return None
        self._refresh_context('Đã cập nhật ngữ cảnh')
        return self.agent.build_context()

    def set_problems_callback(self, callback: Any) -> None:
        try:
            self.agent.set_problems_callback(callback)
            self.set_status('Đã kết nối Problems')
        except Exception as exc:
            self._show_error(self.agent.safe_error(exc))

    def _workspace_chip(self) -> str:
        workspace = self.agent.workspace
        if workspace is None:
            return 'No project'
        root = getattr(workspace, 'root', None) or getattr(workspace, 'path', None)
        if root is None:
            return 'Project'
        return Path(str(root)).name or 'Project'

    def _refresh_context(self, prefix: str) -> None:
        try:
            report = self.agent.context_report()
        except Exception as exc:
            self._show_error(self.agent.safe_error(exc))
            return
        suffix = ' · đã cắt bớt' if report.truncated else ''
        text = f'{prefix} · {report.file_count} tệp · {report.total_bytes} byte{suffix}'
        self.context_label.setText(text)
        self.context_chip.setText(self._workspace_chip())
        if hasattr(self, 'context_report_label'):
            self.context_report_label.setText(text)
        self.context_changed.emit(text)

    @property
    def workspace(self) -> Any:
        return self.agent.workspace

    @property
    def status_text(self) -> str:
        return self.status_label.text()

    @property
    def is_busy(self) -> bool:
        return self._thread is not None and self._thread.isRunning()

    def set_status(self, text: str) -> None:
        value = str(text)
        self.status_label.setText(value)
        lowered = value.casefold()
        if any(token in lowered for token in ('sending', 'finishing', 'cancelling', 'đang')):
            state = 'busy'
        elif any(token in lowered for token in ('error', 'lỗi', 'failed')):
            state = 'error'
        else:
            state = 'ready'
        if self.status_dot.property('state') != state:
            self.status_dot.setProperty('state', state)
            self.status_dot.style().unpolish(self.status_dot)
            self.status_dot.style().polish(self.status_dot)
        self.status_changed.emit(value)

    def _set_busy(self, value: bool) -> None:
        self.cancel_button.setEnabled(value)
        self.provider_combo.setEnabled(not value)
        self.model_edit.setEnabled(not value)
        self.base_url_edit.setEnabled(not value)
        self.api_key_edit.setEnabled(not value)
        running = 'true' if value else 'false'
        if self.send_button.property('running') != running:
            self.send_button.setProperty('running', running)
            self.send_button.style().unpolish(self.send_button)
            self.send_button.style().polish(self.send_button)
        self.send_button.setText('Dừng' if value else 'Gửi')
        self.busy_changed.emit(value)

    # ------------------------------------------------------------ transcript
    def _bubble_html(self, label: str, text: str) -> str:
        title, glyph, avatar_bg, avatar_fg, _role = _BUBBLE_ROLES.get(
            label, (label, '·', '#272743', '#c9cdf0', 'user')
        )
        body = html.escape(str(text))
        return (
            '<table width="100%" cellspacing="0" cellpadding="0" style="margin-bottom:10px;"><tr>'
            f'<td width="36" valign="top"><div style="background:{avatar_bg};color:{avatar_fg};'
            'border-radius:14px;width:28px;height:28px;font-size:13px;font-weight:700;'
            f'text-align:center;line-height:28px;margin-right:6px;">{glyph}</div></td>'
            '<td valign="top">'
            f'<div style="color:#8b91aa;font-size:11px;font-weight:700;margin-bottom:3px;">{title}</div>'
            '<div style="background:#202039;border:1px solid #303049;border-radius:8px;'
            'padding:8px 10px;color:#d8dbe7;font-size:13px;white-space:pre-wrap;word-wrap:break-word;">'
            f'{body}</div></td></tr></table>'
        )

    def _append_transcript(self, label: str, text: str) -> None:
        value = str(text).replace('\x00', '')
        if len(value) > MAX_MESSAGE_CHARS * 2:
            value = value[:MAX_MESSAGE_CHARS * 2] + '\n[đã cắt bớt nội dung]'
        self.transcript.moveCursor(QTextCursor.End)
        self.transcript.append(self._bubble_html(label, value))
        self.transcript.moveCursor(QTextCursor.End)
        self.welcome.hide()

    def _show_error(self, text: str) -> None:
        value = str(text)
        self._append_transcript('Error', value)
        self.error.emit(value)
        self.set_status(value)

    def _trim_history(self) -> None:
        if len(self._history) > 20:
            del self._history[:-20]

    # ----------------------------------------------------------------- chat
    def send_message(self, message: str | None = None) -> bool:
        if self.is_busy:
            self.set_status('Yêu cầu AI đang chạy')
            return False
        value = self.input_edit.toPlainText() if message is None else message
        if not isinstance(value, str) or not value.strip() or '\x00' in value:
            self.set_status('Hãy nhập nội dung trước')
            return False
        value = value.strip()
        if len(value) > MAX_MESSAGE_CHARS:
            self.set_status('Nội dung quá lớn')
            return False
        if not self._save_ui_settings():
            return False
        try:
            context = self.agent.build_context()
        except Exception as exc:
            self._show_error(self.agent.safe_error(exc))
            return False
        api_key = self.api_key_edit.text().strip() or None
        self.api_key_edit.clear()
        if api_key:
            value = value.replace(api_key, '[REDACTED]')
        self._history.append({'role': 'user', 'content': value})
        self._trim_history()
        self._append_transcript('You', value)
        self.message_sent.emit(value)
        self.input_edit.clear()
        thread = QThread(self)
        worker = _AIWorker(self.agent, self._history, context, api_key)
        worker.moveToThread(thread)
        thread.started.connect(worker.run)
        worker.completed.connect(self._response_ready)
        worker.failed.connect(self._request_failed)
        worker.completed.connect(thread.quit)
        worker.failed.connect(thread.quit)
        worker.completed.connect(worker.deleteLater)
        worker.failed.connect(worker.deleteLater)
        thread.finished.connect(self._thread_finished)
        self._thread = thread
        self._worker = worker
        self._set_busy(True)
        self.set_status('Đang gửi…')
        thread.start()
        return True

    def _response_ready(self, response: AgentResponse) -> None:
        if not isinstance(response, AgentResponse):
            self._request_failed('AI worker returned an invalid response')
            return
        for result in response.tool_results:
            if isinstance(result, ToolResult):
                self.tool_result.emit(result)
                if result.ok:
                    self._append_transcript('Tool', result.as_text())
                else:
                    value = result.as_text()
                    self.tool_error.emit(value)
                    self._append_transcript('Tool error', value)
        for parse_error in response.parse_errors:
            value = 'Tool parser: ' + parse_error
            self.tool_error.emit(value)
            self._append_transcript('Tool error', value)
        text = response.text.strip()
        if text:
            self._history.append({'role': 'assistant', 'content': text})
            self._trim_history()
            self._append_transcript('Assistant', text)
            self.response_received.emit(text)
        self.set_status('Đang hoàn tất…')

    def _request_failed(self, message: str) -> None:
        value = self.agent.safe_error(RuntimeError(message))
        self._append_transcript('Error', value)
        self.error.emit(value)
        self.set_status(value)

    def _thread_finished(self) -> None:
        thread = self._thread
        if thread is not None:
            thread.deleteLater()
        self._thread = None
        self._worker = None
        self._set_busy(False)
        if self.status_label.text() in {'Đang gửi…', 'Đang hoàn tất…'}:
            self.set_status('Sẵn sàng')

    def cancel(self) -> bool:
        if not self.is_busy:
            self.set_status('Không có yêu cầu AI nào đang chạy')
            return False
        cancelled = self.agent.cancel()
        if cancelled:
            self.set_status('Đang huỷ…')
        return cancelled

    def new_chat(self) -> None:
        if self.is_busy:
            self.set_status('Hãy huỷ yêu cầu hiện tại trước khi bắt đầu trò chuyện mới')
            return
        self._history.clear()
        self.agent.clear_context()
        self.transcript.clear()
        self.input_edit.clear()
        self.api_key_edit.clear()
        self.welcome.show()
        self.chat_tab_button.setChecked(True)
        self.pages.setCurrentIndex(0)
        self.set_status('Cuộc trò chuyện mới')
        self._refresh_context(
            'Ngữ cảnh dự án đã sẵn sàng' if self.agent.workspace is not None else 'Không có dự án đang mở'
        )

    def closeEvent(self, event: Any) -> None:  # noqa: N802 - Qt API
        self.api_key_edit.clear()
        if self.is_busy:
            self.cancel()
            event.ignore()
            return
        super().closeEvent(event)
