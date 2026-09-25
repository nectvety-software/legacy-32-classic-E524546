"""Standalone virtual phone window (LuaS30-IDE style device shell).

F9 / Run opens this floating window with the same VirtualPhone widget
reparented here so the IDE panes stay compact.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import QHBoxLayout, QLabel, QVBoxLayout, QWidget

__all__ = ['PhoneWindow']


class PhoneWindow(QWidget):
    """Cửa sổ máy ảo riêng — giống LuaS30-IDE mở emulator window khi Run."""

    closed = Signal()

    def __init__(self, device, parent=None) -> None:
        super().__init__(parent, Qt.Window)
        self.setWindowTitle('VQEAF Virtual Phone — Host Lua VM')
        self.setObjectName('PhoneWindow')
        self.setWindowFlag(Qt.WindowMaximizeButtonHint, False)
        self._device = device
        self._host_tab = None

        root = QVBoxLayout(self)
        root.setContentsMargins(10, 8, 10, 8)
        root.setSpacing(6)

        header = QHBoxLayout()
        title = QLabel('VIRTUAL PHONE  ·  240 × 320')
        title.setObjectName('PanelTitle')
        hint = QLabel('PC host Lua VM  ·  not ESP32 / Symbian hardware')
        hint.setObjectName('Subtle')
        header.addWidget(title)
        header.addStretch(1)
        header.addWidget(hint)
        root.addLayout(header)

        body = QHBoxLayout()
        body.setSpacing(10)
        body.addWidget(device, 1)
        root.addLayout(body)

        self.resize(360, 620)
        self.setMinimumSize(320, 520)

    def attach_device(self, device, host_tab=None) -> None:
        """Embed VirtualPhone; *host_tab* is where it should return on close."""
        self._device = device
        self._host_tab = host_tab
        if device.parent() is not self:
            device.setParent(self)
            # re-insert into the body layout
            layout = self.layout().itemAt(1).layout()
            while layout.count():
                item = layout.takeAt(0)
                if item.widget() and item.widget() is not device:
                    item.widget().setParent(None)
            layout.addWidget(device, 1)
            device.show()

    def detach_device(self) -> None:
        """Return the device widget to its IDE tab host if needed."""
        if self._device is not None and self._host_tab is not None:
            self._device.setParent(self._host_tab)

    def show_and_raise(self) -> None:
        self.show()
        self.raise_()
        self.activateWindow()
        if self._device is not None:
            self._device.setFocus(Qt.OtherFocusReason)

    def closeEvent(self, event) -> None:
        # Keep the VM alive only while the phone window is the run surface;
        # do not silently kill a guest here — Studio stop_job owns that.
        self.closed.emit()
        super().closeEvent(event)
