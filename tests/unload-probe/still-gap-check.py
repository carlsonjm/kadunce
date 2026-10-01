#!/usr/bin/env python3
"""Check that a Stack keeps its distance from its neighbour in every photograph.

Usage: still-gap-check.py SAMPLES STACK_COLOURS SCREEN_WIDTH
SAMPLES holds one capture-extents.py result per line; STACK_COLOURS is a comma
list of the Stack's colours. The lone card beside the Stack in the first
photograph is its neighbour. Every photograph where both edges facing each
other are on screen must show the same gap, within three pixels.
"""
import json
import sys

samples = [json.loads(line) for line in open(sys.argv[1]) if line.strip()]
stack = sys.argv[2].split(',')
width = int(sys.argv[3])


def stack_edges(sample):
    spans = [sample[c] for c in stack if sample.get(c)]
    return (min(s[0] for s in spans), max(s[1] for s in spans)) if spans else None


first = samples[0]
left, right = stack_edges(first)
lone = [c for c in first if c not in stack and first[c]]
before = [c for c in lone if first[c][1] <= left]
after = [c for c in lone if first[c][0] >= right]
if before:
    neighbour, side = max(before, key=lambda c: first[c][1]), -1
elif after:
    neighbour, side = min(after, key=lambda c: first[c][0]), 1
else:
    sys.exit('no lone card beside the Stack in the first photograph')
# The front card is the widest of the Stack's colours in the first photograph.
front = max((c for c in stack if first.get(c)), key=lambda c: first[c][1] - first[c][0])
gaps, faces = [], []
for sample in samples:
    edges, span, face = stack_edges(sample), sample.get(neighbour), sample.get(front)
    if not edges or not span:
        continue
    if side < 0 and 0 < edges[0] and span[1] < width and span[1] <= edges[0]:
        gaps.append(edges[0] - span[1])
        if face and face[0] > 0:
            faces.append(face[0] - span[1])
    elif side > 0 and edges[1] < width and span[0] > 0 and span[0] >= edges[1]:
        gaps.append(span[0] - edges[1])
        if face and face[1] < width:
            faces.append(span[0] - face[1])
print(f'fan gap to {neighbour}: {sorted(set(gaps))}; front card gap: {sorted(set(faces))}')
if len(gaps) < 5:
    sys.exit('too few photographs show the Stack beside its neighbour')
for name, values in (('fan', gaps), ('front card', faces)):
    if values and max(values) - min(values) > 3:
        sys.exit(f'the Stack\'s {name} moved against its neighbour by {max(values) - min(values)} px')
