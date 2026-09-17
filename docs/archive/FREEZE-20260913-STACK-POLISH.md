# Accepted stack polish — September 13

J accepted and authorized freeze/publication of the accumulated stack polish.
Includes the previously local dd047b5 directional browse animation.

## Scope

- Lift exposes the nearest existing shoulder, preserving remainder order.
- Pickup and held paging synchronize destination back-to-front native stacking;
  the held card stays elevated.
- Tette-centered neighbors use a compact, selected-relative deck rather than
  retaining the formerly selected open fan.
- Neighbor shoulders share 40% of the visible side peek equally: two shoulders
  for three cards, three for four or more. The compact inward boundary remains
  fixed, preserving the gap. Centered browse and insertion remain unchanged.
- No new input timing, cache, native geometry or ownership policy.

## Evidence and provenance

Installed plugin matches the accepted candidate SHA-256:
`fc3126e8b0864c13cbc12548b264f7e979f385fcf50863ee212e412040e22efe`.

Focused row/stack motion, card model/layout tests and source/control checks pass.
Model tests cover every lifted face of a four-stack and cancellation. Geometry
tests cover equal shoulders and preserved inward boundaries on both sides for
three/four cards. J passed the physical rounds, including the final spacing.

Local opt-in installer: `../install-kadunce-even-neighbor-20260913.sh`.
Its `--rollback` restores the compact-neighbor artifact `70fdce52`.
Earlier held-order and browse bundles remain available under `../work`.
These artifacts are local; fresh clones build through the normal installer.
KWin engine and trusted repair are unchanged. Unrelated NEXT-ROADMAP edits are
excluded from this freeze. Zen sampling remains a separate known issue.
