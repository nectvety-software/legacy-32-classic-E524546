"""QEAPP Studio M2 desktop IDE: project editor + original signer CLI + host preview.

The UI is intentionally thin. No duplicate package parser, no signature bypass,
no device executable runtime claims. Qt is the only optional desktop dependency.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

from PySide6.QtCore import QObject, Qt, QThread, Signal, Slot
from PySide6.QtGui import QAction, QColor, QFont, QPixmap, QSyntaxHighlighter, QTextCharFormat, QTextCursor, QTextDocument
from PySide6.QtWidgets import (
    QApplication, QComboBox, QFileDialog, QFormLayout, QGroupBox,
    QHBoxLayout, QInputDialog, QLabel, QLineEdit, QMainWindow, QMessageBox,
    QPlainTextEdit, QPushButton, QSizePolicy, QScrollArea, QSpinBox, QSplitter, QStatusBar,
    QTabWidget, QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget, QFrame, QStackedWidget, QCheckBox, QStyle
)

from studio.core import commands
from studio.core.config import StudioConfig
from studio.core.jobs import JobRunner
from studio.core.paths import ensure_user_dirs, is_default_projects_root, project_dirname
from studio.core.project_library import ProjectLibrary, default_projects_root
from studio.core.replays import ReplayEvent, ReplayError, parse as parse_replay, serialize as serialize_replay
from studio.core.workspace import Document, Workspace, WorkspaceError
from studio.core.project_search import search_project
from studio.gui.ai_panel import AIPanel
from studio.gui.code_editor import EditorBase
from studio.gui.home_page import HomePage

STYLE = '''
QMainWindow, QWidget { background-color:#141927; color:#dae5fa; font-size:12px; }
QToolBar, QMenuBar, QMenu, QStatusBar { background-color:#20283b; color:#d5e5fc; }
QTreeWidget, QPlainTextEdit, QTabWidget::pane, QLineEdit, QComboBox {
    background-color:#101728; color:#d8ecfd; border:1px solid #34435d;
    selection-background-color:#344f82; }
QTabBar::tab { padding:8px 15px; background:#26324a; margin-right:2px; }
QTabBar::tab:selected { background:#3f477b; color:#fcfcff; }
QPushButton { padding:6px; background:#304477; border:1px solid #4d68a5; border-radius:4px; }
QPushButton:hover { background:#475f9d; }
QPushButton:disabled { color:#697b9c; background:#232c41; }
QLabel#notice { color:#f5ce70; }
'''


class SimpleHighlighter(QSyntaxHighlighter):
    def __init__(self, document, ext: str):
        super().__init__(document)
        self.ext = ext
        self.strings = QTextCharFormat()
        self.strings.setForeground(QColor('#98d6a6'))
        self.keys = QTextCharFormat()
        self.keys.setForeground(QColor('#9eb9ff'))
        self.comment = QTextCharFormat()
        self.comment.setForeground(QColor('#7384a4'))

    def highlightBlock(self, text: str):
        import re
        for match in re.finditer(r'"(?:[^"\\]|\\.)*"', text):
            self.setFormat(match.start(), len(match.group()), self.strings)
        if self.ext == '.json':
            for match in re.finditer(r'"[A-Za-z_][\w]*"\s*:', text):
                self.setFormat(match.start(), len(match.group()) - 1, self.keys)
        elif self.ext in ('.lua', '.py'):
            for match in re.finditer(r'\b(?:function|local|end|if|then|else|return|def|class|import|from)\b', text):
                self.setFormat(match.start(), len(match.group()), self.keys)
            for match in re.finditer(r'#[^\n]*' if self.ext == '.py' else r'--[^\n]*', text):
                self.setFormat(match.start(), len(match.group()), self.comment)


class CodeEditor(EditorBase):
    def __init__(self, doc: Document):
        super().__init__()
        self.relative_path = doc.relative_path
        self.disk_sha = doc.sha256
        self.setPlainText(doc.text)
        font = QFont('Consolas')
        font.setStyleHint(QFont.Monospace)
        font.setPointSize(10)
        self.setFont(font)
        self.setTabStopDistance(self.fontMetrics().horizontalAdvance(' ') * 4)
        self._margin_changed()
        self.highlighter = SimpleHighlighter(self.document(), Path(doc.relative_path).suffix.lower())
        self.document().setModified(False)


class JobWorker(QObject):
    output = Signal(str)
    finished = Signal(int, str, str)

    def __init__(self, runner: JobRunner, spec: commands.JobSpec):
        super().__init__()
        self.runner, self.spec = runner, spec

    @Slot()
    def run(self):
        code = -1
        try:
            code = self.runner.execute(self.spec, self.output.emit, cwd=commands.ROOT)
        except Exception as exc:
            # The only paths inside worker errors originate from fixed tool code;
            # still redact the user-selected private signing-key path.
            text = str(exc)
            for secret in self.spec.secrets:
                text = text.replace(secret, '[REDACTED]')
            self.output.emit('Job error: ' + text + '\n')
        preview = ''
        if code == 0 and self.spec.preview and self.spec.preview.is_file():
            preview = str(self.spec.preview)
        self.finished.emit(code, self.spec.label, preview)


class StudioWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('QEAPP Studio v0.7.4 · Developer Workbench')
        self.resize(1250, 790)
        self.setMinimumSize(900, 600)
        self.setStyleSheet(STYLE)
        try:
            ensure_user_dirs()
        except OSError:
            pass
        self.config = StudioConfig()
        configured_root = self.config.data.get('projects_root', '').strip()
        if configured_root and (
            is_default_projects_root(configured_root)
            or not Path(configured_root).expanduser().is_dir()
        ):
            self.config.update(projects_root='')
            configured_root = ''
        try:
            projects_root = Path(configured_root).expanduser() if configured_root else default_projects_root()
            self.project_library = ProjectLibrary(projects_root, self.config.data['recent_projects'])
        except (OSError, ValueError):
            self.project_library = ProjectLibrary(default_projects_root(), self.config.data['recent_projects'])
        self.workspace: Workspace | None = None
        self.runner = JobRunner()
        self.thread: QThread | None = None
        self.worker: JobWorker | None = None
        self.actions_when_idle: list[QAction | QPushButton] = []
        self.editors: dict[str, CodeEditor] = {}
        self.image_previews: dict[str, QWidget] = {}
        self.replay_events: list[ReplayEvent] = []
        self.replay_dirty = False
        self._ai_problem_snapshot: list[dict[str, str]] = []
        self._pending_emulator_script: Path | None = None
        self._setup_ui()

    def _setup_ui(self):
        from studio.gui.theme import APP_STYLE
        from studio.gui.virtual_phone import VirtualPhone
        self.setWindowTitle('QEAPP Studio 0.7.6 — Development Workspace')
        self.resize(1510, 920)
        self.setMinimumSize(1060, 670)
        self.setStyleSheet(APP_STYLE)
        menu_file = self.menuBar().addMenu('&File')
        menu_edit = self.menuBar().addMenu('&Edit')
        menu_view = self.menuBar().addMenu('&View')
        menu_run = self.menuBar().addMenu('&Run')
        menu_tools = self.menuBar().addMenu('&Tools')
        menu_help = self.menuBar().addMenu('&Help')
        def action(menu, title, fn, shortcut=None):
            act = QAction(title, self)
            act.triggered.connect(fn)
            if shortcut: act.setShortcut(shortcut)
            menu.addAction(act)
            return act
        action(menu_file, 'New Text App…', lambda: self.create_project('text'))
        action(menu_file, 'New Web App…', lambda: self.create_project('web'))
        action(menu_file, 'New Standard Lua App (Beta)…', lambda: self.create_project('lua-standard'))
        action(menu_file, 'New Lua App (Beta)…', lambda: self.create_project('lua'))
        action(menu_file, 'New Lua Snake Game (Beta)…', lambda: self.create_project('lua-snake'))
        action(menu_file, 'New Pixel Sprite Game (Beta)…', lambda: self.create_project('lua-sprite'))
        action(menu_file, 'New Chess Game (Beta)…', lambda: self.create_project('lua-chess'))
        menu_file.addSeparator()
        action(menu_file, 'Home / Projects', self.show_home, 'Ctrl+Shift+H')
        action(menu_file, 'Open Project…', self.choose_project, 'Ctrl+O')
        action(menu_file, 'Quick Open File…', self.quick_open, 'Ctrl+P')
        action(menu_file, 'New Source File…', self.new_file)
        action(menu_file, 'Insert Pixel Sprite PNG…', self.insert_pixel_sprite)
        action(menu_file, 'New Folder…', self.new_folder)
        action(menu_file, 'Save', self.save_current, 'Ctrl+S')
        action(menu_file, 'Refresh Explorer', self.refresh_tree, 'F5')
        action(menu_edit, 'Find', self.find_in_editor, 'Ctrl+F')
        action(menu_edit, 'Replace', self.show_replace, 'Ctrl+H')
        action(menu_edit, 'Go to Line…', self.go_to_line, 'Ctrl+G')
        action(menu_edit, 'Search Entire Project…', self.show_project_search, 'Ctrl+Shift+F')
        menu_file.addSeparator()
        action(menu_file, 'Close Project', self.close_project)

        self.act_validate = action(menu_run, 'Validate', self.validate_project, 'F6')
        self.act_build = action(menu_run, 'Build signed .qeapp', self.build_project, 'F7')
        self.act_lua = action(menu_run, 'Render Lua Replay', self.preview_lua, 'F8')
        self.act_live = action(menu_run, 'Run Virtual Phone (Lua)', self.launch_emulator, 'F9')
        action(menu_run, 'Inspect signed QEAPP/2', self.inspect_package)
        self.act_inspect = menu_run.actions()[-1]
        self.act_doctor = action(menu_run, 'Firmware doctor', self.doctor)
        self.act_beta_check = action(menu_run, 'Lua Beta Device Preflight…', self.beta_preflight)
        self.act_test = action(menu_run, 'Run host regression tests', self.run_tests)
        menu_run.addSeparator()
        self.act_stop = action(menu_run, 'STOP active job / device', self.stop_job, 'Shift+F5')
        self.act_stop.setEnabled(True)
        action(menu_view, 'Toggle Explorer', self.toggle_explorer, 'Ctrl+B')
        action(menu_view, 'Toggle Bottom Panel', self.toggle_bottom_panel, 'Ctrl+J')
        action(menu_view, 'Show Virtual Phone', self.show_virtual_phone, 'Ctrl+Shift+V')
        action(menu_view, 'Command Palette', self.command_palette, 'Ctrl+Shift+P')
        action(menu_tools, 'AI Agent', self.show_ai_agent, 'Ctrl+Shift+I')
        action(menu_tools, 'GUI & Environment Diagnostics…', self.show_diagnostics, 'Ctrl+Shift+D')
        action(menu_tools, 'Settings — Build / Firmware…', self.open_firmware_settings)
        action(menu_tools, 'Safe Repair Mode…', self.safe_repair_mode)
        action(menu_tools, 'Reset IDE Layout (confirm)…', self.reset_layout)
        action(menu_run, 'Restart Lua Host VM', self.restart_virtual_machine, 'Ctrl+Shift+R')
        action(menu_help, 'About QEAPP Studio / Virtual Phone', self.about_studio)
        action(menu_help, 'Export Diagnostic Bundle…', self.export_diagnostic_bundle)

        # Main four-pane workbench: activity rail / Explorer / editor / device.
        self.activity = QFrame()
        self.activity.setObjectName('ActivityRail')
        activity_layout = QVBoxLayout(self.activity)
        activity_layout.setContentsMargins(0, 7, 0, 7)
        activity_layout.setSpacing(7)
        self.activity.setFixedWidth(49)
        for icon, label, callback in (
            ('E', 'Explorer', lambda: self._select_activity('explorer')),
            ('S', 'Search', lambda: self.show_project_search()),
            ('D', 'Device', self.show_virtual_phone),
            ('A', 'AI Agent', self.show_ai_agent),
            ('B', 'Build', self.validate_project),
        ):
            button = QPushButton('')
            standard_icons = {
                'Explorer': QStyle.StandardPixmap.SP_DirIcon,
                'Search': QStyle.StandardPixmap.SP_FileDialogContentsView,
                'Device': QStyle.StandardPixmap.SP_ComputerIcon,
                'AI Agent': QStyle.StandardPixmap.SP_FileDialogInfoView,
                'Build': QStyle.StandardPixmap.SP_MediaPlay,
            }
            button.setIcon(self.style().standardIcon(standard_icons[label]))
            button.setAccessibleName(label)
            button.setObjectName('ActivityButton')
            button.setToolTip(label)
            button.setCheckable(label in ('Explorer', 'Search'))
            button.clicked.connect(callback)
            activity_layout.addWidget(button)
            if label == 'Explorer': self.explorer_toggle = button
            if label == 'Search': self.search_toggle = button
        activity_layout.addStretch()

        self.explorer_panel = QFrame()
        self.explorer_panel.setObjectName('ExplorerDock')
        explorer_layout = QVBoxLayout(self.explorer_panel)
        explorer_layout.setContentsMargins(9, 7, 6, 6)
        explorer_layout.setSpacing(7)
        tree_title = QHBoxLayout()
        panel_title = QLabel('EXPLORER')
        panel_title.setObjectName('PanelTitle')
        tree_title.addWidget(panel_title)
        tree_title.addStretch()
        refresh = QPushButton('R')
        refresh.setToolTip('Refresh F5')
        refresh.setFixedSize(27, 25)
        refresh.clicked.connect(self.refresh_tree)
        tree_title.addWidget(refresh)
        explorer_layout.addLayout(tree_title)
        self.project_filter = QLineEdit()
        self.project_filter.setPlaceholderText('Filter project files…')
        self.project_filter.textChanged.connect(self.filter_tree)
        explorer_layout.addWidget(self.project_filter)
        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        self.tree.itemDoubleClicked.connect(self.open_tree_file)
        self.left_pages = QStackedWidget()
        explorer_page = QWidget()
        explorer_stack = QVBoxLayout(explorer_page)
        explorer_stack.setContentsMargins(0, 0, 0, 0)
        explorer_stack.addWidget(self.tree)
        self.left_pages.addWidget(explorer_page)
        search_page = QWidget()
        search_layout = QVBoxLayout(search_page)
        search_layout.setContentsMargins(0, 0, 0, 0)
        self.project_query = QLineEdit()
        self.project_query.setPlaceholderText('Search text in all source files')
        self.project_query.returnPressed.connect(self.search_all_files)
        search_layout.addWidget(self.project_query)
        search_options = QHBoxLayout()
        self.search_case = QCheckBox('Match case')
        search_options.addWidget(self.search_case)
        self.btn_search = QPushButton('Search')
        self.btn_search.clicked.connect(self.search_all_files)
        search_options.addWidget(self.btn_search)
        search_layout.addLayout(search_options)
        self.search_count = QLabel('Ctrl+Shift+F · literal bounded search')
        self.search_count.setWordWrap(True)
        self.search_count.setObjectName('Subtle')
        search_layout.addWidget(self.search_count)
        self.search_results = QTreeWidget()
        self.search_results.setHeaderHidden(True)
        self.search_results.itemActivated.connect(self.open_search_result)
        self.search_results.itemDoubleClicked.connect(self.open_search_result)
        search_layout.addWidget(self.search_results, 1)
        self.left_pages.addWidget(search_page)
        explorer_layout.addWidget(self.left_pages, 1)
        self.lbl_project = QLabel('No project open')
        self.lbl_project.setWordWrap(True)
        self.lbl_project.setObjectName('Subtle')
        explorer_layout.addWidget(self.lbl_project)

        editor_panel = QWidget()
        editor_panel_layout = QVBoxLayout(editor_panel)
        editor_panel_layout.setContentsMargins(0, 0, 0, 0)
        editor_panel_layout.setSpacing(0)
        topbar = QFrame()
        topbar.setObjectName('WorkbenchBar')
        topbar_layout = QHBoxLayout(topbar)
        topbar_layout.setContentsMargins(10, 5, 10, 5)
        topbar_layout.setSpacing(7)
        brand = QLabel('QEAPP  /  STUDIO')
        brand.setObjectName('PanelTitle')
        topbar_layout.addWidget(brand)
        def bar_button(title, callback, obj=None):
            b = QPushButton(title)
            if obj: b.setObjectName(obj)
            b.clicked.connect(callback)
            topbar_layout.addWidget(b)
            return b
        self.btn_home = bar_button('⌂  Home', self.show_home)
        topbar_layout.addStretch()
        self.btn_open = bar_button('Open', self.choose_project)
        self.btn_save = bar_button('Save', self.save_current)
        self.btn_validate = bar_button('Validate', self.validate_project)
        self.btn_build = bar_button('Build', self.build_project)
        self.btn_run = bar_button('Run F9', self.launch_emulator, 'PrimaryButton')
        self.btn_stop = bar_button('Stop', self.stop_job)
        editor_panel_layout.addWidget(topbar)
        self.editor_splitter = QSplitter(Qt.Vertical)
        editor_top = QWidget()
        editor_top_layout = QVBoxLayout(editor_top)
        editor_top_layout.setContentsMargins(0, 0, 0, 0)
        editor_top_layout.setSpacing(0)
        self.welcome = QLabel('QEAPP STUDIO  •  CREATE OR OPEN A PROJECT\n\n'
                              'New Lua game → F9 Run virtual phone\n'
                              'F6 Validate  •  F7 Build signed QEAPP/2\n'
                              'F8 Export deterministic replay PNG\n\n'
                              'Lua game packaging requires experimental firmware vqeaf_lua_beta.')
        self.welcome.setAlignment(Qt.AlignCenter)
        self.welcome.setWordWrap(True)
        self.welcome.setStyleSheet('color:#9ca2bb; font-size:14px; padding:12px;')
        editor_top_layout.addWidget(self.welcome)
        self.tabs = QTabWidget()
        self.tabs.setTabsClosable(True)
        self.tabs.tabCloseRequested.connect(self.close_tab)
        self.tabs.currentChanged.connect(self._on_tab_changed)
        editor_top_layout.addWidget(self.tabs, 1)
        search_row = QHBoxLayout()
        self.editor_find = QLineEdit()
        self.editor_find.setPlaceholderText('Find in current tab…')
        self.editor_find.returnPressed.connect(self.find_next)
        self.editor_find.hide()
        next_button = QPushButton('Find Next')
        next_button.clicked.connect(self.find_next)
        next_button.hide()
        self._find_next_btn = next_button
        self.editor_replace = QLineEdit()
        self.editor_replace.setPlaceholderText('Replace with…')
        self.editor_replace.hide()
        self.replace_button = QPushButton('Replace')
        self.replace_button.clicked.connect(self.replace_next)
        self.replace_button.hide()
        search_row.addWidget(self.editor_find)
        search_row.addWidget(next_button)
        search_row.addWidget(self.editor_replace)
        search_row.addWidget(self.replace_button)
        editor_top_layout.addLayout(search_row)
        self.editor_splitter.addWidget(editor_top)
        bottom = QFrame()
        bottom.setObjectName('BottomPanel')
        bottom_layout = QVBoxLayout(bottom)
        bottom_layout.setContentsMargins(8, 4, 8, 5)
        bottom_layout.setSpacing(3)
        bottom_head = QHBoxLayout()
        bottom_title = QLabel('TERMINAL   ·   OUTPUT   ·   BUILD LOG')
        bottom_title.setObjectName('PanelTitle')
        bottom_head.addWidget(bottom_title)
        bottom_head.addStretch()
        self.bottom_clear = QPushButton('Clear')
        self.bottom_clear.clicked.connect(lambda: self.logs.clear())
        bottom_head.addWidget(self.bottom_clear)
        bottom_layout.addLayout(bottom_head)
        self.logs = QPlainTextEdit()
        self.logs.setReadOnly(True)
        self.logs.document().setMaximumBlockCount(1800)
        font = QFont('Consolas', 10)
        font.setStyleHint(QFont.Monospace)
        self.logs.setFont(font)
        self.bottom_tabs = QTabWidget()
        self.bottom_tabs.setDocumentMode(True)
        self.bottom_tabs.addTab(self.logs, 'OUTPUT')
        self.problems = QTreeWidget()
        self.problems.setHeaderLabels(['Source / Job', 'Diagnostic'])
        self.problems.header().setStretchLastSection(True)
        self.problems.itemDoubleClicked.connect(self.open_problem)
        self.bottom_tabs.addTab(self.problems, 'PROBLEMS')
        bottom_layout.addWidget(self.bottom_tabs, 1)
        self.bottom_panel = bottom
        self.editor_splitter.addWidget(bottom)
        self.editor_splitter.setSizes([640, 110])
        editor_panel_layout.addWidget(self.editor_splitter, 1)

        self.right_tabs = QTabWidget()
        self.right_tabs.setDocumentMode(True)
        self.right_tabs.setMinimumWidth(230)
        self.ai_panel = AIPanel(self)
        self.ai_panel.set_problems_callback(self._ai_problems)
        self.ai_panel.status_changed.connect(lambda text: self._log('AI: ' + text))
        self.right_tabs.addTab(self.ai_panel, 'AI AGENT')
        self.device = VirtualPhone(self)
        self.device.launch_requested.connect(self.launch_emulator)
        self.device.log.connect(self._log)
        self.device.guest_problem.connect(self.on_guest_problem)
        self.device.metric_sample.connect(self.update_vm_inspector)
        self.device.state_changed.connect(lambda _state: self._sync_stop_controls())
        # Compact dock; live shell is reparented into PhoneWindow on F9 / Open.
        self.device_dock = QWidget()
        dock_l = QVBoxLayout(self.device_dock)
        dock_l.setContentsMargins(6, 6, 6, 6)
        dock_open = QPushButton('Open Virtual Phone window')
        dock_open.setObjectName('PrimaryButton')
        dock_open.clicked.connect(self.show_virtual_phone)
        dock_l.addWidget(dock_open)
        dock_note = QLabel('F9 Run opens a floating phone window (LuaS30-style).')
        dock_note.setObjectName('Subtle')
        dock_note.setWordWrap(True)
        dock_l.addWidget(dock_note)
        dock_l.addWidget(self.device, 1)
        self.right_tabs.addTab(self.device_dock, 'VIRTUAL PHONE')
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_layout.setContentsMargins(9, 7, 9, 7)
        right_layout.addWidget(QLabel('REPLAY & HOST RENDER'))
        notice = QLabel('Deterministic host previews. Lua firmware is beta;'
                        ' virtual device does not emulate ESP32-S3 peripherals.')
        notice.setObjectName('notice')
        notice.setWordWrap(True)
        right_layout.addWidget(notice)
        self.preview = QLabel('F8 · Render Lua replay or host C++ demo')
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setMinimumSize(240, 320)
        self.preview.setMaximumSize(280, 355)
        self.preview.setStyleSheet('background:#0d1b1f;border:2px solid #4f7b57;')
        right_layout.addWidget(self.preview, alignment=Qt.AlignHCenter)
        self.demo = QComboBox()
        self.demo.addItems(['snake', 'hello'])
        self.scenario = QComboBox()
        self.scenario.addItems(['playing', 'ready', 'paused'])
        controls = QFormLayout()
        controls.addRow('Demo', self.demo)
        controls.addRow('Scenario', self.scenario)
        right_layout.addLayout(controls)
        self.btn_preview = QPushButton('Run C++ Host Preview')
        self.btn_preview.clicked.connect(self.preview_host)
        right_layout.addWidget(self.btn_preview)
        self.btn_lua = QPushButton('Render Lua Replay (F8)')
        self.btn_lua.clicked.connect(self.preview_lua)
        right_layout.addWidget(self.btn_lua)
        replay_group = QGroupBox('DETERMINISTIC INPUT REPLAY')
        replay_layout = QVBoxLayout(replay_group)
        frame_row = QHBoxLayout()
        self.frames_spin = QSpinBox()
        self.frames_spin.setRange(1, 200)
        self.frames_spin.setValue(8)
        self.frames_spin.setPrefix('Frames: ')
        self.frame_spin = QSpinBox()
        self.frame_spin.setRange(0, 7)
        self.frame_spin.setPrefix('At: ')
        self.frames_spin.valueChanged.connect(lambda n: self.frame_spin.setMaximum(n-1))
        frame_row.addWidget(self.frames_spin)
        frame_row.addWidget(self.frame_spin)
        replay_layout.addLayout(frame_row)
        key_row = QHBoxLayout()
        self.replay_key = QComboBox()
        self.replay_key.addItems(['start', 'up', 'down', 'left', 'right', 'option'])
        self.replay_state = QComboBox()
        self.replay_state.addItems(['Press', 'Release'])
        key_row.addWidget(self.replay_key)
        key_row.addWidget(self.replay_state)
        replay_layout.addLayout(key_row)
        replay_buttons = QHBoxLayout()
        add_event = QPushButton('+ Event')
        add_event.clicked.connect(self.add_replay_event)
        undo_event = QPushButton('Undo')
        undo_event.clicked.connect(self.undo_replay_event)
        replay_buttons.addWidget(add_event)
        replay_buttons.addWidget(undo_event)
        replay_layout.addLayout(replay_buttons)
        self.replay_summary = QLabel('No replay events yet')
        self.replay_summary.setWordWrap(True)
        replay_layout.addWidget(self.replay_summary)
        save_replay = QPushButton('Save Input Replay JSON')
        save_replay.clicked.connect(self.save_replay)
        replay_layout.addWidget(save_replay)
        right_layout.addWidget(replay_group)
        self.btn_firmware = QPushButton('Choose Firmware Source…')
        self.btn_firmware.clicked.connect(self.select_firmware)
        right_layout.addWidget(self.btn_firmware)
        self.lbl_firmware = QLabel('Firmware path: not selected')
        self.lbl_firmware.setWordWrap(True)
        right_layout.addWidget(self.lbl_firmware)
        self.lbl_mode = QLabel('QEAPP/2: signed text/web; Lua: beta only')
        self.lbl_mode.setWordWrap(True)
        right_layout.addWidget(self.lbl_mode)
        right_layout.addStretch(1)
        scroller = QScrollArea()
        scroller.setWidgetResizable(True)
        scroller.setFrameShape(QScrollArea.NoFrame)
        scroller.setWidget(right_panel)
        self.right_tabs.addTab(scroller, 'REPLAY / HOST')
        # Reuse the right-side workbench as the single owner for diagnostics
        # and VM inspection. No extra dashboard or duplicate global controls.
        diag_page = QWidget()
        diag_layout = QVBoxLayout(diag_page)
        diag_layout.setContentsMargins(10, 9, 10, 9)
        self.vm_inspector = QLabel('VM INSPECTOR  ·  no Lua session active')
        self.vm_inspector.setObjectName('MetricBadge')
        self.vm_inspector.setWordWrap(True)
        diag_layout.addWidget(self.vm_inspector)
        diag_buttons = QHBoxLayout()
        diag_run = QPushButton('Run Doctor')
        diag_run.clicked.connect(self.run_gui_doctor)
        diag_repair = QPushButton('Safe Repair')
        diag_repair.clicked.connect(self.safe_repair_mode)
        diag_buttons.addWidget(diag_run)
        diag_buttons.addWidget(diag_repair)
        diag_layout.addLayout(diag_buttons)
        self.diag_output = QPlainTextEdit()
        self.diag_output.setReadOnly(True)
        self.diag_output.setMaximumBlockCount(500)
        self.diag_output.setPlainText('Run Doctor: non-destructive environment check.\n'
            'Repair: preview changes before confirmation. Only corrupt user settings can be reset.\n'
            'Lua debug logs appear in logs/vm-*.jsonl when explicitly enabled.\n'
            'Board peripherals and ESP32 boot are NOT emulated.')
        diag_layout.addWidget(self.diag_output, 1)
        self.right_tabs.addTab(diag_page, 'DIAGNOSTICS')
        self.diag_page = diag_page

        outer = QWidget()
        outer_layout = QHBoxLayout(outer)
        outer_layout.setContentsMargins(0, 0, 0, 0)
        outer_layout.setSpacing(0)
        outer_layout.addWidget(self.activity)
        self.content_splitter = QSplitter(Qt.Horizontal)
        self.content_splitter.addWidget(self.explorer_panel)
        self.content_splitter.addWidget(editor_panel)
        self.content_splitter.addWidget(self.right_tabs)
        self.content_splitter.setStretchFactor(0, 0)
        self.content_splitter.setStretchFactor(1, 1)
        self.content_splitter.setStretchFactor(2, 0)
        self._compact_layout_applied = False
        self.content_splitter.setSizes([175, 920, 255])
        outer_layout.addWidget(self.content_splitter, 1)
        self.page_stack = QStackedWidget()
        self.page_stack.addWidget(outer)
        self.home_page = HomePage()
        self.home_page.new_project_requested.connect(self.create_project_from_home)
        self.home_page.open_project_requested.connect(self.open_project_from_home)
        self.home_page.open_folder_requested.connect(self.open_project_folder)
        self.home_page.reveal_requested.connect(self.reveal_project)
        self.home_page.remove_recent_requested.connect(self.remove_recent_project)
        self.home_page.refresh_requested.connect(self.refresh_home)
        self.page_stack.addWidget(self.home_page)
        self.setCentralWidget(self.page_stack)
        self.setStatusBar(QStatusBar(self))
        self.statusBar().showMessage('QEAPP Studio 0.7.6  •  Lua host VM  •  Signed QEAPP/2 beta')
        self.actions_when_idle = [self.act_validate, self.act_build, self.act_inspect,
                                 self.act_doctor, self.act_beta_check, self.act_test,
                                 self.act_lua, self.btn_preview, self.btn_lua,
                                 self.btn_validate, self.btn_build]
        self.btn_stop.setEnabled(False)
        self.act_stop.setEnabled(False)
        self._log('QEAPP Studio v0.7.2 — VM inspect, debug logging, GUI diagnostics, safe repair.\n')
        self._set_project(None)
        if self.config.data['firmware_root']:
            self.lbl_firmware.setText('Firmware: ' + self.config.data['firmware_root'])
        self.explorer_toggle.setChecked(True)
        from PySide6.QtCore import QSettings
        self.ui_settings = QSettings('VQEAF', 'QEAPPStudio')
        geo = self.ui_settings.value('ui/windowGeometry')
        split = self.ui_settings.value('ui/mainSplitter')
        edit_split = self.ui_settings.value('ui/editorSplitter')
        if geo: self.restoreGeometry(geo)
        if split: self.content_splitter.restoreState(split)
        if edit_split: self.editor_splitter.restoreState(edit_split)
        if self.ui_settings.value('ui/bottomVisible', True, type=bool) is False:
            self.bottom_panel.hide()

    def show_home(self):
        if hasattr(self, 'page_stack'):
            self.page_stack.setCurrentWidget(self.home_page)
        self.refresh_home()

    def show_editor(self):
        if hasattr(self, 'page_stack'):
            self.page_stack.setCurrentIndex(0)

    def refresh_home(self):
        if not hasattr(self, 'home_page'):
            return
        self.project_library.set_recent_paths(self.config.data['recent_projects'])
        try:
            self.home_page.set_records(self.project_library.scan())
        except (OSError, ValueError) as exc:
            self._log('Project library: ' + str(exc) + '\n')

    def open_project_from_home(self, path=''):
        if not path:
            self.choose_project()
            return
        try:
            root = Path(path).resolve(strict=True)
            self._set_project(root)
            self.config.update(last_open_folder=str(root.parent))
        except (OSError, ValueError, WorkspaceError) as exc:
            self._error(str(exc))

    def open_project_folder(self, path=''):
        if not path:
            self.choose_project()
            return
        target = Path(path)
        if (target / 'qeapp.project.json').is_file():
            self.open_project_from_home(path)
        else:
            self.reveal_project(path)

    def reveal_project(self, path=''):
        if not path:
            return
        try:
            from PySide6.QtCore import QUrl
            from PySide6.QtGui import QDesktopServices
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(Path(path).resolve())))
        except (OSError, RuntimeError, ValueError) as exc:
            self._log('Reveal project: ' + str(exc) + '\n')

    def remove_recent_project(self, path=''):
        if not path:
            return
        try:
            target = str(Path(path).resolve())
            remaining = [item for item in self.config.data['recent_projects'] if item != target]
            self.config.update(recent_projects=remaining)
            self.project_library.set_recent_paths(remaining)
            self.refresh_home()
        except (OSError, ValueError) as exc:
            self._log('Remove recent project: ' + str(exc) + '\n')

    def create_project_from_home(self):
        self._create_project_via_dialog(default_template='lua-standard', open_after=True)

    def _create_project_via_dialog(self, *, default_template: str, open_after: bool = False, parent_folder: Path | None = None):
        from studio.core.app_id import collect_app_ids
        from studio.core.project_scaffold import ScaffoldError, create_standard_lua_project
        from studio.gui.new_project_dialog import NewProjectDialog

        projects_root = Path(parent_folder) if parent_folder is not None else Path(self.project_library.root)
        dialog = NewProjectDialog(self, projects_root=projects_root, default_template=default_template)
        if dialog.exec() != dialog.DialogCode.Accepted:
            return
        result = dialog.get_result()
        try:
            used = collect_app_ids(projects_root)
            if result.app_id in used:
                raise ScaffoldError(f'App ID đã tồn tại: {result.app_id}')
            if result.template in ('lua-standard', 'lua'):
                # Blank and standard Lua share the full default structure generator.
                # Keep lua-hello content for template=lua via init after scaffold base.
                target = create_standard_lua_project(
                    result.destination,
                    result.app_id,
                    result.display_name,
                    project_name=result.project_name,
                    used_app_ids=used,
                )
                if result.template == 'lua':
                    self._overlay_lua_hello_sample(target, result.app_id, result.display_name)
            else:
                from tools.qstudio import init_project
                init_project(result.template, result.destination, result.app_id, result.display_name)
                target = result.destination
            self.config.update(last_open_folder=str(projects_root))
            self.project_library.set_recent_paths(self.config.data['recent_projects'])
            self._set_project(target)
            if open_after:
                self.show_editor()
            self.open_document('main.lua' if (target / 'main.lua').is_file() else 'qeapp.project.json')
            self._log(f'Created project {result.display_name} ({result.app_id})\n')
        except (OSError, ValueError, KeyError, ScaffoldError) as exc:
            self._error(str(exc))

    def _overlay_lua_hello_sample(self, target: Path, app_id: str, display_name: str):
        """Keep sample gameplay for template lua while preserving standard layout."""
        sample = commands.ROOT / 'projects' / 'lua-hello' / 'main.lua'
        if sample.is_file():
            (target / 'main.lua').write_text(sample.read_text(encoding='utf-8'), encoding='utf-8', newline='\n')
        cfg = target / 'qeapp.project.json'
        obj = json.loads(cfg.read_text(encoding='utf-8'))
        obj['id'], obj['name'] = app_id, display_name
        cfg.write_text(json.dumps(obj, indent=2, ensure_ascii=True) + '\n', encoding='utf-8')

    def show_ai_agent(self):
        self.show_editor()
        if hasattr(self, 'ai_panel'):
            self.right_tabs.setCurrentWidget(self.ai_panel)
            self.ai_panel.setFocus(Qt.OtherFocusReason)

    def _ai_problems(self):
        return list(self._ai_problem_snapshot)

    def _refresh_ai_problem_snapshot(self):
        self._ai_problem_snapshot = [
            {'source': self.problems.topLevelItem(index).text(0),
             'diagnostic': self.problems.topLevelItem(index).text(1)}
            for index in range(self.problems.topLevelItemCount())
        ]

    def launch_emulator(self):
        if self.thread is not None:
            self._log('Build/preview running; wait before launching device.')
            return
        if not self._save_all_before_job():
            return
        try:
            from tools.qstudio import validate
            obj, assets = validate(self.workspace.root)
            if obj['type'] != 'lua':
                raise ValueError('Virtual device executes Lua projects only; text/web: use host preview')
            source = assets['content']
            # Original validator constrains path traversal/symlinks and <=64 KiB.
        except (OSError, ValueError) as exc:
            self._error('Virtual device: ' + str(exc))
            return
        self.show_virtual_phone()
        self._set_operation_status('RUN_HOST_LUA', 'STARTING HOST VM')
        from studio.core import commands
        exe = commands.ROOT / 'build' / ('qe_lua_host.exe' if sys.platform == 'win32' else 'qe_lua_host')
        host_cpp = commands.ROOT / 'runtime/host/qe_lua_host.cpp'
        vm_cpp = commands.ROOT / 'runtime/src/QeLuaRuntime.cpp'
        if exe.is_file() and all(exe.stat().st_mtime_ns >= path.stat().st_mtime_ns
                                 for path in (host_cpp,vm_cpp)):
            self.device.start(source, exe)
            self._sync_stop_controls()
            self._set_operation_status('RUN_HOST_LUA', 'HOST RUNNING')
            win = getattr(self, 'phone_window', None)
            if win is not None:
                win.show_and_raise()
            return
        # Build host in background; no GUI blocking and no vendor SDK needed.
        self._pending_emulator_script = source
        self._run_job(commands.build_lua_host())

    def _sync_stop_controls(self):
        busy = self.thread is not None or self.device.vm is not None
        self.act_stop.setEnabled(busy)
        self.btn_stop.setEnabled(busy)

    def _ensure_phone_window(self):
        from studio.gui.phone_window import PhoneWindow
        if getattr(self, 'phone_window', None) is None:
            self.phone_window = PhoneWindow(self.device, self)
            self.phone_window.attach_device(self.device, self.device_dock)
            self.phone_window.closed.connect(self._on_phone_window_closed)
        return self.phone_window

    def _on_phone_window_closed(self):
        self._sync_stop_controls()
        self._log('Virtual phone window closed (VM state unchanged; use STOP to end the guest).\n')

    def show_virtual_phone(self):
        win = self._ensure_phone_window()
        win.show_and_raise()
        self._log('Virtual phone window opened (host Lua VM shell).\n')

    def toggle_explorer(self):
        self.explorer_panel.setVisible(not self.explorer_panel.isVisible())
        self.explorer_toggle.setChecked(self.explorer_panel.isVisible())

    def _select_activity(self, page):
        self.show_editor()
        self.explorer_panel.show()
        self.explorer_toggle.setChecked(page == 'explorer')
        self.search_toggle.setChecked(page == 'search')
        self.left_pages.setCurrentIndex(1 if page == 'search' else 0)
        if page == 'search': self.project_query.setFocus()
        else: self.tree.setFocus()

    def show_project_search(self):
        self._select_activity('search')
        self.project_query.selectAll()

    def search_all_files(self):
        self.search_results.clear()
        if not self.workspace:
            self.search_count.setText('Open a project to search source files.')
            return
        query = self.project_query.text()
        if not query.strip():
            self.search_count.setText('Enter source text to find. (Max 100 characters.)')
            return
        try:
            snapshots = {rel: edit.toPlainText() for rel, edit in self.editors.items()
                         if edit.document().isModified()}
            report = search_project(self.workspace, query,
                                    snapshots=snapshots,
                                    case_sensitive=self.search_case.isChecked())
        except (ValueError, OSError) as exc:
            self.search_count.setText(str(exc))
            return
        parents = {}
        for item in report.matches:
            parent = parents.get(item.path)
            if parent is None:
                parent = QTreeWidgetItem([item.path])
                self.search_results.addTopLevelItem(parent)
                parents[item.path] = parent
            child = QTreeWidgetItem([f'{item.line}:{item.column}  {item.excerpt.strip()}'])
            child.setData(0, Qt.UserRole, (item.path, item.line, item.column))
            parent.addChild(child)
        self.search_results.expandAll()
        self.search_count.setText(
            f'{len(report.matches)} match(es) in {report.files_scanned} file(s)'
            + (' · truncated at safe limit' if report.truncated else ''))

    def open_search_result(self, item, column=0):
        loc = item.data(0, Qt.UserRole)
        if not loc:
            self.search_results.expandItem(item)
            return
        rel, line, column = loc
        self.open_document(rel)
        edit = self.editors.get(rel)
        if edit is not None:
            edit.goto(line, column)

    def toggle_bottom_panel(self):
        new_visible = not self.bottom_panel.isVisible()
        self.bottom_panel.setVisible(new_visible)
        if new_visible:
            self.editor_splitter.setSizes([640, 110])
        self.ui_settings.setValue('ui/bottomVisible', new_visible)

    def filter_tree(self, text):
        target = text.strip().lower()
        def walk(node):
            matches = target in node.text(0).lower()
            child = any(walk(node.child(i)) for i in range(node.childCount()))
            node.setHidden(bool(target) and not (matches or child))
            return matches or child
        for i in range(self.tree.topLevelItemCount()):
            walk(self.tree.topLevelItem(i))
        if target: self.tree.expandAll()

    def quick_open(self):
        if not self.workspace: self._error('Open project first'); return
        files = list(self.workspace.iter_files())
        if not files: return
        target, ok = QInputDialog.getItem(self, 'Quick Open  ·  Ctrl+P', 'Project file', files, 0, False)
        if ok and target: self.open_document(target)

    def find_in_editor(self):
        if self._current_editor() is None: return
        self.editor_find.show()
        self._find_next_btn.show()
        self.editor_find.setFocus()

    def show_replace(self):
        self.find_in_editor()
        if self._current_editor() is None: return
        self.editor_replace.show()
        self.replace_button.show()

    def replace_next(self):
        editor = self._current_editor()
        if editor is None or not self.editor_find.text(): return
        cursor = editor.textCursor()
        if not (cursor.hasSelection() and cursor.selectedText() == self.editor_find.text()):
            self.find_next()
            cursor = editor.textCursor()
        if cursor.hasSelection() and cursor.selectedText() == self.editor_find.text():
            cursor.insertText(self.editor_replace.text())
            self.find_next()

    def go_to_line(self):
        editor = self._current_editor()
        if editor is None: return
        current = editor.textCursor().blockNumber() + 1
        number, ok = QInputDialog.getInt(self, 'Go to line', 'Line:', current,
                                          1, editor.blockCount())
        if ok: editor.goto(number)

    def _cursor_changed(self):
        editor = self._current_editor()
        if editor:
            cursor = editor.textCursor()
            self.statusBar().showMessage(
                f'{editor.relative_path}   Ln {cursor.blockNumber()+1}, '
                f'Col {cursor.positionInBlock()+1}   · Host-only preview')

    def find_next(self):
        editor = self._current_editor()
        if editor is None or not self.editor_find.text(): return
        if not editor.find(self.editor_find.text()):
            editor.moveCursor(QTextCursor.Start)
            editor.find(self.editor_find.text())

    def command_palette(self):
        values = ('Home / Projects','AI Agent','Open Project','Validate','Build signed .qeapp', 'Run Lua virtual phone',
                  'Render replay', 'Stop', 'Choose firmware', 'Run host tests',
                  'Search project', 'Go to line', 'Diagnostics', 'Safe Repair', 'Restart VM')
        label, ok = QInputDialog.getItem(self, 'QEAPP Command Palette', 'Command', values, 0, False)
        if not ok: return
        commands_map = dict(zip(values, (self.show_home, self.show_ai_agent, self.choose_project, self.validate_project,
            self.build_project, self.launch_emulator, self.preview_lua, self.stop_job,
            self.select_firmware, self.run_tests, self.show_project_search, self.go_to_line,
            self.show_diagnostics, self.safe_repair_mode, self.restart_virtual_machine)))
        commands_map[label]()

    def update_vm_inspector(self, details):
        self.vm_inspector.setText('VM INSPECTOR\n' + details)

    def on_guest_problem(self, line: int, details: str):
        source = self.device._pending_source
        rel = 'main.lua'
        if self.workspace and source:
            try:
                rel = source.resolve().relative_to(self.workspace.root).as_posix()
            except ValueError:
                rel = 'main.lua'
        item = QTreeWidgetItem([f'{rel}:{line}' if line else rel, details[:250]])
        item.setData(0, Qt.UserRole, (rel, line))
        self.problems.addTopLevelItem(item)
        self._refresh_ai_problem_snapshot()
        self.bottom_panel.show()
        self.bottom_tabs.setCurrentWidget(self.problems)
        self._log('LUA PROBLEM ' + item.text(0) + ': ' + details)
        if line and self.workspace and rel in self.workspace.iter_files():
            self.open_document(rel)
            editor = self.editors.get(rel)
            if editor: editor.goto(line)

    def open_problem(self, item, column=0):
        location = item.data(0, Qt.UserRole)
        if not location or not self.workspace: return
        rel, line = location
        if line and rel in self.workspace.iter_files():
            self.open_document(rel)
            editor = self.editors.get(rel)
            if editor: editor.goto(line)

    def show_diagnostics(self):
        self.show_editor()
        self.right_tabs.setCurrentWidget(self.diag_page)
        self.run_gui_doctor()

    def run_gui_doctor(self):
        from studio.core.repair import inspect
        checks = inspect(qt_probe=False)
        report = '\n'.join(f'[{x.status}] {x.name}: {x.detail}' for x in checks)
        # UI-specific check uses the active QApplication and local QImage,
        # not a claim about Microsoft Windows or physical ESP32 hardware.
        from PySide6.QtGui import QImage
        image = QImage(240, 320, QImage.Format_RGB16)
        report += ('\n[PASS] Qt image allocation: 240x320' if not image.isNull()
                   else '\n[FAIL] Qt framebuffer allocation')
        report += '\n[INFO] Actual hardware/PlatformIO checks: NOT RUN here.'
        self.diag_output.setPlainText(report)
        self._log('GUI Doctor finished: results available in Diagnostics tab.')
        return checks

    def safe_repair_mode(self):
        from studio.core.repair import inspect, apply
        self.right_tabs.setCurrentWidget(self.diag_page)
        checks = self.run_gui_doctor()
        affected = [c for c in checks if c.repairable]
        if not affected:
            QMessageBox.information(self, 'Safe Repair',
                'No auto-repairable corrupted user settings detected.\n'
                'Missing Lua source/compiler: use the launcher setup flags.\n'
                'This tool never replaces or deletes project source, .venv, firmware or signing keys.')
            return
        target = ', '.join(c.name for c in affected)
        answer = QMessageBox.question(self, 'Safe Repair — confirm',
            f'Backup and reset corrupt {target}?\n\n'
            'Your original file is kept as a timestamped backup.\n'
            'No project files, VM, firmware, keys or packages will be changed.',
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if answer != QMessageBox.Yes: return
        try:
            backup = apply()
            self.diag_output.appendPlainText('REPAIRED: original backup at ' + str(backup))
            self._log('Safe Repair created backup: ' + str(backup))
            # Hot reload only user config; do not silently close/reload editors.
            self.config = StudioConfig()
        except (OSError, ValueError) as exc:
            self._error('Safe Repair failed; original file preserved where possible: ' + str(exc))

    def reset_layout(self):
        answer = QMessageBox.question(self, 'Reset IDE Layout',
           'Restore default panel sizes and show Explorer + bottom output?\n'
           'Project source and settings.json will not be touched.',
           QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if answer != QMessageBox.Yes: return
        self.explorer_panel.show()
        self.bottom_panel.show()
        self.content_splitter.setSizes([175, 920, 255])
        self.editor_splitter.setSizes([640, 110])
        self.ui_settings.remove('ui/mainSplitter')
        self.ui_settings.remove('ui/editorSplitter')
        self.ui_settings.remove('ui/bottomVisible')
        self._log('Workspace layout reset. Source files unchanged.')

    def restart_virtual_machine(self):
        if not self.device.restart():
            self._log('Restart VM: no previously launched Lua source or host binary. Run F9 first.')
        self._sync_stop_controls()

    def about_studio(self):
        QMessageBox.information(self, 'QEAPP Studio 0.7.6',
            'Independent PySide6 code editor and Lua 5.4 host virtual phone.\n\n'
            'Device preview executes the shared bounded host VM, not ESP32 hardware.\n'
            'Lua QEAPP/2 requires experimental vqeaf_lua_beta firmware and matching signature.\n'
            'Desktop virtual phone does not emulate SD, WiFi, browser or app installer.')

    def _log(self, message: str, kind: str | None = None):
        """Append a line to OUTPUT. kind: 'ok' green, 'err' red, None auto-detect."""
        text = message if message.endswith('\n') else message + '\n'
        color = self._log_color(text, kind)
        self.logs.moveCursor(QTextCursor.End)
        if color is None:
            self.logs.insertPlainText(text)
        else:
            fmt = QTextCharFormat()
            fmt.setForeground(QColor(color))
            cursor = self.logs.textCursor()
            cursor.movePosition(QTextCursor.End)
            cursor.insertText(text, fmt)
        self.logs.ensureCursorVisible()

    @staticmethod
    def _log_color(text: str, kind: str | None = None):
        if kind == 'ok':
            return '#22c55e'
        if kind == 'err':
            return '#ef4444'
        if kind == 'warn':
            return '#f59e0b'
        lower = text.casefold()
        err_tokens = (
            'error', 'fail', 'failed', 'exception', 'traceback', 'invalid',
            'rejected', 'denied', 'cannot', 'unable', 'missing', 'not found',
            'user_cancelled', 'os_root_', 'project_invalid', 'signer_',
            'vm_crash', 'lua_beta_required',
        )
        ok_tokens = (
            'pass', 'passed', 'success', 'successful', 'ok', 'created',
            'saved', 'installed', 'verified', 'ready', 'build passed',
            'validate passed', 'host ready', 'signature verified',
        )
        # Prefer a clear FAIL/PASS or ERROR marker over mixed wording.
        if any(tok in lower for tok in ('fail', 'error', 'exception', 'traceback', 'rejected', 'invalid')):
            return '#ef4444'
        if lower.lstrip().startswith(('pass', 'ok', 'success', 'created', 'saved', 'verified', 'ready')):
            return '#22c55e'
        if any(tok in lower for tok in err_tokens):
            return '#ef4444'
        if any(tok in lower for tok in ok_tokens):
            return '#22c55e'
        return None

    def _error(self, text: str):
        self._log('ERROR: ' + text, 'err')
        QMessageBox.warning(self, 'QEAPP Studio', text)

    def _set_project(self, path: Path | None):
        if getattr(self, 'device', None) and self.workspace is not None:
            if path is None or Path(path).resolve()!=self.workspace.root:
                self.device.stop()
                self.problems.clear()
                self._refresh_ai_problem_snapshot()
        if path is not None:
            workspace = Workspace(path)
            if not self._close_all_tabs():
                return
            self.workspace = workspace
            self.config.remember(workspace.root)
            self.project_library.set_recent_paths(self.config.data['recent_projects'])
            self.project_library.add_recent(workspace.root)
            if hasattr(self, 'ai_panel'):
                self.ai_panel.set_project(workspace)
            self.lbl_project.setText('ROOT: ' + workspace.root.name)
            try:
                from tools.qstudio import validate
                obj, _ = validate(workspace.root)
                self.lbl_mode.setText('QEAPP/2 '+obj['type']+ (' — beta signed build requires vqeaf_lua_beta' if obj['type']=='lua' else ' — signed build available'))
            except Exception as exc:
                self.lbl_mode.setText('Validate required: ' + str(exc))
            self._log('Project: ' + str(workspace.root))
            self._load_replay()
        else:
            self.workspace = None
            if hasattr(self, 'ai_panel'):
                self.ai_panel.set_project(None)
            self.tree.clear()
            self.lbl_project.setText('No project open')
            self.lbl_mode.setText('QEAPP/2: text/web stable; Lua app: beta target only')
            self.replay_events = []
            self.replay_dirty = False
            self._refresh_replay()
        self.refresh_tree()
        self.search_results.clear()
        self.search_count.setText('Search source files · changes not saved are included')
        self.welcome.setVisible(self.workspace is None and self.tabs.count() == 0)
        if self.workspace is None:
            self.show_home()
        else:
            self.show_editor()

    def choose_project(self):
        start = self.config.data['last_open_folder'] or str(Path.home())
        folder = QFileDialog.getExistingDirectory(self, 'Open QEAPP project', start)
        if folder:
            try:
                self._set_project(Path(folder))
                self.config.update(last_open_folder=str(Path(folder).parent))
            except (WorkspaceError, OSError) as exc:
                self._error(str(exc))

    def create_project(self, kind: str):
        mapping = {
            'text': 'text', 'web': 'web', 'lua': 'lua', 'lua-standard': 'lua-standard',
            'lua-snake': 'lua-snake', 'lua-sprite': 'lua-sprite', 'lua-chess': 'lua-chess',
        }
        template = mapping.get(kind, kind)
        folder = QFileDialog.getExistingDirectory(self, 'Choose parent folder')
        if not folder:
            return
        self._create_project_via_dialog(
            default_template=template,
            open_after=True,
            parent_folder=Path(folder),
        )

    def refresh_tree(self):
        self.tree.clear()
        if not self.workspace:
            return
        grouped: dict[tuple, QTreeWidgetItem] = {}
        for rel in self.workspace.iter_files():
            folder = ()
            segments = Path(rel).parts
            for segment in segments[:-1]:
                folder += (segment,)
                if folder not in grouped:
                    parent = grouped.get(folder[:-1])
                    item = QTreeWidgetItem([segment])
                    if parent is None:
                        self.tree.addTopLevelItem(item)
                    else:
                        parent.addChild(item)
                    grouped[folder] = item
            file_item = QTreeWidgetItem([segments[-1]])
            file_item.setData(0, Qt.UserRole, rel)
            if not folder:
                self.tree.addTopLevelItem(file_item)
            else:
                grouped[folder].addChild(file_item)
        self.tree.expandToDepth(1)
        self.filter_tree(self.project_filter.text())

    def open_tree_file(self, item: QTreeWidgetItem):
        rel = item.data(0, Qt.UserRole)
        if rel:
            self.open_document(rel)

    def _open_png_preview(self, rel: str):
        if not self.workspace:
            return
        if rel in self.image_previews:
            self.tabs.setCurrentWidget(self.image_previews[rel]); return
        try:
            raw = self.workspace.read_png_preview(rel)
            pix = QPixmap()
            if not pix.loadFromData(raw,'PNG'):
                raise WorkspaceError('Image decoder rejected PNG')
            page = QWidget()
            layout = QVBoxLayout(page)
            label = QLabel(f'{rel}\n{pix.width()} × {pix.height()} px · host preview only')
            label.setAlignment(Qt.AlignCenter)
            layout.addWidget(label)
            canvas = QLabel()
            canvas.setAlignment(Qt.AlignCenter)
            canvas.setPixmap(pix.scaled(256,256,Qt.KeepAspectRatio,Qt.FastTransformation))
            layout.addWidget(canvas,alignment=Qt.AlignCenter)
            layout.addStretch(1)
            self.image_previews[rel] = page
            index = self.tabs.addTab(page,Path(rel).name)
            self.tabs.setTabToolTip(index,rel)
            self.tabs.setCurrentWidget(page)
        except (WorkspaceError,OSError) as exc:
            self._error('PNG preview: '+str(exc))

    def open_document(self, rel: str):
        if not self.workspace:
            return
        if Path(rel).suffix.lower() == '.png':
            self._open_png_preview(rel)
            return
        if rel in self.editors:
            self.tabs.setCurrentWidget(self.editors[rel])
            return
        try:
            doc = self.workspace.read(rel)
        except (WorkspaceError, OSError) as exc:
            self._error(str(exc))
            return
        edit = CodeEditor(doc)
        edit.document().modificationChanged.connect(lambda dirty, e=edit: self._update_title(e))
        edit.cursorPositionChanged.connect(self._cursor_changed)
        self.editors[rel] = edit
        index = self.tabs.addTab(edit, Path(rel).name)
        self.tabs.setTabToolTip(index, rel)
        self.tabs.setCurrentWidget(edit)

    def new_file(self):
        if not self.workspace:
            self._error('Open a project before creating source files')
            return
        rel, ok = QInputDialog.getText(self, 'New file', 'Relative path (example: src/main.lua)')
        if not ok:
            return
        try:
            document = self.workspace.save(rel, '', None)
            self.refresh_tree()
            self.open_document(document.relative_path)
        except (WorkspaceError, OSError) as exc:
            self._error(str(exc))

    def insert_pixel_sprite(self):
        """Convert a tiny PNG into an inline Lua mask; never load target assets from disk."""
        editor = self._current_editor()
        if editor is None or not self.workspace or Path(editor.relative_path).suffix.lower()!='.lua':
            self._error('Open a Lua source tab before inserting a sprite')
            return
        file, _ = QFileDialog.getOpenFileName(self, 'Pixel sprite PNG (1..32 pixels per side)',
                                              str(self.workspace.root), 'PNG image (*.png)')
        if not file:return
        name, ok = QInputDialog.getText(self, 'Sprite name', 'Lua symbol (example: hero):',text='hero')
        if not ok:return
        try:
            from tools.pixel_sprite import lua_snippet
            snippet=lua_snippet(Path(file),name)
            editor.textCursor().insertText('\n'+snippet+'\n')
            self._log(f'Inserted sprite `{name}` from a local PNG into {editor.relative_path} (not saved).')
        except (OSError, ValueError, ImportError) as exc:
            self._error('Sprite import failed: '+str(exc))

    def new_folder(self):
        if not self.workspace:
            self._error('Open a project before creating folders')
            return
        rel, ok = QInputDialog.getText(self, 'New folder', 'Relative folder path (example: assets/sprites)')
        if not ok:
            return
        try:
            self.workspace.mkdir(rel)
            self.refresh_tree()
        except (WorkspaceError, OSError) as exc:
            self._error(str(exc))

    def _update_title(self, edit: CodeEditor):
        index = self.tabs.indexOf(edit)
        if index >= 0:
            self.tabs.setTabText(index, Path(edit.relative_path).name + (' ●' if edit.document().isModified() else ''))

    def _current_editor(self) -> CodeEditor | None:
        widget = self.tabs.currentWidget()
        return widget if isinstance(widget, CodeEditor) else None

    def save_current(self) -> bool:
        edit = self._current_editor()
        if not self.workspace or edit is None:
            return True
        try:
            doc = self.workspace.save(edit.relative_path, edit.toPlainText(), edit.disk_sha)
            edit.disk_sha = doc.sha256
            edit.document().setModified(False)
            self._log('Saved: ' + edit.relative_path)
            return True
        except (WorkspaceError, OSError) as exc:
            self._error(str(exc))
            return False

    def _resolve_unsaved(self, edit: CodeEditor) -> bool:
        if not edit.document().isModified():
            return True
        result = QMessageBox.question(self, 'Unsaved changes',
            'Save changes to ' + edit.relative_path + '?',
            QMessageBox.Save | QMessageBox.Discard | QMessageBox.Cancel)
        if result == QMessageBox.Cancel:
            return False
        if result == QMessageBox.Save:
            self.tabs.setCurrentWidget(edit)
            return self.save_current()
        return True

    def close_tab(self, index: int):
        edit = self.tabs.widget(index)
        if isinstance(edit, CodeEditor) and not self._resolve_unsaved(edit):
            return
        if isinstance(edit, CodeEditor):
            self.editors.pop(edit.relative_path, None)
        for rel, preview in list(self.image_previews.items()):
            if preview is edit:
                self.image_previews.pop(rel, None)
        self.tabs.removeTab(index)
        edit.deleteLater()

    def _close_all_tabs(self) -> bool:
        for i in reversed(range(self.tabs.count())):
            edit = self.tabs.widget(i)
            if isinstance(edit, CodeEditor) and not self._resolve_unsaved(edit):
                return False
        self.tabs.clear()
        self.editors.clear()
        self.image_previews.clear()
        return True

    def close_project(self):
        if self._close_all_tabs():
            self._set_project(None)

    def _on_tab_changed(self, index: int):
        if index >= 0:
            editor = self._current_editor()
            if editor:
                self.statusBar().showMessage(editor.relative_path)

    def _save_all_before_job(self) -> bool:
        if not self.workspace:
            self._error('Open a project first')
            return False
        for edit in list(self.editors.values()):
            if edit.document().isModified():
                self.tabs.setCurrentWidget(edit)
                if not self.save_current():
                    return False
        return True

    def select_firmware(self):
        self.open_firmware_settings()

    def open_firmware_settings(self):
        from studio.gui.firmware_settings import FirmwareSettingsDialog
        dialog = FirmwareSettingsDialog(self, current=self.config.data.get('firmware_root', ''))
        if dialog.exec() == dialog.DialogCode.Accepted:
            path = dialog.selected_path or ''
            self.config.update(firmware_root=path)
            self.lbl_firmware.setText('Firmware: ' + (path or 'not selected'))
            if path:
                self._log(f'Firmware source set: {path}\n')
            self._set_operation_status('SETTINGS', 'FIRMWARE_ROOT_UPDATED')

    def _save_firmware_root(self, path: str) -> None:
        self.config.update(firmware_root=path)
        self.lbl_firmware.setText('Firmware: ' + (path or 'not selected'))

    def _firmware(self, *, operation: str = 'BUILD_SIGNED_QEAPP') -> Path | None:
        """Resolve VQEAF-OS source for signed build. Offers recovery when missing."""
        from studio.core.errors import StudioError
        from tools.studio_firmware import FirmwareRootError, firmware_root, inspect_firmware_root

        configured = self.config.data.get('firmware_root', '')
        report = inspect_firmware_root(configured) if configured else inspect_firmware_root('')
        if report.get('ready_for_build'):
            return Path(report['resolved'])

        # Fall back to env / sibling discovery used by CLI.
        try:
            discovered = firmware_root()
        except FirmwareRootError:
            discovered = None
        if discovered is not None:
            self._save_firmware_root(str(discovered))
            return discovered

        code = report.get('error_code') or 'OS_ROOT_MISSING'
        if code not in ('OS_ROOT_MISSING', 'OS_ROOT_INVALID', 'OS_ROOT_IS_STUDIO', 'SIGNER_MISSING'):
            code = 'OS_ROOT_MISSING'
        err = StudioError(code, report.get('message') or '', operation=operation)
        self._log(err.ui_message() + '\n')
        self._set_operation_status(operation, 'BUILD FAILED')
        self.problems.addTopLevelItem(QTreeWidgetItem([operation, f'{err.code} · {err.info.title}']))
        self._refresh_ai_problem_snapshot()

        from studio.gui.firmware_settings import FirmwareRecoveryDialog
        recovery = FirmwareRecoveryDialog(self, error=err, current=configured)
        result = recovery.exec()
        if recovery.open_settings or result != recovery.DialogCode.Accepted:
            if recovery.open_settings:
                self.open_firmware_settings()
                return self._firmware(operation=operation) if self.config.data.get('firmware_root') else None
            self._log('Build cancelled: firmware source not selected.\n')
            self._set_operation_status(operation, 'BUILD FAILED')
            return None
        if recovery.selected_path:
            self._save_firmware_root(recovery.selected_path)
            return Path(recovery.selected_path)
        return None

    def _run_job(self, spec: commands.JobSpec):
        if self.thread is not None:
            self._log('Busy. Stop the active job first.')
            return
        self._log('\n━━ ' + spec.label + ' ━━')
        self._job_output_lines = []
        self.thread = QThread(self)
        self.worker = JobWorker(self.runner, spec)
        self.worker.moveToThread(self.thread)
        self.thread.started.connect(self.worker.run)
        self.worker.output.connect(self._job_output)
        self.worker.finished.connect(self._job_finished)
        self.worker.finished.connect(self.thread.quit)
        self.worker.finished.connect(self.worker.deleteLater)
        self.thread.finished.connect(self._thread_finished)
        self.thread.start()
        for action in self.actions_when_idle:
            action.setEnabled(False)
        self.act_stop.setEnabled(True)
        self.btn_stop.setEnabled(True)
        self.statusBar().showMessage(spec.label)

    def _job_output(self, text: str):
        self._log(text)
        if not hasattr(self, '_job_output_lines'):
            self._job_output_lines = []
        self._job_output_lines.append(text)
        if len(self._job_output_lines) > 2000:
            del self._job_output_lines[:-1200]

    def _add_job_problems(self, label: str):
        pattern = re.compile(r'(?P<file>.+?):(?P<line>\d+)(?::(?P<column>\d+))?:\s*(?P<severity>error|warning|note):\s*(?P<message>.+)', re.IGNORECASE)
        seen: set[tuple[str, str, str]] = set()
        for line in getattr(self, '_job_output_lines', []):
            match = pattern.search(line.strip())
            if match is None:
                continue
            source = match.group('file').strip().strip('"')
            line_number = int(match.group('line'))
            column = int(match.group('column') or 0)
            relative = source.replace('\\', '/')
            if self.workspace:
                try:
                    candidate = Path(source)
                    if candidate.is_absolute():
                        relative = candidate.resolve(strict=False).relative_to(self.workspace.root).as_posix()
                except (OSError, ValueError):
                    pass
            message = match.group('message').strip()[:500]
            key = (relative, str(line_number), message)
            if key in seen:
                continue
            seen.add(key)
            location = f'{relative}:{line_number}' + (f':{column}' if column else '')
            item = QTreeWidgetItem([location, f'{match.group("severity").upper()}: {message}'])
            item.setData(0, Qt.UserRole, (relative, line_number))
            self.problems.addTopLevelItem(item)
        return bool(seen)

    @Slot(int, str, str)
    def _job_finished(self, code: int, label: str, preview: str):
        operation = getattr(self, '_last_operation', '') or label
        if code == 0:
            self._log('PASS — ' + label + '\n', 'ok')
        else:
            self._log(f'FAIL exit {code} — ' + label + '\n', 'err')
        if code != 0:
            parsed = self._add_job_problems(label)
            if not parsed:
                self.problems.addTopLevelItem(QTreeWidgetItem([label, f'Exit {code} · See Output for diagnostics']))
            self._refresh_ai_problem_snapshot()
            self.bottom_tabs.setCurrentWidget(self.problems)
            if 'Build' in label or 'QEAPP' in label:
                self._set_operation_status(operation or 'BUILD_SIGNED_QEAPP', 'BUILD FAILED')
            elif 'Validate' in label:
                self._set_operation_status(operation or 'VALIDATE_PROJECT', 'VALIDATE FAILED')
            else:
                self.statusBar().showMessage('FAILED: ' + label, 10000)
        else:
            # Keep prior failures visible until the user explicitly starts a new validation.
            self.bottom_tabs.setCurrentWidget(self.logs)
            if 'Build' in label and 'Lua host' not in label:
                self._set_operation_status(operation or 'BUILD_SIGNED_QEAPP', 'BUILD PASSED')
            elif 'Validate' in label:
                self._set_operation_status(operation or 'VALIDATE_PROJECT', 'VALIDATE PASSED')
            elif 'Lua' in label and 'host' in label.lower():
                self._set_operation_status(operation or 'RUN_HOST_LUA', 'HOST READY')
            else:
                self.statusBar().showMessage('PASS: ' + label, 10000)
        if label == 'Build Lua host virtual-phone runner' and code == 0 and self._pending_emulator_script is not None:
            script = self._pending_emulator_script
            self._pending_emulator_script = None
            exe = commands.ROOT / 'build' / ('qe_lua_host.exe' if sys.platform == 'win32' else 'qe_lua_host')
            self.device.start(script, exe)
            self._set_operation_status('RUN_HOST_LUA', 'HOST RUNNING')
        if label == 'Build Lua host virtual-phone runner' and code != 0:
            self._pending_emulator_script = None
            self._set_operation_status('RUN_HOST_LUA', 'HOST FAILED')
        if preview:
            self.right_tabs.setCurrentIndex(1)
            pix = QPixmap(preview)
            if not pix.isNull():
                self.preview.setPixmap(pix.scaled(240, 320, Qt.KeepAspectRatio,
                                                 Qt.FastTransformation))

    @Slot()
    def _thread_finished(self):
        if self.thread:
            self.thread.deleteLater()
        self.thread = None
        self.worker = None
        for action in self.actions_when_idle:
            action.setEnabled(True)
        self._sync_stop_controls()

    def stop_job(self):
        if getattr(self, 'device', None) is not None:
            self.device.stop(silent=True)
            self._sync_stop_controls()
        if self.runner.cancel():
            self._log('Stop requested; terminating worker process tree…')
        elif self.thread is not None:
            self._log('Job is starting/finishing; waiting for cleanup.')

    def validate_project(self):
        if self._save_all_before_job():
            self._set_operation_status('VALIDATE_PROJECT', 'VALIDATING')
            self._run_job(commands.validate(self.workspace.root))

    def _set_operation_status(self, operation: str, status: str):
        """Status is always tagged with its operation id so Host and Build never mix."""
        self._last_operation = operation
        self._last_op_status = status
        self.statusBar().showMessage(f'{operation}: {status}')
        kind = 'ok' if any(k in status for k in ('PASSED', 'READY', 'UPDATED', 'RUNNING')) else (
            'err' if any(k in status for k in ('FAILED', 'INVALID')) else None)
        self._log(f'[{operation}] {status}\n', kind)

    def export_diagnostic_bundle(self):
        try:
            from tools.studio_doctor import export_bundle
            path = export_bundle()
        except Exception as exc:
            self._error('Export Diagnostic Bundle: ' + str(exc))
            return
        self._log(f'Diagnostic bundle written: {path}\n')
        self.statusBar().showMessage(f'Diagnostic bundle: {path}', 8000)

    def build_preflight(self, project_type: str) -> tuple[Path | None, str | None]:
        """Return (firmware_root, error_code). error_code is None when ready."""
        from studio.core.errors import StudioError
        from tools.studio_firmware import inspect_firmware_root

        configured = self.config.data.get('firmware_root', '')
        report = inspect_firmware_root(configured) if configured else inspect_firmware_root('')
        if not report.get('ready_for_build'):
            try:
                root = self._firmware(operation='BUILD_SIGNED_QEAPP')
            except Exception as exc:
                self._error(str(exc))
                return None, 'OS_ROOT_MISSING'
            if root is None:
                return None, 'OS_ROOT_MISSING'
            report = inspect_firmware_root(root)
        root = Path(report.get('resolved') or configured)
        if project_type == 'lua' and not report.get('ready_for_lua_build'):
            # Host preview stays available; signed Lua package is not allowed on stock-only tree.
            err = StudioError(
                'LUA_BETA_REQUIRED',
                'Checkout này không có src/lua/QeLuaRuntime.cpp (firmware Lua beta).',
                operation='BUILD_SIGNED_QEAPP',
            )
            self._log(err.ui_message() + '\n')
            self.problems.addTopLevelItem(QTreeWidgetItem(['BUILD_SIGNED_QEAPP', f'{err.code} · {err.info.title}']))
            self._refresh_ai_problem_snapshot()
            self._set_operation_status('BUILD_SIGNED_QEAPP', 'BUILD FAILED')
            self._error(err.ui_message())
            return None, 'LUA_BETA_REQUIRED'
        return root, None

    def build_project(self):
        if not self._save_all_before_job():
            return
        self._set_operation_status('BUILD_SIGNED_QEAPP', 'PREFLIGHT')
        from tools.qstudio import validate
        try:
            obj, _ = validate(self.workspace.root)
        except (OSError, ValueError) as exc:
            self._error('PROJECT_INVALID: Validation failed: ' + str(exc))
            self._set_operation_status('BUILD_SIGNED_QEAPP', 'BUILD FAILED')
            self.problems.addTopLevelItem(QTreeWidgetItem(['BUILD_SIGNED_QEAPP', f'PROJECT_INVALID · {exc}']))
            return
        if obj['type'] not in ('web', 'text', 'lua'):
            self._error('Supported project types are web/text/lua')
            self._set_operation_status('BUILD_SIGNED_QEAPP', 'BUILD FAILED')
            return
        firmware, err_code = self.build_preflight(obj['type'])
        if firmware is None:
            return
        key_file, _ = QFileDialog.getOpenFileName(self, 'Choose P-256 private signing key (not saved)',
                                                  str(Path.home()), 'PEM private keys (*.pem *.key)')
        if not key_file:
            self._set_operation_status('BUILD_SIGNED_QEAPP', 'BUILD FAILED')
            self._log('USER_CANCELLED: signing key not selected.\n')
            return
        key_id, ok = QInputDialog.getText(self, 'Publisher Key ID', 'Key ID (hex or decimal)',
                                          text='0x544c5541' if obj['type']=='lua' else '0x31534351')
        if not ok:
            self._set_operation_status('BUILD_SIGNED_QEAPP', 'BUILD FAILED')
            self._log('USER_CANCELLED: key id not provided.\n')
            return
        try:
            numeric = int(key_id, 0)
            if not 0 <= numeric <= 0xFFFFFFFF:
                raise ValueError('Key ID out of range')
        except ValueError as exc:
            self._error(str(exc))
            self._set_operation_status('BUILD_SIGNED_QEAPP', 'BUILD FAILED')
            return
        dist = self.workspace.root / 'dist'
        dist.mkdir(exist_ok=True)
        self._set_operation_status('BUILD_SIGNED_QEAPP', 'BUILDING')
        self._run_job(commands.build(self.workspace.root, firmware, Path(key_file),
                                     dist / (obj['id'] + '.qeapp'), numeric, experimental_lua=obj['type']=='lua'))

    def inspect_package(self):
        p, _ = QFileDialog.getOpenFileName(self, 'Inspect QEAPP/2 binary',
                                           str(self.workspace.root if self.workspace else Path.home()),
                                           'QEAPP package (*.qeapp)')
        if not p:
            return
        # Basic Inspect checks format+hash, NOT cryptographic signature.
        pub = QMessageBox.question(self, 'Signature check',
              'Select a public PEM to verify the ECDSA signature?\n'
              'Choosing No only checks package structure/hashes.',
              QMessageBox.Yes | QMessageBox.No)
        public_path = None
        if pub == QMessageBox.Yes:
            v, _ = QFileDialog.getOpenFileName(self, 'Choose publisher PUBLIC key',
                                               str(Path.home()), 'Public PEM (*.pem)')
            if not v:
                return
            public_path = Path(v)
        self._run_job(commands.inspect(Path(p), public_path))

    def doctor(self):
        root = self._firmware()
        if root:
            self._run_job(commands.doctor(root))

    def beta_preflight(self):
        root = self._firmware()
        if root:
            self._run_job(commands.beta_preflight(root))

    def run_tests(self):
        val = self.config.data['firmware_root']
        fw = Path(val) if val and Path(val).is_dir() else None
        self._run_job(commands.tests(fw))

    def _refresh_replay(self):
        size = len(self.replay_events)
        self.replay_summary.setText(
            f'{size} key event(s) / 128' + (' · UNSAVED' if self.replay_dirty else '') + '. F8 replays saved tests/input_replay.json.'
            if size else 'No recorded key events. F8 plays Start automatically.')

    def _load_replay(self):
        self.replay_events = []
        self.replay_dirty = False
        if self.workspace:
            try:
                doc = self.workspace.read('tests/input_replay.json')
                self.replay_events = parse_replay(doc.text, 200)
            except WorkspaceError:
                pass  # A new project has no replay. Never auto-replace corrupt JSON.
            except ReplayError as exc:
                self._log('Existing replay JSON invalid (not overwritten): ' + str(exc))
        if self.replay_events:
            self.frames_spin.setValue(max(8, max(e.frame for e in self.replay_events) + 1))
        self._refresh_replay()

    def add_replay_event(self):
        if not self.workspace:
            self._error('Open a Lua project to record input.')
            return
        if len(self.replay_events) >= 128:
            self._error('Replay limit is 128 events.')
            return
        self.replay_events.append(ReplayEvent(
            self.frame_spin.value(), self.replay_key.currentText(),
            self.replay_state.currentIndex() == 0))
        self.replay_dirty = True
        self._refresh_replay()

    def undo_replay_event(self):
        if self.replay_events:
            self.replay_events.pop()
            self.replay_dirty = True
        self._refresh_replay()

    def save_replay(self):
        if not self.workspace:
            self._error('Open a project first')
            return False
        if not self._save_all_before_job():
            return False
        try:
            value = serialize_replay(self.replay_events, self.frames_spin.value())
            dest = self.workspace.root / 'tests'
            if not dest.exists():
                self.workspace.mkdir('tests')
            old = self.workspace.read('tests/input_replay.json') if (dest / 'input_replay.json').exists() else None
            saved = self.workspace.save('tests/input_replay.json', value, old.sha256 if old else None)
            edit = self.editors.get('tests/input_replay.json')
            if edit is not None:
                edit.setPlainText(value)
                edit.disk_sha = saved.sha256
                edit.document().setModified(False)
            self.replay_dirty = False
            self.refresh_tree()
            self._log(f'Saved {len(self.replay_events)} replay events to tests/input_replay.json')
            return True
        except (ReplayError, WorkspaceError, OSError) as exc:
            self._error(str(exc))
            return False

    def preview_lua(self):
        if not self._save_all_before_job():
            return
        if self.replay_dirty and not self.save_replay():
            return
        from tools.qstudio import validate
        try:
            obj, _ = validate(self.workspace.root)
            if obj['type']!='lua':
                raise ValueError('This action requires a type=lua project')
        except (OSError, ValueError) as exc:
            self._error(str(exc))
            return
        dest = self.workspace.root/'build'/'host-lua-preview.png'
        self._run_job(commands.lua_preview(self.workspace.root,dest,self.frames_spin.value()))

    def preview_host(self):
        demo = self.demo.currentText()
        scenario = self.scenario.currentText()
        out = commands.ROOT / 'build' / 'host_preview' / (demo + '_' + scenario + '.png')
        self._run_job(commands.simulate(demo, scenario, out))

    def showEvent(self, event):
        super().showEvent(event)
        # Re-apply compact pane sizes after the real layout is active
        # (Qt redistributes splitter sizes during the first show).
        if not getattr(self, '_compact_layout_applied', False):
            self._compact_layout_applied = True
            from PySide6.QtCore import QTimer
            QTimer.singleShot(0, self._apply_compact_splitters)

    def _apply_compact_splitters(self):
        if self.ui_settings.value('ui/mainSplitter'):
            return
        self.content_splitter.setSizes([175, 920, 255])
        self.editor_splitter.setSizes([640, 110])
        self.right_tabs.setMinimumWidth(230)

    def closeEvent(self, event):
        self.device.stop(silent=True)
        win = getattr(self, 'phone_window', None)
        if win is not None:
            win.close()
        if self.thread is not None:
            self.stop_job()
            QMessageBox.information(self, 'Active process',
                                    'Wait for the active job to finish stopping before closing the IDE.')
            event.ignore()
            return
        if not self._close_all_tabs():
            event.ignore()
            return
        self.ui_settings.setValue('ui/windowGeometry',self.saveGeometry())
        self.ui_settings.setValue('ui/mainSplitter',self.content_splitter.saveState())
        self.ui_settings.setValue('ui/editorSplitter',self.editor_splitter.saveState())
        event.accept()
