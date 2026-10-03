#!/usr/bin/env python3
"""A GTK 3 window titled by its argument, drawing its own title bar.

GTK 3 keeps an invisible resize border outside the window's frame on every
side, the top included, as Firefox-based browsers do.
"""
import sys

import gi

gi.require_version('Gtk', '3.0')
from gi.repository import Gtk  # noqa: E402

window = Gtk.Window(title=sys.argv[1])
bar = Gtk.HeaderBar(title=sys.argv[1], show_close_button=True)
window.set_titlebar(bar)
window.set_default_size(560, 420)
window.connect('destroy', Gtk.main_quit)
window.show_all()
Gtk.main()
