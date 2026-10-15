#!/usr/bin/env python3
"""Holds a context-line claim on one display for as long as it runs, as a
status bar does (TETTEGOUCHE-CONTEXT.md § Context-line claims), then leaves the
bus, which clears it."""
import sys
import time

output = sys.argv[1]
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 60
try:
    import dbus

    bus = dbus.SessionBus()
    kadunce = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "co.goodinput.Kadunce")
    claimed = bool(kadunce.claimContextLine("scene", output))
except ImportError:
    from gi.repository import Gio, GLib

    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    reply = bus.call_sync("org.kde.KWin", "/Kadunce", "co.goodinput.Kadunce", "claimContextLine",
                          GLib.Variant("(ss)", ("scene", output)), GLib.VariantType("(b)"),
                          Gio.DBusCallFlags.NONE, -1, None)
    claimed = reply.unpack()[0]
print("claimed" if claimed else "refused", flush=True)
time.sleep(seconds)
