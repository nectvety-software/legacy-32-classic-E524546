"""QEAPP Studio M2 desktop IDE: project editor + original signer CLI + host preview.

The UI is intentionally thin. No duplicate package parser, no signature bypass,
no device executable runtime claims. Qt is the only optional desktop dependency.
"""
from __future__ import annotations

import json
from pathlib import Path

from PySide6.QtCore import QObject, Qt, QThread, Signal, Slot
from PySide6.QtGui import QAction, QColor, QFont, QPixmap, QSyntaxHighlighter, QTextCharFormat, QTextCursor
from PySide6.QtWidgets import (
    QApplication, QComboBox, QFileDialog, QFormLayout, QGroupBox,
    QHBoxLayout, QInputDialog, QLabel, QLineEdit, QMainWindow, QMessageBox,
    QPlainTextEdit, QPushButton, QSizePolicy, QScrollArea, QSpinBox, QSplitter, QStatusBar,
    QTabWidget, QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget
)

from studio.core import commands
from studio.core.config import StudioConfig
from studio.core.jobs import JobRunner
from studio.core.replays import ReplayEvent, ReplayError, parse as parse_replay, serialize as serialize_replay
from studio.core.workspace import Document, Workspace, WorkspaceError

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
            for match in re.finditer(r'--[^\n]*|#[^\n]*' if self.ext == '.py' else r'--[^\n]*', text):
                self.setFormat(match.start(), len(match.group()), self.comment)


class CodeEditor(QPlainTextEdit):
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
        self.setWindowTitle('QEAPP Studio v0.5 · Lua Sprite Beta')
        self.resize(1250, 790)
        self.setMinimumSize(900, 600)
        self.setStyleSheet(STYLE)
        self.config = StudioConfig()
        self.workspace: Workspace | None = None
        self.runner = JobRunner()
        self.thread: QThread | None = None
        self.worker: JobWorker | None = None
        self.actions_when_idle: list[QAction | QPushButton] = []
        self.editors: dict[str, CodeEditor] = {}
        self.image_previews: dict[str, QWidget] = {}
        self.replay_events: list[ReplayEvent] = []
        self.replay_dirty = False
        self._setup_ui()

    def _setup_ui(self):
        menu_file = self.menuBar().addMenu('&File')
        menu_run = self.menuBar().addMenu('&Tools')
        def action(menu, title, fn, shortcut=None):
            act = QAction(title, self)
            act.triggered.connect(fn)
            if shortcut:
                act.setShortcut(shortcut)
            menu.addAction(act)
            return act
        action(menu_file, 'New Text App…', lambda: self.create_project('text'))
        action(menu_file, 'New Web App…', lambda: self.create_project('web'))
        action(menu_file, 'New Lua App (Beta)…', lambda: self.create_project('lua'))
        action(menu_file, 'New Pixel Snake Lua Game (Beta)…', lambda: self.create_project('lua-snake'))
        action(menu_file, 'New Sprite Lua Game (Beta)…', lambda: self.create_project('lua-sprite'))
        action(menu_file, 'Open Project…', self.choose_project, 'Ctrl+O')
        action(menu_file, 'New Source File…', self.new_file)
        action(menu_file, 'Insert Pixel Sprite PNG…', self.insert_pixel_sprite)
        action(menu_file, 'New Folder…', self.new_folder)
        action(menu_file, 'Save', self.save_current, 'Ctrl+S')
        action(menu_file, 'Refresh Explorer', self.refresh_tree, 'F5')
        menu_file.addSeparator()
        action(menu_file, 'Close Project', self.close_project)
        self.act_validate = action(menu_run, 'Validate', self.validate_project, 'F6')
        self.act_build = action(menu_run, 'Build signed .qeapp', self.build_project, 'F7')
        self.act_inspect = action(menu_run, 'Inspect package', self.inspect_package)
        self.act_doctor = action(menu_run, 'Firmware doctor', self.doctor)
        self.act_beta_check = action(menu_run, 'Lua Beta Device Preflight…', self.beta_preflight)
        self.act_test = action(menu_run, 'Run host tests', self.run_tests)
        self.act_lua = action(menu_run, 'Preview Lua on PC', self.preview_lua, 'F8')
        menu_run.addSeparator()
        self.act_stop = action(menu_run, 'STOP active job', self.stop_job, 'Shift+F5')
        self.act_stop.setEnabled(False)

        content = QSplitter(Qt.Horizontal)
        explorer_panel = QWidget()
        explorer_layout = QVBoxLayout(explorer_panel)
        explorer_layout.setContentsMargins(4, 5, 4, 4)
        explorer_layout.addWidget(QLabel('PROJECT EXPLORER'))
        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        self.tree.itemDoubleClicked.connect(self.open_tree_file)
        explorer_layout.addWidget(self.tree, 1)
        self.lbl_project = QLabel('No project')
        self.lbl_project.setWordWrap(True)
        explorer_layout.addWidget(self.lbl_project)
        content.addWidget(explorer_panel)

        editor_group = QSplitter(Qt.Vertical)
        self.tabs = QTabWidget()
        self.tabs.setTabsClosable(True)
        self.tabs.tabCloseRequested.connect(self.close_tab)
        self.tabs.currentChanged.connect(self._on_tab_changed)
        editor_group.addWidget(self.tabs)
        logs_panel = QWidget()
        log_layout = QVBoxLayout(logs_panel)
        log_layout.setContentsMargins(1, 2, 1, 1)
        log_layout.addWidget(QLabel('BUILD · SIMULATION · DIAGNOSTICS'))
        self.logs = QPlainTextEdit()
        self.logs.setReadOnly(True)
        font = QFont('Consolas')
        font.setStyleHint(QFont.Monospace)
        self.logs.setFont(font)
        self.logs.document().setMaximumBlockCount(1800)
        log_layout.addWidget(self.logs)
        editor_group.addWidget(logs_panel)
        editor_group.setSizes([540, 200])
        content.addWidget(editor_group)

        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_layout.setContentsMargins(5, 4, 5, 4)
        right_layout.addWidget(QLabel('HOST PREVIEW · 240 × 320'))
        notice = QLabel('Native C++ previews + bounded Lua 5.4 host VM. Lua device build is EXPERIMENTAL and requires vqeaf_lua_beta; hardware NOT tested.')
        notice.setObjectName('notice')
        notice.setWordWrap(True)
        right_layout.addWidget(notice)
        self.preview = QLabel('Run Host Snake / Hello')
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
        self.btn_preview = QPushButton('▶ Run Host Preview')
        self.btn_preview.clicked.connect(self.preview_host)
        right_layout.addWidget(self.btn_preview)
        self.btn_lua = QPushButton('▶ Run Lua Preview (F8)')
        self.btn_lua.clicked.connect(self.preview_lua)
        right_layout.addWidget(self.btn_lua)
        replay_group = QGroupBox('GAMEPAD REPLAY · host VM only')
        replay_controls = QVBoxLayout(replay_group)
        replay_row = QHBoxLayout()
        self.frames_spin = QSpinBox()
        self.frames_spin.setRange(1, 200)
        self.frames_spin.setValue(8)
        self.frames_spin.setPrefix('Frames: ')
        replay_row.addWidget(self.frames_spin)
        self.frame_spin = QSpinBox()
        self.frame_spin.setRange(0, 7)
        self.frame_spin.setPrefix('At: ')
        self.frames_spin.valueChanged.connect(lambda n: self.frame_spin.setMaximum(n - 1))
        replay_row.addWidget(self.frame_spin)
        replay_controls.addLayout(replay_row)
        key_row = QHBoxLayout()
        self.replay_key = QComboBox()
        self.replay_key.addItems(['start', 'up', 'down', 'left', 'right', 'option'])
        self.replay_state = QComboBox()
        self.replay_state.addItems(['Press', 'Release'])
        key_row.addWidget(self.replay_key)
        key_row.addWidget(self.replay_state)
        replay_controls.addLayout(key_row)
        buttons = QHBoxLayout()
        add_event = QPushButton('+ Event')
        add_event.clicked.connect(self.add_replay_event)
        buttons.addWidget(add_event)
        remove_event = QPushButton('Undo event')
        remove_event.clicked.connect(self.undo_replay_event)
        buttons.addWidget(remove_event)
        replay_controls.addLayout(buttons)
        self.replay_summary = QLabel('No events recorded; F8 runs default Start.')
        self.replay_summary.setWordWrap(True)
        replay_controls.addWidget(self.replay_summary)
        save_replay = QPushButton('Save Replay JSON')
        save_replay.clicked.connect(self.save_replay)
        replay_controls.addWidget(save_replay)
        right_layout.addWidget(replay_group)
        self.btn_stop = QPushButton('■ Stop Build / Run')
        self.btn_stop.clicked.connect(self.stop_job)
        right_layout.addWidget(self.btn_stop)
        self.btn_stop.setEnabled(False)
        self.btn_firmware = QPushButton('Choose Firmware Source…')
        self.btn_firmware.clicked.connect(self.select_firmware)
        right_layout.addWidget(self.btn_firmware)
        self.lbl_firmware = QLabel('Firmware path: not selected')
        self.lbl_firmware.setWordWrap(True)
        right_layout.addWidget(self.lbl_firmware)
        self.lbl_mode = QLabel('Export: QEAPP/2 text/web only')
        self.lbl_mode.setWordWrap(True)
        right_layout.addWidget(self.lbl_mode)
        right_layout.addStretch(1)
        right_scroller = QScrollArea()
        right_scroller.setWidgetResizable(True)
        right_scroller.setFrameShape(QScrollArea.NoFrame)
        right_scroller.setWidget(right_panel)
        content.addWidget(right_scroller)
        content.setSizes([225, 710, 310])
        self.setCentralWidget(content)
        self.setStatusBar(QStatusBar(self))
        self.actions_when_idle = [self.act_validate, self.act_build, self.act_inspect,
                                  self.act_doctor, self.act_beta_check, self.act_test, self.act_lua, self.btn_preview, self.btn_lua]
        self._log('QEAPP Studio v0.5 — bounded Lua VM, sprite import + deterministic D-pad replay.\n')
        self._set_project(None)
        last = self.config.data['recent_projects']
        if last:
            try:
                self._set_project(Path(last[0]))
            except (WorkspaceError, OSError):
                self._log('Recent project not found; choose project manually.\n')
        if self.config.data['firmware_root']:
            self.lbl_firmware.setText('Firmware: ' + self.config.data['firmware_root'])

    def _log(self, message: str):
        self.logs.moveCursor(QTextCursor.End)
        self.logs.insertPlainText(message if message.endswith('\n') else message + '\n')
        self.logs.ensureCursorVisible()

    def _error(self, text: str):
        self._log('ERROR: ' + text)
        QMessageBox.warning(self, 'QEAPP Studio', text)

    def _set_project(self, path: Path | None):
        if path is not None:
            workspace = Workspace(path)
            if not self._close_all_tabs():
                return
            self.workspace = workspace
            self.config.remember(workspace.root)
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
            self.tree.clear()
            self.lbl_project.setText('No project open')
            self.lbl_mode.setText('QEAPP/2: text/web stable; Lua app: beta target only')
            self.replay_events = []
            self.replay_dirty = False
            self._refresh_replay()
        self.refresh_tree()

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
        from tools.qstudio import init_project
        app_id, ok = QInputDialog.getText(self, 'New QEAPP project', 'App ID (a-z 0-9 _ -):')
        if not ok:
            return
        name, ok = QInputDialog.getText(self, 'New QEAPP project', 'Display name (ASCII):')
        if not ok:
            return
        folder = QFileDialog.getExistingDirectory(self, 'Choose parent folder')
        if not folder:
            return
        try:
            target = Path(folder) / app_id
            init_project(kind, target, app_id, name)
            self._set_project(target)
            self.open_document('qeapp.project.json')
        except (OSError, ValueError, KeyError) as exc:
            self._error(str(exc))

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
        folder = QFileDialog.getExistingDirectory(self, 'Choose VQEAF OS firmware source')
        if folder:
            root = Path(folder)
            if not (root / 'tools' / 'build_qeapp.py').is_file():
                self._error('Choose VQEAF OS source containing tools/build_qeapp.py')
                return
            self.config.update(firmware_root=str(root.resolve()))
            self.lbl_firmware.setText('Firmware: ' + str(root.resolve()))

    def _firmware(self) -> Path | None:
        val = self.config.data['firmware_root']
        if val and (Path(val) / 'tools' / 'build_qeapp.py').is_file():
            return Path(val)
        self._error('Select local VQEAF-OS source first (not the Studio folder).')
        return None

    def _run_job(self, spec: commands.JobSpec):
        if self.thread is not None:
            self._log('Busy. Stop the active job first.')
            return
        self._log('\n━━ ' + spec.label + ' ━━')
        self.thread = QThread(self)
        self.worker = JobWorker(self.runner, spec)
        self.worker.moveToThread(self.thread)
        self.thread.started.connect(self.worker.run)
        self.worker.output.connect(self._log)
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

    @Slot(int, str, str)
    def _job_finished(self, code: int, label: str, preview: str):
        self._log(('PASS' if code == 0 else f'FAIL exit {code}') + ' — ' + label + '\n')
        if preview:
            pix = QPixmap(preview)
            if not pix.isNull():
                self.preview.setPixmap(pix.scaled(240, 320, Qt.KeepAspectRatio,
                                                 Qt.FastTransformation))
        self.statusBar().showMessage(('PASS' if code == 0 else 'FAILED') + ': ' + label, 10000)

    @Slot()
    def _thread_finished(self):
        if self.thread:
            self.thread.deleteLater()
        self.thread = None
        self.worker = None
        for action in self.actions_when_idle:
            action.setEnabled(True)
        self.act_stop.setEnabled(False)
        self.btn_stop.setEnabled(False)

    def stop_job(self):
        if self.runner.cancel():
            self._log('Stop requested; terminating worker process tree…')
        elif self.thread is not None:
            self._log('Job is starting/finishing; waiting for cleanup.')

    def validate_project(self):
        if self._save_all_before_job():
            self._run_job(commands.validate(self.workspace.root))

    def build_project(self):
        if not self._save_all_before_job():
            return
        from tools.qstudio import validate
        try:
            obj, _ = validate(self.workspace.root)
        except (OSError, ValueError) as exc:
            self._error('Validation failed: ' + str(exc))
            return
        if obj['type'] not in ('web', 'text', 'lua'):
            self._error('Supported project types are web/text/lua')
            return
        firmware = self._firmware()
        if firmware is None:
            return
        key_file, _ = QFileDialog.getOpenFileName(self, 'Choose P-256 private signing key (not saved)',
                                                  str(Path.home()), 'PEM private keys (*.pem *.key)')
        if not key_file:
            return
        key_id, ok = QInputDialog.getText(self, 'Publisher Key ID', 'Key ID (hex or decimal)',
                                          text='0x544c5541' if obj['type']=='lua' else '0x31534351')
        if not ok:
            return
        try:
            numeric = int(key_id, 0)
            if not 0 <= numeric <= 0xFFFFFFFF:
                raise ValueError('Key ID out of range')
        except ValueError as exc:
            self._error(str(exc))
            return
        dist = self.workspace.root / 'dist'
        dist.mkdir(exist_ok=True)
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

    def closeEvent(self, event):
        if self.thread is not None:
            self.stop_job()
            QMessageBox.information(self, 'Active process',
                                    'Wait for the active job to finish stopping before closing the IDE.')
            event.ignore()
            return
        if not self._close_all_tabs():
            event.ignore()
            return
        event.accept()
