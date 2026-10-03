#!/usr/bin/env python3
"""Photograph the private compositor once and print, as JSON, how much of each
named area is each colour.

Usage: capture-shares.py NAME=X,Y,WIDTH,HEIGHT ... -- RRGGBB ...
Areas are in the compositor's coordinates; the photograph spans them all.
"""
import json
import os
import sys

import dbus

split = sys.argv.index('--')
areas = {}
for spec in sys.argv[1:split]:
    name, rect = spec.split('=')
    areas[name] = tuple(int(v) for v in rect.split(','))
colours = [tuple(int(c[i:i + 2], 16) for i in (0, 2, 4)) for c in sys.argv[split + 1:]]
left = min(x for x, _, _, _ in areas.values())
top = min(y for _, y, _, _ in areas.values())
right = max(x + w for x, _, w, _ in areas.values())
bottom = max(y + h for _, y, _, h in areas.values())
bus = dbus.SessionBus()
shot = dbus.Interface(bus.get_object('org.kde.KWin', '/org/kde/KWin/ScreenShot2'),
                      'org.kde.KWin.ScreenShot2')
read_end, write_end = os.pipe()
meta = shot.CaptureArea(left, top, right - left, bottom - top, {'native-resolution': False},
                        dbus.types.UnixFd(write_end))
os.close(write_end)
data = bytearray()
with os.fdopen(read_end, 'rb') as pipe:
    while chunk := pipe.read(1 << 16):
        data += chunk
stride = int(meta['stride'])
result = {}
for name, (x, y, w, h) in areas.items():
    counts = [0] * len(colours)
    for row in range(y - top, y - top + h):
        base = row * stride
        for col in range(x - left, x - left + w):
            b, g, r = data[base + col * 4:base + col * 4 + 3]
            for i, (cr, cg, cb) in enumerate(colours):
                if abs(r - cr) <= 6 and abs(g - cg) <= 6 and abs(b - cb) <= 6:
                    counts[i] += 1
                    break
    result[name] = {sys.argv[split + 1 + i]: round(n / (w * h), 3) for i, n in enumerate(counts)}
print(json.dumps(result))
