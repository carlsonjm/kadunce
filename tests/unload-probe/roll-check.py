#!/usr/bin/env python3
"""Reads one keyboard motion recorded frame by frame and checks the card above.

Each sample is [milliseconds, keys' top edge or null, card's bottom edge]. The
card is drawn ending a gutter above the keys whatever size its client has, so
the gap seen on screen is the larger of the gutter and the distance from the
client's own bottom edge to the keys.

    roll-check.py GUTTER CATCH-UP-MS MAX-RESIZES MIN-STEPS < history.json

Fails when the gap seen is wider than the gutter later than CATCH-UP-MS after
the keys first move, when the client was asked for more sizes than
MAX-RESIZES in the motion, or when the keys were seen in fewer than MIN-STEPS
places on the way, as keys that vanish at once are. Prints what it measured
either way.
"""
import json
import sys

gutter, catch_up = float(sys.argv[1]), float(sys.argv[2])
max_resizes, min_steps = int(sys.argv[3]), int(sys.argv[4])
samples = [s for s in json.load(sys.stdin) if len(s) >= 3]
if not samples:
    sys.exit("no frames recorded")
start_keys = samples[0][1]
moved = next((s[0] for s in samples if s[1] != start_keys), None)
if moved is None:
    sys.exit("the keys never moved")
late = []
widest = gutter
for t, keys, bottom in (s[:3] for s in samples):
    if keys is None:
        continue
    seen = max(gutter, keys - bottom)
    widest = max(widest, seen)
    if seen > gutter + 1 and t - moved > catch_up:
        late.append((round(t - moved), round(seen)))
bottoms = [s[2] for s in samples]
resizes = sum(1 for a, b in zip(bottoms, bottoms[1:]) if a != b)
steps = len({s[1] for s in samples if s[1] is not None})
print(f"frames={len(samples)} steps={steps} widest={round(widest)} resizes={resizes} late={late[:5]}")
if late or resizes > max_resizes or steps < min_steps:
    sys.exit(1)
