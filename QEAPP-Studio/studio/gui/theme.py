"""QEAPP Studio dark navy theme, aligned with the LuaS30-IDE visual language.

Palette: ink #111122 / surface #19192e / border #303049 / accent #ff8a00.
"""
APP_STYLE = '''
QMainWindow,QDialog,QWidget { background:#111122; color:#f1f2f7; font-family:'Segoe UI',Arial,sans-serif; font-size:12px; }
QMenuBar { background:#151527; color:#d8dbe7; border-bottom:1px solid #24243d; padding:1px 4px; }
QMenuBar::item { background:transparent; padding:5px 10px; border-radius:4px; }
QMenuBar::item:selected { background:#23233c; }
QMenu { background:#19192e; color:#d8dbe7; border:1px solid #303049; border-radius:6px; padding:5px; }
QMenu::item { padding:6px 24px 6px 14px; border-radius:4px; }
QMenu::item:selected { background:#1c2a3b; color:#ffffff; }
QMenu::item:disabled { color:#6d7290; }
QMenu::separator { height:1px; background:#24243d; margin:5px 8px; }
QMenu::indicator { width:12px; height:12px; }
QToolBar { background:#151527; border:0; spacing:4px; }
QStatusBar { background:#151527; color:#8b91aa; font-size:11px; border-top:1px solid #24243d; }
QSplitter::handle { background:#24243d; }
QSplitter::handle:hover { background:#575774; }
QSplitter::handle:horizontal { width:3px; }
QSplitter::handle:vertical { height:3px; }
QTreeWidget,QPlainTextEdit,QLineEdit,QComboBox,QSpinBox,QTextEdit { background:#19192e; color:#d8dbe7;
  border:1px solid #303049; border-radius:6px; selection-background-color:#41415b; selection-color:#ffffff; padding:4px 6px; }
QLineEdit,QComboBox,QSpinBox { min-height:24px; }
QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QPlainTextEdit:focus,QTextEdit:focus { border-color:#575774; }
QLineEdit:disabled,QComboBox:disabled { color:#6d7290; background:#151527; }
QComboBox::drop-down { border:0; width:22px; background:transparent; }
QComboBox::down-arrow { image:none; border-left:4px solid transparent; border-right:4px solid transparent; border-top:5px solid #9ca2bb; margin-right:7px; }
QComboBox QAbstractItemView { background:#19192e; color:#d8dbe7; border:1px solid #303049;
  selection-background-color:#1c2a3b; outline:0; }
QTreeWidget { border:0; outline:0; padding:2px; background:#111122; }
QTreeWidget::item { min-height:22px; padding:1px 2px; border-radius:4px; }
QTreeWidget::item:hover { background:#23233c; }
QTreeWidget::item:selected { background:#41415b; color:#ffffff; }
QTreeWidget::branch { background:#111122; }
QTabWidget::pane { border:0; background:#19192e; }
QTabBar::tab { background:#151527; color:#8b91aa; padding:7px 14px; margin-right:1px;
  border-top-left-radius:5px; border-top-right-radius:5px; min-width:70px; }
QTabBar::tab:selected { background:#19192e; color:#ffffff; border-top:2px solid #ff8a00; }
QTabBar::tab:hover:!selected { background:#1c1c33; color:#d8dbe7; }
QPushButton { background:#1c1c33; color:#f1f2f7; border:1px solid #303049;
  border-radius:6px; padding:5px 12px; min-height:20px; }
QPushButton:hover { background:#23233c; border-color:#424262; }
QPushButton:pressed { background:#19192e; }
QPushButton:disabled { color:#6d7290; background:#151527; border-color:#24243d; }
QPushButton:checked { background:#2c2c4e; border-color:#575774; }
QPushButton#PrimaryButton { background:#ff8a00; border-color:#ff8a00; color:#1a1a2c; font-weight:700; padding:6px 16px; }
QPushButton#PrimaryButton:hover { background:#ff9a22; border-color:#ff9a22; }
QPushButton#PrimaryButton:pressed { background:#e87a00; border-color:#e87a00; }
QPushButton#PrimaryButton:disabled { background:#3a3227; border-color:#3a3227; color:#7d7466; }
QPushButton#ActivityButton { background:transparent; border:0; border-left:3px solid transparent;
  border-radius:0; min-height:38px; font-size:18px; color:#8b91aa; }
QPushButton#ActivityButton:hover { background:#23233c; color:#d8dbe7; }
QPushButton#ActivityButton:checked { background:#2c2c4e; border-left:3px solid #ff8a00; color:#ff8a00; }
QLabel#PanelTitle { font-size:10px; color:#8b91aa; font-weight:600; letter-spacing:0.4px; }
QLabel#Subtle { color:#8b91aa; font-size:10px; }
QLabel#MetricBadge { background:#18392f; color:#7bd8ad; padding:4px 7px; border-radius:6px; font-family:Consolas; }
QLabel#PhoneBrand { color:#9ca2bb; font-size:11px; font-weight:700; }
QLabel#notice { color:#ff9b32; }
QFrame#ActivityRail,QFrame#ExplorerDock { background:#151527; border-right:1px solid #24243d; }
QFrame#WorkbenchBar { background:#19192e; border-bottom:1px solid #24243d; }
QFrame#BottomPanel { background:#151527; border-top:1px solid #24243d; }
QFrame#PhoneBezel { background:#202038; border:1px solid #3a3a56; border-radius:22px; }
QPushButton#DpadButton { background:#2c2c4e; font-weight:600; color:#f1f2f7; }
QPushButton#DpadButton:hover { background:#3a3a56; }
QPushButton#SystemButton { background:#23233c; font-size:10px; color:#9ca2bb; }
QPushButton#SystemButton:hover { background:#2c2c4e; }
QGroupBox { border:1px solid #303049; border-radius:6px; margin-top:6px; font-weight:600; padding-top:13px; }
QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; color:#9ca2bb; }
QScrollArea { border:0; background:transparent; }
QScrollBar:vertical { background:#151527; width:10px; margin:2px; }
QScrollBar::handle:vertical { background:#3a3a56; border-radius:5px; min-height:32px; }
QScrollBar::handle:vertical:hover { background:#575774; }
QScrollBar:horizontal { background:#151527; height:10px; margin:2px; }
QScrollBar::handle:horizontal { background:#3a3a56; border-radius:5px; min-width:32px; }
QScrollBar::handle:horizontal:hover { background:#575774; }
QScrollBar::add-line,QScrollBar::sub-line { width:0; height:0; }
QScrollBar::add-page,QScrollBar::sub-page { background:transparent; }
QToolTip { background:#202038; color:#f1f2f7; border:1px solid #3a3a56; padding:4px 6px; font-size:11px; }
QCheckBox { spacing:6px; color:#d8dbe7; }
QCheckBox::indicator { width:14px; height:14px; border:1px solid #3a3a56; border-radius:3px; background:#19192e; }
QCheckBox::indicator:checked { background:#ff8a00; border-color:#ff8a00; }
QCheckBox:hover { color:#ffffff; }
QProgressBar { background:#151527; border:1px solid #24243d; border-radius:4px; height:6px; text-align:center; color:transparent; }
QProgressBar::chunk { background:#ff8a00; border-radius:3px; }
QMessageBox,QInputDialog { background:#19192e; }
QMessageBox QPushButton,QInputDialog QPushButton { min-width:76px; }
QFrame#PanelFrame { background:#111122; border:1px solid #24243d; border-radius:6px; }
QLabel#SidePanelTitle { font-size:10px; font-weight:600; letter-spacing:0.4px; color:#8b91aa; }
'''
