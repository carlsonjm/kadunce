#!/usr/bin/env python3
"""A GTK 3 window whose message box names its parent only once it is shown.

Electron shows a native message box before the compositor knows whose it is:
the parent is exported from the browser's own connection and imported on the
box's, a round trip after the box appears. SIGUSR1 opens one here the same way,
naming the parent `delay` milliseconds after showing it; SIGUSR2 closes it.
"""
import signal
import sys

import gi

gi.require_version('Gtk', '3.0')
from gi.repository import GLib, Gtk  # noqa: E402

delay = int(sys.argv[1]) if len(sys.argv) > 1 else 30
window = Gtk.Window(title='Late parent probe')
window.set_default_size(560, 420)
window.connect('destroy', Gtk.main_quit)
window.show_all()
boxes = []


def name_parent(box):
    box.set_transient_for(window)
    return False


def open_box():
    box = Gtk.MessageDialog(message_type=Gtk.MessageType.QUESTION,
                            buttons=Gtk.ButtonsType.OK_CANCEL,
                            text='Open the page?')
    box.set_title('Late parent box')
    box.set_modal(True)
    box.connect('response', lambda b, _r: b.destroy())
    box.show()
    boxes.append(box)
    GLib.timeout_add(delay, name_parent, box)
    return True


def close_boxes():
    while boxes:
        boxes.pop().destroy()
    return True


GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGUSR1, open_box)
GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGUSR2, close_boxes)
Gtk.main()
