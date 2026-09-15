# Kadunce visual and motion audit — September 15, 2026

## Scope and candidate

A-Team audit against `ITASCA-VISUAL-LANGUAGE.md`, including motion, from accepted
`origin/main` **75bb06a**. Branch `a/kadunce-visual-alignment`, isolated worktree
`../.worktrees/kadunce-visual-alignment`. Kadunce production files only.

The older Tette authoritative-success candidate remains separate: uncommitted
changes in `../.worktrees/tette-ambient-providers`, branch
`astra/ambient-providers`, HEAD `f78a03f`. Ten provider/model/bridge/test files were
modified at this audit's start. No completion-cue commit or passing validation is
claimed here. Nothing from that worktree participates in this candidate.

## Changes

- Asset commit **6cdaadf** changes only painted foreground in the enabled/disabled
  tray SVGs to Ghost White `#F8F8FF`. Masks retain exact black/white values; fan
  geometry, disabled slash, enabled notch and opacity levels are unchanged.
- Spatial hints become `return to desktop` and `place in slot N of M`; their text
  uses Ghost White. These are environmental hints with no input target.
- The source check's frozen CardLineLayout hashes now identify accepted **884e5c3**
  (output-local dock clearance), already present unchanged at **75bb06a**. The
  previous expected hashes predated that accepted change. Neither layout file
  is changed by this audit; model hashes and behavioral assertions remain.

## Surface inventory and decisions

| Surface / source | Finding and disposition |
|---|---|
| Persistent tray — `control/src/main.cpp`, `control/assets/*.svg` | Custom stacked-card identity is intentional. Foreground aligned; silhouette and enabled/disabled distinction preserved. Always-active StatusNotifier, click toggle, checked menu state and failure rollback remain. This is not a generic power icon. |
| Tray menu | Text actions already use sentence case. Native checked/disabled rows communicate state; disabled compatibility text is information. No raw Unicode control glyph or mixed action-icon cluster. Keep native menu spacing, focus, contrast and accessibility behavior. |
| Settings dialog | System font, form label, supporting hint, bounded spinbox and native Save/Cancel controls. Preserve QStyle spacing and standard widgets as a native-control exception to custom pills; no stylesheet that could weaken contrast/focus or the safety UI. `Active`, `Card Line`, `Bento` and `KWin` retain product/state names. |
| Repair/diagnostic dialogs | Native QMessageBox roles and standard buttons, including recovery copy and detailed diagnostics. `dialog-error`, `dialog-warning`, `dialog-information` are system feedback icons, not a suite-owned action cluster. Retain platform lookup and palette. |
| Native cards / stack / previews — `Effect.cpp` | Accepted 10 px card aperture and matching card outlines, rigid fan, proportional live content and backing remain. These are spatial content surfaces, not pill controls. Preserve app imagery, geometry, accepted neutral shader material and black-bar policy. Changing these to generic 14–18 px popup tokens would alter accepted visuals. |
| Bento / divider affordances | The visible 4 px rail has a 2 px radius; it indicates a real divider action. Larger hit region remains independent of the small glyph. Preview boxes follow the accepted 10 px spatial outline. Idle rails remain hidden; no passive status pill added. |
| Destination / insertion hints — `DesktopExitLabel.h`, `Effect.cpp` | Lowercase and Ghost White corrected. Existing 240×36 presented box, 12 px effective corner and 12 px top inset remain. It is a rounded information box, not a capsule (radius is less than half-height). Font is system family, 14 px effective at the existing 2× raster. No geometry, cache-lifetime or input change. |
| Spacing / nested continuity | Card/outline/aperture share the accepted radius; rails follow their own control silhouette. HUD inset is on the 4 px rhythm. Card gutters, dock clearance, fan offsets and contact geometry are spatial/interaction contracts, not arbitrary UI padding; retained. Native forms/menus defer to QStyle. |
| Highlight / color roles | No decorative accent or stacked hover effects added. Near-white tray state uses both shape and opacity. Compositor outlines remain neutral spatial hints, distinct from selection ownership. Provider/application colors and native widget palettes remain. Ghost White is used for the two suite-painted foreground surfaces touched here. |
| Asset inventory | Two local status SVGs plus a PNG application logo. No QML action surface or raw Unicode action-icon implementation. Zero Lucide SVGs are needed for this patch: do not vendor unused icons or decorate every text action solely to create a subset. Future custom action icons must use the pinned Lucide 1.46.0 subset and shared semantic names. |

The soft-primary role remains useful for supporting contrast on large/translucent
surfaces. This audit does not consolidate it into Ghost White or repaint native
palettes. App-specific outer silhouettes remain justified exceptions.

## Motion audit

| Motion / owner | Evidence and result |
|---|---|
| Pickup — `HeldCardGeometry.h` | 180 ms cubic ease-out; contact-anchored geometry. Appropriate direct-manipulation response; preserves accepted 44% endpoint. |
| Row paging — `RowPageMotion.h`, `CardStageController.cpp` | 220 ms cubic ease-out; selected/neighbor/wrapped faces share displacement and clock. Captures visible poses before selection changes. Input does not wait for completion. |
| Stack browse / insertion | Browse 220 ms with bounded depth accent; insertion 350 ms OutCubic; preview open/close 350 ms InQuart. The latter timing/easing is an explicit accepted-physics exception to the new general tiers. Retargets use the current blend; cancellation invalidates timers. No speculative replacement. |
| Arrival / launcher guest | Preview 280 ms, expansion and guest motion 220 ms OutCubic. Explicit browsing/activation cancels automatic arrival. Guest neighbor reversal starts from current opacity, with owner/generation validation on delayed cleanup. |
| Bento / drop settle — `Effect.cpp` | Shared 220 ms OutCubic, native placement once, paint-only interpolation. Current presented poses seed a reflow. Move/resize, minimization, output/geometry invalidation and carry interruption retire stale motion. |
| Choreography / lifetime | No decorative idle animation or repeated stagger. Repaint continues only while bounded motion needs frames; post-paint scheduling preserves endpoint delivery. Destination labels reflect validated reservations and disappear when invalid; they invent no completion. |
| Native tray/settings | No suite animation added. QStyle/desktop own standard control presentation. Five-second control refresh and three-second compatibility timeout are service/state timing, not animation. |

### Open gap: platform scaling / reduced motion

Custom compositor motion uses fixed `QElapsedTimer` durations; these paths do not
read the platform animation scale or expose a reduced-motion branch. This affects
pickup, row/stack, preview/arrival, guest-neighbor motion, Bento and drop settling.
It is a real design-language gap, **not a compliance pass**. No new uninterruptible
transition was identified in the reviewed paths; this source audit is not a claim
of exhaustive runtime interruption coverage.

Keep it a separate bounded follow-up: introduce one platform-scale policy without
changing default accepted physics, separate visual duration from the existing
90 ms rail activation and 300/350/500 ms input dwell thresholds, define immediate
final-geometry behavior at zero scale, and test interruption/cleanup at zero,
normal and changed scale. Coupled arrival/guest cleanup timers must remain coherent.
A blanket duration replacement in this visual patch would risk state timing and
exceed the no-animation-refactor boundary.

## Validation and installed state

- Production native plugin built in `/tmp/kadunce-visual-alignment-build`.
- Five focused CTests passed: row-page-motion, stack-browse-motion, carry-paint,
  card-line-layout and bento-layout.
- `bash tests/verify-control.sh` passed: control build, appstream/compatibility
  tests (2/2), offscreen startup, SVG parsing and persistent control/startup guards.
  Service remains `PartOf` and `WantedBy` `graphical-session.target`.
- `bash tests/verify-source.sh` passed after correcting the two stale accepted
  layout hashes. Original failure and baseline comparison are recorded above.
- Temporary staged native install and runtime-library resolution passed; no missing
  dependencies. `bash -n install.sh` and `git diff --check` passed.
- Compiler emitted existing missing-initializer warnings in unchanged controller
  code; no compile failures. No new compositor/runtime test was needed for text
  and foreground changes. The full package script was not rerun.

**Source candidate only.** No live install, effect toggle, logout, service restart
or push. No post-install live-control or J physical acceptance is claimed. The
existing installer performs live registration and requests a later session restart;
its command is for J's separate physical-review decision, not an audit test.

## J's visible-review checklist

After an explicitly chosen install and normal session activation:

1. Confirm the persistent tray control is present; run `tests/verify-live-control.sh`
   in the graphical session before feature testing. Check the same recognizable
   enabled/disabled artwork at the actual panel scale.
2. Inspect tray menu/settings focus, checked state and text readability; native
   styling and controls should look and behave as before.
3. During an existing desktop return and stack-placement gesture, check lowercase
   Ghost White hints, legibility and clipping at tablet/monitor scale.
4. Confirm card/fan/preview corners, Bento rail, gutters and motion retain their
   accepted appearance. No motion-scaling improvement is claimed by this candidate.

Stop point: J visual acceptance or a specific visible failure; no broader renderer
migration. Motion accessibility policy remains the next separate design/engineering
assignment if PM prioritizes the documented gap.
