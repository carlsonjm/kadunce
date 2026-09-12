# KDE presentation research

## Current review

The version-pinned [reuse audit](KDE-REUSE-AUDIT.md) supersedes the preliminary
master-only inventory below. Mobile task switcher/taskpanel, KWin 6.7.5 scene
hosts, thumbnails, Overview handlers and transition helpers are now inspected.
Recommendation: compare tablet-local OffscreenQuickScene with the existing
renderer before committing to a fullscreen QuickSceneEffect migration. No
prototype or performance measurement was performed in this research turn.

## Preliminary inventory (historical)

Reviewed 2026-09-11. This is an evidence index, not a claim that a replacement
renderer has been built or that upstream code solves Kadunce's touch problems.

## Sources inspected

- [KDE effect documentation](https://develop.kde.org/docs/plasma/kwineffect/):
  describes scene delegates per output and live WindowThumbnail/WindowModel
  presentation. It is one effect implementation route, not a prerequisite that
  makes an existing C++ effect legitimate.
- [OverviewEffect declaration](https://raw.githubusercontent.com/KDE/kwin/master/src/plugins/overview/overvieweffect.h):
  Overview derives from QuickSceneEffect and exposes transition factors and
  gesture-in-progress state. This is the nearest desktop comparison for a small
  live-window presentation experiment.
- [EffectTogglableState implementation](https://raw.githubusercontent.com/KDE/kwin/master/src/effect/effecttogglablestate.cpp):
  separates activation/deactivation progress and completion decisions, including
  realtime touch-border and touchscreen-swipe integration. Investigate how this
  state model handles interrupted transitions before adapting it.
- [Plasma Mobile README](https://raw.githubusercontent.com/KDE/plasma-mobile/master/README.md):
  identifies shell components and task-panel areas, labels its MobileShell API
  private/unstable, and documents nested Wayland testing. Reuse needs a clear
  compatibility boundary; do not depend on private shell components casually.
- [Mobile KWin directory](https://github.com/KDE/plasma-mobile/tree/master/kwin)
  and [containments](https://github.com/KDE/plasma-mobile/tree/master/containments):
  located mobiletaskswitcher and taskpanel candidates. Their actual gesture and
  rendering implementations have **not yet been audited**.

The upstream source above is master, not pinned to this machine's installed
KWin. Local `/usr/include/kwin/effect/quickeffect.h` confirms QuickSceneEffect is
available. Local effecthandler/globalshortcuts/input headers distinguish touchpad
and touchscreen registration. API availability alone proves neither compatibility
nor suitable per-output input ownership.

## Findings versus hypotheses

Verified: Kadunce already derives from OffscreenEffect and transforms live KWin
windows. It has its own input recognition and transition machinery. The existing
separation into controllers does not make workspace identity independent of
presentation lifetime.

Hypothesis to test: a shared continuous gesture/transition state, potentially
using a QuickSceneEffect presentation, could reduce discontinuities. It is not
yet established that the renderer is the cause of rough fast swipes. A new
service or class split alone will not make animation smooth.

Preserve the external desktop: a scene's full-screen input ownership must not
swallow pointer/touch interactions on another output. Preserve tablet clipping
without hiding a native window during a cross-output drag.

## Next bounded investigation

1. Pin Overview and Mobile sources to compatible releases; record revision and
   exact handlers for start, progress, reversal, commit and cancel.
2. Distinguish one-finger card manipulation, touchscreen edge gestures, multi-
   finger system gestures and touchpad gestures. Do not transplant thresholds
   across these input types.
3. Run a minimal scene in an isolated nested compositor. No shell replacement or
   compositor restart in the user's session. Nested execution has not been done
   in this review.
4. Compare the same live windows and layout with the current renderer. Measure
   input-to-pose continuity, interrupted settle, frame intervals and output
   passthrough. Record observations before choosing the rendering architecture.

See [REFACTOR-PLAN.md](REFACTOR-PLAN.md) for scope and safety gates. The Android/
iPad comparison from earlier work remains design context; this review does not
claim a new audit of those platforms or an evidence-backed universal hold time.
