# Edge ownership audit — 2026-09-11

September12 superseding direction: J explicitly approves replacing automatic KDE
edge placement for ordinary windows too, while Kadunce is enabled. The historical
unmanaged-window exception below is no longer policy. NativeEdgePolicy provides
runtime-only electric-border tiling/maximize suppression; explicit Shift-custom
tiling is separate and remains uncovered. See CURRENT_STATE for verification.

Read-only runtime audit. No code, configuration, installation or repair changes.
User explicitly requires existing bottom dock functions to remain unchanged.

## Findings

Installed KWin package reports 6.7.5-1.1. The matching upstream branch's
[window handling](https://github.com/KDE/kwin/blob/Plasma/6.7/src/window.cpp)
has separate electric-border and Shift-custom-tiling paths. Side-edge tiling
includes bottom corner quarter tiles; bottom-center is not that target.
Native move-step notification precedes native zone handling; move-finished
notification follows native placement. Reacting afterward is not prevention.
Global electric-border options alone do not cover the Shift path.

Local kwinrc has no explicit electric-border overrides; this does not establish
current in-memory option values. The disabled screenedge effect is not evidence
that native window tiling is disabled. Saved custom tile layouts also exist.

Kadunce's Effect registers top/bottom touch borders when direct Z13 system-edge
ownership is unavailable. WorkspaceInputRouter preserves bottom gesture behavior
and routes shown Dock/AppletPopup hit regions to Plasma, including mixed-device
safety-control access. Preserve these paths, not merely a fixed bottom strip.

Kadunce's held-card side paging uses a 300ms dwell and 350ms repeat; stack preview
also participates. These need arbitration with new placement intent independently
of native KWin snapping. Current titlebar moves remain native interactive moves;
consumed Card Line holds take a different input path. Touch still lacks pointer's
updateCardGrabDestination call, so transfer parity is not complete.

## Recommended boundary, not yet implemented

- Kadunce owns placement for its cards/carries; KWin must not preview or commit
  competing native edge placement during that ownership.
- Leave ordinary unmanaged desktop windows native. A global policy affecting
  all windows while the plugin is loaded is broader and remains unapproved.
- Preserve bottom-center dock gestures, panel/popup input, tray kill switch,
  keyboard shortcuts and intentional manual desktop changes.
- Distinguish side/bottom-corner window placement from bottom dock interaction.
  Do not activate the generic CarryEdge::Bottom as a bottom-center snap target.
- Audit native movement magnetism separately from tiling; do not globally zero
  border/window/center snap distances to compensate for a carry implementation.
- Route browse, stack insertion and layout-edge preview through one carry intent
  decision rather than concurrent timers controlling the same held card.

## Next bounded task

Prove a pre-placement ownership boundary for native titlebar/Bento moves in the
isolated KWin harness. Determine whether supported interception can prevent both
electric-border and Shift tiling without changing persistent global settings;
otherwise document the narrow upstream arbitration hook required before coding.
Do not claim an existing per-window inhibit API has been found.

Acceptance: one preview/commit owner; ordinary desktop moves remain native;
top/side/corner/seam targets work without dock regression; mouse/touch parity;
cancel, output loss, disable and unload restore ownership. No post-snap geometry
undo loop. Installed plugin and trusted repair remain unchanged.
