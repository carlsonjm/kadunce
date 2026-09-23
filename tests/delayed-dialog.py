#!/usr/bin/env python3
"""A window that opens a confirmation over itself a few seconds after a tap.

For hand-testing a dialog that arrives while its application is not in front:
tap the button, then go to another card or to Spread before it opens. The
confirmation names this window as its parent, as an application's own dialog
does.
"""
import sys

from PyQt6.QtCore import QTimer
from PyQt6.QtWidgets import QApplication, QLabel, QMessageBox, QPushButton, QVBoxLayout, QWidget

DELAY_SECONDS = 8


def main():
    app = QApplication(sys.argv)
    window = QWidget()
    window.setWindowTitle('Delayed dialog')
    window.resize(640, 420)
    layout = QVBoxLayout(window)
    label = QLabel(f'Tap the button, then leave: a confirmation opens here '
                   f'{DELAY_SECONDS} seconds later.')
    label.setWordWrap(True)
    button = QPushButton(f'Open a confirmation in {DELAY_SECONDS} seconds')
    button.setMinimumHeight(64)
    layout.addWidget(label)
    layout.addWidget(button)

    def confirm():
        box = QMessageBox(QMessageBox.Icon.Question, 'Delayed confirmation',
                          'This opened while you were elsewhere. Keep it?',
                          QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No, window)
        box.open()

    button.clicked.connect(lambda: QTimer.singleShot(DELAY_SECONDS * 1000, confirm))
    window.show()
    sys.exit(app.exec())


if __name__ == '__main__':
    main()
