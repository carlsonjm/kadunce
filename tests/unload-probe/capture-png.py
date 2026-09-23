#!/usr/bin/env python3
"""Photograph an area of the private compositor into a PNG file.

Usage: capture-png.py X Y WIDTH HEIGHT PATH
"""
import os
import struct
import sys
import zlib

import dbus

x, y, width, height = (int(v) for v in sys.argv[1:5])
path = sys.argv[5]
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
rows = bytearray()
for row in range(h):
    rows.append(0)
    line = data[row * stride:row * stride + w * 4]
    for col in range(w):
        b, g, r = line[col * 4:col * 4 + 3]
        rows += bytes((r, g, b))


def chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))


with open(path, 'wb') as out:
    out.write(b'\x89PNG\r\n\x1a\n')
    out.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)))
    out.write(chunk(b'IDAT', zlib.compress(bytes(rows), 6)))
    out.write(chunk(b'IEND', b''))
