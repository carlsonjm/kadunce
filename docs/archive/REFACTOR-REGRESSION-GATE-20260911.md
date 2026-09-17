# Block 1: accepted-behavior regression gate

Established 2026-09-11 against the recovered baseline in ROLLBACK-PROVENANCE.md.
This is a behavior contract, not a promise that every behavior is automated.
Tests may change implementation shape during refactoring; preserve their product
assertions rather than requiring particular private names or call structures.

## Coverage map

| Contract | Existing automated evidence | Still requires live verification |
| --- | --- | --- |
| Active gutter 6–48, default 10; output-relative | card-line-layout: all supported gutters and offset origins; window-handling: config notification/clamp | Actual app geometry, minimum-size constraints |
| Release restores desktop state, including fullscreen | window-handling: restore sequence, fullscreen/maximize/tile and lost-output fallback | Real fullscreen repaint, focus and original desktop geometry |
| Two groups: 64% center/54% shoulder; three+: 54% | card-line-layout: dimensions, centering, envelopes; card-line-model: directional neighbor and third arrival/removal | Animated continuity and visual corner clipping |
| Stacks retain member order and selected face | card-line-model: paging, detach/cancel, insertion, large stacks | Real-window identity across presentation lifecycle is NOT yet guaranteed |
| Passive tablet neighbors never paint externally | window-handling: complete paint-route truth table; source guards: renderer uses tablet fence | Actual multi-output clipping, fractional scale, carried-window visibility |
| Native moves and Plasma panel inputs retain ownership | panel-input: native passthrough, foreign release, panel mouse/touch sequences | KWin transaction ordering and physical touchscreen routing |
| Tette outside tap dismisses; movement is not a tap | panel-input: tap, cancel, out-and-back drag, next-click recovery, inside-launcher passthrough | Tette actually closes, swipe collapses cards and stays in Card Line |
| New app replaces Tette center before Active | card-line-model: centered arrival and shoulder order; launch-identity: matching policy | Splash→main-window lifecycle, focus restoration, stale guest completion |
| Kill switch survives independently; repair preserves baseline | verify-control, verify-live-control, verify-repair (isolated snapshot only) | Actual toggle/re-enable and incompatible-plugin recovery require explicit permission |

Unit tests of policy/functions do not execute the live compositor. Source grep
guards are weaker structural evidence, not substitutes for integration tests.
No timer values, current swipe roughness or full-stage release bug are newly
declared acceptance requirements by this gate.

## Short live checklist for future candidates

Record candidate/source identity, session, display setup and PASS/FAIL/NOT RUN.
Use disposable windows; save work before fullscreen or recovery checks.

1. **Active and release:** start with a small ordinary window, enter Active,
   check four gutters, enter/leave Card Line, then Ctrl+Esc. Check restored size,
   focus and interaction. Repeat with maximize and true fullscreen; no black view.
2. **Layouts:** with two groups, page each way; add a third and close it. Check
   64%/54% roles, no duplicated shoulder, stable neighbor side. Cycle a stack,
   lift/cancel a member, and confirm order and selected face.
3. **Tette:** outside tap closes; drag outside and back does not tap. Swipe Tette
   away: remaining cards collapse and Card Line stays open. Open Tette again,
   launch an app: it replaces the guest center, then becomes Active.
4. **Input boundaries:** tap panel controls and an Active app; neither selects a
   background card. A deliberate system-edge gesture still enters Card Line.
5. **Docked only — parked:** drag an ordinary monitor window onto tablet, cancel
   with Escape, then drop; carry back. Passive neighbors never leak. Test with
   Tette open. Record Active-drag whole-stage release as a known failure, not a
   new regression or a passed transfer. Repeat differing scale/output origins.
6. **Safety:** verify tray registration and startup wiring read-only. With explicit
   permission, disable/re-enable and verify desktop recovery. Never exercise real
   repair simply to run regression checks; use its disposable canceled-install test.

## Block boundary

September 11 result: all seven native tests pass with the additional gutter and
input cases; source guards, control package checks and live tray check pass.
The isolated repair test passes build/tests, canceled authorization and corrupt
snapshot rejection. Installed-plugin and trusted-repair archive hashes are
unchanged. Runtime `native/src` matches the recovered baseline. No fresh live
gesture/display checklist was run, and monitor checks remain parked.

Block 1 is complete when the coverage map and checklist exist, practical added
unit cases pass, and gaps/known failures are named. It does not require testing
an unchanged desktop again or fixing monitor behavior from the couch.

Block 2 is workspace identity/state separation **without visual changes**.
Its local candidate now adds workspace-snapshot and workspace-state tests (nine
native tests total). State tests compare commands against the unchanged baseline
model and check membership removal, identity after index shifts, duplicate
admission rejection, cancellation, and session reset. Live acceptance remains
separate; the installed baseline and repair archive have not been replaced.
Renderer research, touch timing, Bento features and new Tette capabilities are
parked until their block. A regression blocks progress; a new idea goes into the
parking list unless required for the current block's contract.
