#!/usr/bin/env python3
"""A GTK 4 window whose only child is a focused text field.

GTK asks for text input the way Ghostty does: it never requests the keyboard
itself, so the compositor raises one whenever the field gains focus.

With --refocus the window also holds a button, and SIGUSR1 has the application
move its focus to the button and back into the field, as an application
taking the focus back into its own text box does with nobody touching it.
With --centre the field stands halfway down instead, as a search box centred
on a page does, and a second field sits at the top edge.
"""
import signal
import sys

import gi

gi.require_version('Gtk', '4.0')
from gi.repository import GLib, Gtk  # noqa: E402

REFOCUS = '--refocus' in sys.argv[1:]
CENTRE = '--centre' in sys.argv[1:]


def activate(app):
    title = 'Keyboard GTK centre probe' if CENTRE else 'Keyboard GTK probe'
    window = Gtk.ApplicationWindow(application=app, title=title)
    window.set_default_size(560, 420)
    field = Gtk.Entry()
    field.set_valign(Gtk.Align.CENTER if CENTRE else Gtk.Align.START)
    field.set_vexpand(CENTRE)
    if REFOCUS:
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        elsewhere = Gtk.Button(label='Elsewhere')
        elsewhere.set_valign(Gtk.Align.END)
        elsewhere.set_vexpand(not CENTRE)
        if CENTRE:
            box.append(Gtk.Entry())
        box.append(field)
        box.append(elsewhere)
        window.set_child(box)

        def back():
            field.grab_focus()
            return GLib.SOURCE_REMOVE

        def refocus():
            elsewhere.grab_focus()
            GLib.timeout_add(300, back)
            return GLib.SOURCE_CONTINUE

        GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGUSR1, refocus)
    else:
        window.set_child(field)
    window.present()
    field.grab_focus()


app = Gtk.Application(application_id='studio.warbler.KeyboardGtkCentreProbe' if CENTRE
                      else 'studio.warbler.KeyboardGtkProbe')
app.connect('activate', activate)
app.run(None)
