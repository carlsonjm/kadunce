#!/usr/bin/env python3
"""A GTK 4 window whose only child is a focused text field.

GTK asks for text input the way Ghostty does: it never requests the keyboard
itself, so the compositor raises one whenever the field gains focus.
"""
import gi

gi.require_version('Gtk', '4.0')
from gi.repository import Gtk  # noqa: E402


def activate(app):
    window = Gtk.ApplicationWindow(application=app, title='Keyboard GTK probe')
    window.set_default_size(560, 420)
    field = Gtk.Entry()
    field.set_valign(Gtk.Align.START)
    window.set_child(field)
    window.present()
    field.grab_focus()


app = Gtk.Application(application_id='studio.warbler.KeyboardGtkProbe')
app.connect('activate', activate)
app.run(None)
