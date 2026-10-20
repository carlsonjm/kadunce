#!/usr/bin/env python3
"""Photograph an area of the private compositor and print its contrast.

Usage: capture-contrast.py X Y WIDTH HEIGHT
Prints the spread of luminance across the area, 0 to 255: stripes drawn
sharp read near 255, the same stripes blurred near 0.
"""
import os
import sys

import dbus

x, y, width, height = (int(v) for v in sys.argv[1:5])
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
lights = []
for row in range(h):
    base = row * stride
    for col in range(w):
        b, g, r = data[base + col * 4:base + col * 4 + 3]
        lights.append(0.2126 * r + 0.7152 * g + 0.0722 * b)
print(round(max(lights) - min(lights)))
