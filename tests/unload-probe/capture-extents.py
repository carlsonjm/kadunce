#!/usr/bin/env python3
"""Photograph a band of the private compositor and say where each colour is.

Usage: capture-extents.py X Y WIDTH HEIGHT RRGGBB [RRGGBB ...]
Prints a JSON object mapping each colour to [left, right] in screen
coordinates: the outermost columns where at least a quarter of the band is
within a small tolerance of it, or null where it does not appear.
"""
import json
import os
import sys

import dbus

x, y, width, height = (int(v) for v in sys.argv[1:5])
colours = sys.argv[5:]
targets = {c: tuple(int(c[i:i + 2], 16) for i in (0, 2, 4)) for c in colours}
bus = dbus.SessionBus()
shot = dbus.Interface(bus.get_object('org.kde.KWin', '/org/kde/KWin/ScreenShot2'),
                      'org.kde.KWin.ScreenShot2')
read_end, write_end = os.pipe()
meta = shot.CaptureArea(x, y, width, height, {'native-resolution': False},
                        dbus.types.UnixFd(write_end))
os.close(write_end)
data = bytearray()
with os.fdopen(read_end, 'rb') as pipe:
    while chunk := pipe.read(1 << 16):
        data += chunk
w, h, stride = int(meta['width']), int(meta['height']), int(meta['stride'])
counts = {c: [0] * w for c in colours}
for row in range(h):
    base = row * stride
    for col in range(w):
        b, g, r = data[base + col * 4:base + col * 4 + 3]
        for c, (tr, tg, tb) in targets.items():
            if abs(r - tr) <= 24 and abs(g - tg) <= 24 and abs(b - tb) <= 24:
                counts[c][col] += 1
                break
scale = width / w
result = {}
for c in colours:
    cols = [i for i, n in enumerate(counts[c]) if n * 4 >= h]
    result[c] = [round(x + cols[0] * scale), round(x + (cols[-1] + 1) * scale)] if cols else None
print(json.dumps(result))
