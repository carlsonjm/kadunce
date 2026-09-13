# Refactor plan and work lanes

Current assignments and execution order: ENGINEERING-BLOCKS.md. The plan below
is historical refactor evidence; do not restart completed extraction work.

Prepared 2026-09-11. Design/review plan; no runtime migration performed.

## Current execution order

September 11: stability is user-accepted and KDE research complete.
[Unified card blocks](UNIFIED-CARD-BLOCKS.md) govern the next phase: ownership →
transactions → free-carry presentation → edge arbitration → ordered stacks →
unified display/Bento journey → motion polish. Earlier ordering below is
historical scaffolding. Bento integration is now in scope as an arrangement.
Installation and publishing remain separately authorized.

The agreed work blocks now refine the staged plan below: (1) regression gate,
(2) workspace-state boundary without visual change, (3) interaction ownership,
(4) isolated renderer comparison, (5) continuous motion, (6) touch polish.
Start with [REFACTOR-REGRESSION-GATE.md](REFACTOR-REGRESSION-GATE.md); keep new
features parked rather than broadening the active block. Earlier research ideas
below are not permission to skip the behavioral/state gates.

Explicit prerequisite reaffirmed September 11: unpack the existing Plasma Mobile
research and related discussion before implementing touch/motion changes. Map
what to reuse versus what still needs version-matched evidence; do not jump from
input cleanup to another timing experiment. Candidate validation is separate from
that research, and neither authorizes installation or a repair refresh.

## Current source map

| Responsibility | Current owner | Change to investigate |
| --- | --- | --- |
| Native plugin registration | native/src/plugin.cpp, native/CMakeLists.txt | Already correct effect category |
| Window discovery, UUID export, public commands | native/src/Effect.cpp: workspaceContext, activateApplicationWindow | Workspace facade with versioned adapter |
| Stacks, selection | CardLineModel plus CardStageController's positional live list | UUID-backed domain identity independent of view lifetime |
| Active, restoration, arrival/guest timers | CardStageController | Window-policy adapter separate from render progress |
| Painting, shader, clipping, fan | Effect's paintWindow/prePaintWindow | Read-only renderer, compare QuickSceneEffect |
| Recognition, ownership, dwell | WorkspaceInputRouter | Device-specific adapters driving a coherent gesture session |
| External layouts and restoration | DesktopStageController | Keep KWin mutation ownership explicit; reconcile transfer transactions |
| Safety, settings, compatibility repair | control/ | Keep available independently of compositor presentation |
| Tette bridge client | ../tettegouche/src/main.cpp | Preserve contract while changing backend ownership |

## Staged implementation

1. Establish provenance and regression fixtures. Record source revision/diff,
   binary hash, acceptance and repair source as separate fields. Preserve monitor
   work. Use representative windows, pair/three-card layouts, panel/Tette inputs,
   fullscreen release and cross-output cutoff as existing contracts.
2. Define a workspace snapshot and semantic action facade. First extract within
   the existing plugin without visual change. Renderer cannot mutate domain state;
   KWin adapter alone applies native changes with restore ownership. A separate
   service is a later lifetime decision, not necessary for this first boundary.
3. Compare two minimal presentations with the same model: current renderer and
   a KDE scene effect using live thumbnails. Only invocation, one horizontal
   interaction, selection and release. Pin the upstream version; run in a nested
   compositor with isolated config before user-session testing. Validate external
   output passthrough; fullscreen QuickSceneEffect ownership may conflict with
   an independent external desktop and must be proven, not assumed.
4. Evaluate motion: immediate drag, fast flick, direction reversal, touching again
   during settle, cancel, window close and output loss. Record input timestamps,
   presented pose, progress and frame intervals. No full repaint/log flood per
   event. Choose based on observed continuity and rendering cost.
5. Migrate incremental owners behind the facade. Preserve APIs and restore behavior.
   Define identity survival across release first; effect-unload/restart persistence
   requires a separate design for lifetime, reconciliation and stale windows.
6. After presentation choice, prototype native partial snap → companion chooser
   → committed layout. Cover minimum-size rejection, Escape, output removal,
   mouse and touch. Full maximize bypasses chooser. Return to monitor validation.

## Heavy Lifting lane

Own facade/identity design, renderer selection, gesture ownership, restore and
handoff semantics, service-lifetime decisions, API migration and live acceptance.
Do not substitute another timing patch for evidence about the rejected gesture.

## Bounded lighter-work packets

Assign one packet explicitly. A handoff names baseline/diff, exact files, expected
result, verification and stop condition. Neither packet authorizes installation.

- **KDE source inventory:** update KDE_RESEARCH.md only. Locate version-matched
  Mobile switcher/Overview handlers, list progress/cancel/ownership behavior with
  source references. Stop if APIs cannot be verified; report uncertainty.
- **Documentation reconciliation:** update named dated records against accepted
  fixtures/source. Correct stale guest sizes/protocol descriptions. Do not retime
  or edit runtime behavior to make documentation true.
- **Regression fixture inventory:** list missing scenarios against existing native
  tests. Add tests only after Heavy Lifting specifies the behavioral contract;
  avoid assertions that merely reproduce an implementation.

At handoff completion, report changed files, checks and unresolved points, then
update CURRENT_STATE. Concurrent tasks must use disjoint ownership or isolated
checkouts. A model change alone does not authorize a different scope.

## Product constraints

Preserve mandatory safety control, tablet-only Card Line clipping, native desktop
drag/resize, current pair sizing, stacked identity, Tette standalone recovery,
and the distinction between Tette outside tap and swipe dismissal. Always-on
tablet design is a future policy; this refactor must not erase the release path.
