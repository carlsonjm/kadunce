# KDE reuse audit — strengthen presentation without replacing window policy

Reviewed 2026-09-11. Research only: no runtime edits, installs, session changes,
new gesture timings or benchmark claims. Stability acceptance remains valid.

## Recommendation

Keep Kadunce's tested workspace identity, admission/restoration, output cutoff,
Tette contract and independent safety control. Evaluate reusable **presentation
and input primitives**, not a replacement mobile shell. The biggest architectural
opportunity is one interruptible motion state driving every card's pose, rather
than more independent timer/easing paths. That is a design recommendation, not a
proven explanation of the rejected fast-swipe build.

First compare the current renderer against a tablet-local **OffscreenQuickScene**
using Qt Quick and KWin live thumbnails in a private compositor. Include stock
QuickSceneEffect as a control/reference, not the assumed destination. Make
desktop/panel passthrough the first gate, before polishing the scene.

## Versions and evidence

Installed: KWin/plasma-workspace/libplasma 6.7.5-1.1; Qt base 6.11.2-3 and
declarative 6.11.2-1.1; Kirigami 6.30.0-1.1; KScreen/libkscreen 6.7.5-1.1.
Plasma Mobile and feedbackd were not found in the queried installed inventory.
These are local package records, not a claim that distribution patches equal
upstream sources byte for byte.

Pinned upstream sources, cloned read-only for inspection:

- [KWin 6.7.5](https://github.com/KDE/kwin/tree/ab7df7ccb7c6af20f4b279cd6220f7cd3d2267d7),
  commit `ab7df7ccb7c6af20f4b279cd6220f7cd3d2267d7`.
- [Plasma Mobile 6.7.5](https://github.com/KDE/plasma-mobile/tree/10773c81e36b322f307737391f836126d1115277),
  commit `10773c81e36b322f307737391f836126d1115277`.
- Mobile development comparison: `aa620c77226e680370d3fa1e520c78f8d1917031`.
  It is NOT the installed/recommended release.
- Local source checkouts: `/tmp/kadunce-kde-research.uvf1DJ/{kwin,mobile}`.
  Temporary evidence only; pinned links above are durable retrieval anchors.
- Local installed headers confirm QuickSceneEffect, OffscreenQuickScene,
  EffectTogglableState and the pointer/touch forwarding APIs used in this review.

## Reuse inventory

| Component | What it buys us | Boundary / recommendation |
| --- | --- | --- |
| Qt Quick DragHandler + TapHandler, in qt6-declarative | Device filters, movement thresholds, grabs/cancellation, click-versus-drag machinery | Strong prototype candidate; feed semantic commands to existing owners. Not a global KWin input filter replacement. |
| KWin OffscreenQuickScene / OffscreenQuickView | QML scene rendered to a compositor texture; explicit geometry, DPR, repaint and input forwarding | Most promising incremental presentation host. We retain responsibility for selective routing, lifetime and output-local painting. |
| KWin WindowThumbnail | Live window texture, damage/geometry updates, texture lifetime management | Compare against current live painting. Content has a documented one-frame delay; native Active stays native. |
| QuickSceneEffect | Per-output scene management, input forwarding, fullscreen lifecycle | Useful reference/test control. Global keyboard/mouse interception conflicts with our independent monitor contract unless proven otherwise. |
| EffectTogglableState / realtime touch borders | Invocation progress, opposing activation/deactivation, edge registration | Good boundary model for entering/leaving Card Line. Not a velocity-aware one-finger card pager or stack transaction engine. |
| Qt SmoothedAnimation / SpringAnimation; KWin Slide SpringMotion source | Retargetable settling and position/velocity concepts | Evaluate for release/reversal, never smooth the finger-follow path merely to make it look soft. Slide helper is source-private, not an installed public library. |
| Overview WindowHeapDelegate | Working examples of device-specific drag handlers, stable window IDs and drop handling | Adapt patterns, not the private grid component. It embeds overview layout/activation policy and private imports. |
| Plasma Mobile task switcher / taskpanel | Edge-to-overview state, velocity filtering, intent hysteresis and press feedback examples | Reference, not drop-in dependency. Phone home/close/maximize policy differs from Kadunce. |
| Kirigami / Qt platform hints | Consistent control metrics, interaction/accessibility settings and feedback vocabulary | Reuse selectively. Do not change accepted 64/54 card geometry to theme units. |
| KScreen/libkscreen | Existing display configuration ecosystem | Keep it authoritative for output configuration. It does not provide card transfer/restore semantics. No new monitor daemon in this block. |
| Mobile haptics / feedbackd | Optional tactile confirmation on supported hardware | Defer until actuator/backend support is verified. No mandatory dependency or fake haptic promises for the Z13. |

Qt's documented [DragHandler](https://doc.qt.io/qt-6/qml-qtquick-draghandler.html)
can use `target: null`, separating recognition from a visual item's movement.
Its device/grab controls are useful for an adapter, but only after our router
has decided which scene owns the event. [TapHandler](https://doc.qt.io/qt-6/qml-qtquick-taphandler.html)
provides gesture policies and long-press handling; choose those policies
explicitly rather than relying on defaults to preserve Tette's contract.

## Important findings from actual source

### 1. Plasma Mobile is not a completed flick engine we can import

The release's [FlickContainer.qml](https://github.com/KDE/plasma-mobile/blob/10773c81e36b322f307737391f836126d1115277/kwin/mobiletaskswitcher/package/contents/ui/FlickContainer.qml)
records a touch-flick problem. It uses Flickable to gather horizontal movement,
cancels flicks, and invokes a direction-based snap workaround. Its
[helper](https://github.com/KDE/plasma-mobile/blob/10773c81e36b322f307737391f836126d1115277/kwin/mobiletaskswitcher/package/contents/ui/TaskSwitcherHelpers.qml)
explicitly ignores momentum in that workaround. This is source evidence, not a
fresh reproduction on our hardware. It rules out assuming that installing the
package automatically provides the missing physical feel.

The useful part is its separation of gesture position, displayed selection and
settling. [TaskSwitcher.qml](https://github.com/KDE/plasma-mobile/blob/10773c81e36b322f307737391f836126d1115277/kwin/mobiletaskswitcher/package/contents/ui/TaskSwitcher.qml)
classifies edge gestures using direction, displacement and velocity, with
separate thresholds to reduce intent flicker. It also handles returning during
closing. Its phone-specific home/scrub/close decisions are not our card policy.

The [C++ state](https://github.com/KDE/plasma-mobile/blob/10773c81e36b322f307737391f836126d1115277/kwin/mobiletaskswitcher/plugin/mobiletaskswitchereffect.cpp)
filters velocity with an exponentially weighted average. The later
[development revision](https://github.com/KDE/plasma-mobile/commit/aa620c77226e680370d3fa1e520c78f8d1917031)
diff against the release adds explicit velocity reset, instance-owned prior
deltas, shutdown-timer cancellation, view caching and flick-position-reset guards.
The flick workaround remains. Lesson: interrupted/restarted gestures need
explicit lifecycle tests even in upstream code; do not cherry-pick an entire
development switcher as a shortcut.

### 2. Fullscreen framework ownership is the main integration risk

[QuickSceneEffect::startInternal](https://github.com/KDE/kwin/blob/ab7df7ccb7c6af20f4b279cd6220f7cd3d2267d7/src/effect/quickeffect.cpp)
grabs the keyboard, starts mouse interception, sets the active fullscreen effect
and creates/reactivates views for all screens. Mouse events go to a scene and
keep an implicit scene grab; touch motion is routed by the current position's
view. A transparent or empty external view does not establish native passthrough.
Cross-output contact ownership and tray access therefore require explicit tests.

[OffscreenQuickView](https://github.com/KDE/kwin/blob/ab7df7ccb7c6af20f4b279cd6220f7cd3d2267d7/src/effect/offscreenquickview.h)
is the lower-level alternative: texture export, explicit geometry/DPR and input
forwarding without that fullscreen lifecycle. This looks better aligned with
our selective router, but we have NOT built the combined scene/thumbnail host.
Compatibility, lifecycle cleanup and performance remain prototype questions.

### 3. Live thumbnail does not mean zero-cost or native app interaction

[WindowThumbnailSource](https://github.com/KDE/kwin/blob/ab7df7ccb7c6af20f4b279cd6220f7cd3d2267d7/src/scripting/windowthumbnailitem.cpp)
tracks damage/geometry and renders into a texture with synchronization fences.
Its source documents one frame of content latency due to the context boundary.
That does not necessarily delay the QML card's position, but can make app content
older than its moving frame. It also allocates textures based on window geometry
and scene DPR: count and scaling matter. No speed or memory win is established.

Keep Active windows native. Test thumbnail bounds, corners and source aspect
ratio with Ghostty, fullscreen and small windows. Qt Quick `clip` alone should
not be assumed to reproduce Kadunce's rounded clipping contract.

### 4. The closest desktop example separates touch from mouse

[WindowHeapDelegate](https://github.com/KDE/kwin/blob/ab7df7ccb7c6af20f4b279cd6220f7cd3d2267d7/src/plugins/private/qml/WindowHeapDelegate.qml)
uses TapHandler and distinct DragHandlers for touchscreen versus
mouse/touchpad/stylus. It preserves window IDs and handles drop/return paths.
Those are reusable design patterns. Its broad grab permissions, grid behavior
and private imports are not a safe wholesale replacement for our mixed-device
contract—especially the newly passed held-card kill-switch test.

### 5. Built-in gesture helpers have narrower jobs than ours

[EffectTogglableState](https://github.com/KDE/kwin/blob/ab7df7ccb7c6af20f4b279cd6220f7cd3d2267d7/src/effect/effecttogglablestate.cpp)
separates activation progress and completion, but uses midpoint decisions and
the border helper uses a fixed logical-distance normalization. It is not an
inertial multi-card motion model. Overview registers four-finger touchpad and
three-finger touchscreen gestures; those cannot simply replace one-finger card
swipes. Mobile's own touch-border adapter ignores the supplied output argument:
Kadunce must retain explicit tablet ownership.

### 6. Our current missing layer is observable in source

`WorkspaceInputRouter::updateTouchGesture()` triggers horizontal paging after
58 logical pixels, then marks the gesture committed. It does not represent
continuous row displacement plus release velocity. `CardStageController` has
separate preview, stack and guest transition clocks. `previewTargetForWindow()`
bypasses preview interpolation during a card grab and rounds blended geometry.
These are concrete seams to measure, not proof that any single one caused the
rejected build. A shorter hold timer would not fix those seams.

## Proposed motion contract (not implemented)

- One session owns contact identity, gesture intent, source output, current pose,
  target pose and velocity. Workspace membership remains separate.
- During direct manipulation, motion follows displacement; settling does not
  keep pulling the card toward an old target underneath the finger.
- On release, bounded velocity and displacement choose a destination. Regrab
  starts from the displayed pose, not the old slot origin. Reversal changes the
  destination without a position jump. Velocity continuity must be measured.
- Window admission/closure, pair-to-three sizing and Tette replacement derive
  all participants from the same transition snapshot; no independent restart
  that sends neighbors through the old center.
- Cancellation restores the model and clears pending intent. Safety controls,
  native external dragging and app-owned input retain their tested authority.
- Press feedback can start before a hold becomes a reorder. Paging must not
  require waiting for the reorder hold. Do not tune the existing 300 ms yet.

[SmoothedAnimation](https://doc.qt.io/qt-6/qml-qtquick-smoothedanimation.html)
offers retargeted motion with velocity-preserving splice behavior;
[SpringAnimation](https://doc.qt.io/qt-6/qml-qtquick-springanimation.html)
offers spring/damping control. They are candidates for settle, not proof of
correct gesture physics. The C++ [Slide spring helper](https://github.com/KDE/kwin/blob/ab7df7ccb7c6af20f4b279cd6220f7cd3d2267d7/src/plugins/slide/springmotion.h)
is another model to compare, but is not a public installed helper API.
[QStyleHints](https://doc.qt.io/qt-6/qstylehints.html) supplies platform drag and
hold settings; inspect and respect applicable user preferences instead of
inventing a supposedly universal tablet delay.

## Next bounded experiment and stop gates

1. Private two-output compositor; same sample windows and Kadunce layout model.
   No production install or system-shell replacement. Include a panel/applet
   popup input surface and a native external client, not only fake targets.
2. Compare existing painter with OffscreenQuickScene + live thumbnails. Stock
   QuickSceneEffect is a reference showing what fullscreen ownership changes.
3. First gate: tablet hard cutoff, external mouse/touch/drag delivery, tray
   Disable during held touch, release drain, cancellation and output removal.
   If this fails, stop that host path—do not weaken the safety exceptions.
4. Only after that: identical scripted slow drag, quick flick, reversal and
   settle/regrab traces; paired 64/54, three-card and Tette-shaped guest layouts.
   Log bounded timestamp/pose samples, frame intervals, missed deadlines,
   texture allocations and idle repaint behavior. Check 60 Hz and high-refresh
   hardware later; virtual testing does not establish physical touch latency.
5. Choose renderer only after visual continuity and cost evidence. Keep the
   current renderer if scene reuse does not justify its integration costs.
   Do not add another mutable window registry or a new daemon to hide the issue.

## Maintenance and scope

Use supported Qt APIs first. KWin native integration remains version-sensitive;
QML alone does not remove that dependency. Adapted upstream files require pinned
provenance and their SPDX notices (reviewed files include GPL/LGPL variants).
MobileShell explicitly labels its API private/unstable; keep it out of runtime
dependencies. Its convergentwindows script changes maximize/decorations policy,
so enabling it would create a second authority over our restore geometry.

No new package installation is recommended now. No claim is made that KScreen,
Kirigami or a mobile-shell package supplies monitor handoff transactions. No
Android/iPad audit was repeated here. Monitor hardware validation and general
feature work remain parked; the next work is the isolated comparison above.
