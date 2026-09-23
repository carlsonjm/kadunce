#!/usr/bin/env python3
"""Photograph a band of the private compositor and print how much of it is one colour.

Usage: capture-band.py X Y WIDTH HEIGHT RRGGBB
Prints the fraction of pixels within a small tolerance of the colour.
"""
import os
import sys

import dbus

x, y, width, height = (int(v) for v in sys.argv[1:5])
target = tuple(int(sys.argv[5][i:i + 2], 16) for i in (0, 2, 4))
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
matches = 0
for row in range(h):
    base = row * stride
    for col in range(w):
        b, g, r = data[base + col * 4:base + col * 4 + 3]
        if abs(r - target[0]) <= 6 and abs(g - target[1]) <= 6 and abs(b - target[2]) <= 6:
            matches += 1
print(f'{matches / (w * h):.3f}')
