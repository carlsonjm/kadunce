#!/usr/bin/env python3
"""Supply a private abstract X socket; never touch the user's X11 directory."""
import os
import socket
import sys

if not os.environ.get("XDG_RUNTIME_DIR", "").startswith("/tmp/kadunce-unload-"):
    raise SystemExit("Private probe runtime required")
listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
# Abstract sockets are namespace-local and vanish on close. Do not reuse :0.
for number in range(20000, 30000):
    try:
        listener.bind("\0/tmp/.X11-unix/X" + str(number))
        break
    except OSError:
        continue
else:
    raise SystemExit("No private X display available")
listener.listen()
listener.set_inheritable(True)
os.execvp(sys.argv[1], sys.argv[1:] + ["--xwayland-fd", str(listener.fileno()),
    "--xwayland-display", ":" + str(number)])
