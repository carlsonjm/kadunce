# Remaining roadmap — September 13, 2026

This is the active queue, replacing historical P0–P3/K0–K6 “next” directives.
CURRENT_STATE.md records installed/source provenance. J owns acceptance; physical
failures override earlier passes. Planning is not permission to install or expand scope.

## Accepted baseline — preserve, do not rebuild

- Native touch carry, Card Line/Bento operation, edge entry, dock-safe exit and
  output handoff have prior acceptance. Coverage is now reopened narrowly by R1.
- Card Line shared paging, hidden-neighbor preparation, directional stack browsing,
  ordered insertion, held-front ordering, balanced neighboring decks, and coordinated
  pickup/release have passed. Keep 44% hold size and immediate finger tracking.
- Tette Meta toggle/fullscreen panel access: bdf4523.
- Temperance notification readability/banner and Bluetooth pairing polish: ff62ce3.
  Kadunce Active elevation cleanup passed with the earlier coordinated QoL batch.
- These passes are regression contracts, not proof of every app/device sequence.
  Zen sampling remains unresolved; no new cache experiments are authorized.

## R1 — Bento/card ownership and drop targets (next engineering block)

### Current report
J must force cards into Card Line individually or they revert to ordinary windows
or return to the monitor. Bento drop regions also need work. Exact source state,
gesture and destination for each failure are not yet established. Earlier
cross-display passes are real but do not establish coverage of this sequence.
Do not label it a confirmed code regression or a new architecture requirement yet.

### Product contract
- Eligible windows on an activated card workspace should not require individual
  manual promotion to obtain the intended workspace behavior.
- Source ownership persists while a drag is in flight; crossing a display alone
  must not commit membership or overwrite the source restore record.
- Existing destination Bento receives the card through its admission plan.
- Without destination Bento, monitor open-space release becomes an ordinary
  window; intentional edge placement activates Bento for eligible local windows.
- Tablet Card Line/Active/Bento must agree about admission and departure. A valid
  accepted drop must not spring back; rejection must restore the original state.
- Preview and committed destination must agree. Preserve bottom dock gestures,
  safe landing, tablet output fences, Ctrl+Esc and instant disable.

### R1a: bounded diagnosis before editing
Capture one failing route and a matched passing route on current main. Record
source presentation, native/card membership, initiating input, destination display,
existing destination session, preview intent, and result at release. Include
ordinary windows that have never entered Card Line and cards explicitly promoted.
Compare the relevant changes against the prior accepted edge/ownership freeze;
do not roll back animation or assume all earlier fixes are absent.
Inspect CardStageController, DesktopStageController, NativeCarryRuntime and the
existing admission/reservation boundaries only as evidence requires.
Deliver: exact reproduction, first divergent owner/decision, smallest correction.

### R1b: ownership correction
Fix only the proven boundary, using existing authoritative membership and restore
records. No app-name exceptions, per-frame geometry enforcement, new persistent
service or blanket re-admission. Focused regression test plus J's failing route
and one neighboring passing route; stop after acceptance.

### R1c: Bento drop-region correction
J identifies the unintuitive/missed drop region; compare visible preview with the
actual hit test and release decision. Fix shared target geometry/intent rather
than separate preview and commit heuristics. Cover tablet-only and monitor,
both approach directions, existing/empty Bento and pullback cancellation as
relevant. Separate this change from R1b if it changes interaction policy.

Stop rule: two unsuccessful candidates require an evidence review, not another
speculative layer. No broad KWin rebuild or test campaign without scope approval.

## R2 — Remaining motion (after R1 acceptance)

Only demonstrated gaps: Bento displaced-neighbor/reflow motion and any cross-display
arrival discontinuity. Pickup/drop and line/stack cycling are no longer pending.
No replacement animation engine or blanket duration changes.

## R3 — Known compatibility and release confidence

- Zen rendering/sampling artifact: separate bounded rendering investigation;
  never trade accepted touch responsiveness for speculative cache machinery.
- Affinity splash/main-window transition: deferred, nonblocking compatibility.
- Short physical checkpoint on tablet-only and docked configurations after R1;
  preserve safety switch, restore behavior and dock gestures. J owns physical
  acceptance; automated coverage does not substitute for it.
- Distribution bundle, broader hardware coverage and update/repair integration
  are later packaging decisions, not new mandatory architecture.

## Separate app feature queue

1. Tette Active-sized Browse Everything/All Files: begin with backend feasibility
   and guest-sizing/job-lifetime decisions, not full implementation. Preserve
   search scope/ranking and touch-friendly minimal UI.
2. Tette desktop/Bento use: verify standalone display/focus/launch behavior before
   selecting any guest-contract expansion.
3. Steam/external libraries: scoped discovery and launch work; Steam owns library
   and Proton resolution. Missing drives must fail safely.
4. Temperance truthful power-state audit, stock-notification handback and any
   remaining badge details: recheck current app source and acceptance before
   treating historical inventory entries as open bugs. Bolt is not proof of bypass.

## Deferred, not MVP blockers

Persistent layouts across unload, native-to-stack producers beyond existing
paths, renderer extraction, new services and additional tiling modes.

## Design archive

ROADMAP-HISTORY-20260913.md preserves the entire former roadmap, including existing
E13–E15 additions and the detailed E12 All Files F0–F5 plan. It is design/history,
not execution order. Completed ecosystem fixes must not be reintroduced as tasks.
MVP-RELEASE-SCOPE.md remains the scope/resource policy; this queue reflects the
new user-authorized priorities. Next assignment is R1a, not all blocks at once.
