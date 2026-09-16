# A1 — ownership records at committed Card Line entry

Originally source-only candidate **0565ccf** on `a/ownership-a1`, fresh worktree
`../.worktrees/kadunce-ownership-a1`, based on **b971c0a**. This packet covers only
initial Card Line ownership and committed cross-display tablet release.

## Root cause

`rebuildLiveCards()` committed all discovered members to `m_workspace`, but
`m_parkedRestores` stayed empty. `enterActive()` created records lazily for the
selected member. Thus membership and the retained ownership record used by
`managedRestore()` disagreed until individual visits. The latter is also the
ownership predicate for native resize rejection and authoritative Bento handoff.

`admitTransferredWindowToTablet()` retained the incoming source restore before
`sendToOutput(tablet)`, then applied the existing tablet Active target. A later
release replayed the stored monitor coordinates. This mixed the precommit source
rollback with the newly committed receiver's ownership origin.

PM explicitly confirmed the phase boundary: cancellation before adoption preserves
the source; committed tablet adoption releases on the tablet. The old tablet-entry
fixture's monitor-on-unload comment is superseded by that current expectation.
Admission eligibility and destination selection policy are unchanged.

## Correction

- A shared `retainManagedOwnership()` captures an unrecorded member once, without
  selecting, activating, resizing, maximizing or otherwise mutating its window.
  Initial reset and local new-member admission call it at the membership boundary.
  Active entry reuses the retained record; visits never replace its origin.
- Native geometry is read from `moveResizeGeometry()`, KWin's accepted placement,
  rather than a potentially older Wayland frame awaiting client acknowledgement.
- On cross-output admission, after source/destination commitment and the existing
  `sendToOutput(tablet)`, retain the accepted receiver placement **before** the
  existing Active-size write. KWin supplies the tablet ordinary geometry and native
  restore fields; no new coordinate mapping or landing policy is introduced.
- Same-output admission can still consume its supplied authoritative record.
  Rejected source commitment never moves the window or establishes a receiver
  record. Existing retained members are not overwritten.

Production changes are confined to CardStageController.cpp/.h. There is no renderer,
input timing, dock-clearance, card/stack/Bento geometry, label, visual asset,
Table, Tette or Temperance change. The persistent safety control is untouched.

## Evidence

- Production build passed: `/tmp/kadunce-a1-build`.
- Eight focused CTests passed: workspace-state, workspace-snapshot, carry-session,
  carry-input-route, bento-transfer, carry-paint, card-line-layout, bento-layout.
  Added initial complete-membership/rejected-adoption model cases.
- New source guards require membership before retention, prohibit native mutation
  in shared initial ownership capture, and require committed tablet placement
  before receiver retention and Active sizing. `verify-source.sh` passed.
- `verify-control.sh` passed: control build, 2/2 tests, offscreen startup, SVG and
  mandatory persistent-switch/graphical-session.target guards.
- New `ownership-session.sh` runs actual CardStageController instances with real
  clients on two private virtual KWin outputs. It proves initial origins for both
  selected and unselected members before any Active visit, no initial geometry
  write, origin preservation after visits, and exact final release geometry.
  It also proves rejection leaves monitor state/membership intact and successful
  adoption retains a tablet rectangle of the original ordinary size, which is
  restored after release.
- Private runtime passed, exit 0. Evidence:
  `/tmp/kadunce-unload-test.LTnhNy/session-retry.log`. The initial sandbox attempt
  could not bind its private D-Bus socket; the approved retry reused the same built
  probe and isolated config/data/runtime, not the real session bus. Expected
  private-session portal/PipeWire warnings did not prevent either assertion.
- Shell syntax and diff checks pass. Existing compiler missing-initializer warnings
  are in unchanged controller code.

The runtime fixture tests the authoritative production controller/release methods,
not a physical monitor-to-tablet gesture or Plasma dock painting. J's floating-dock
symptom remains a required physical check. No live install, effect toggle, logout,
reboot, push or physical acceptance is claimed. Post-install live safety remains
required in J's graphical session.

## Install, rollback and bounded physical check

Use this worktree's `install.sh` only when J chooses the candidate. It builds/checks
source, installs/registers the effect and asks for a later normal session restart;
A has not run it. Keep the September 15 accepted **b971c0a** source as rollback;
reinstall that exact accepted baseline if the candidate fails, using the normal
installer/session procedure. No branch reset or history rewriting is required.

1. Arrange ordinary windows on monitor. Adopt one onto tablet; after ownership
   commits, Escape must release it on tablet at ordinary size. Cancel before
   adoption and confirm source monitor restoration remains intact.
2. With multiple previously unowned tablet windows, enter Card Line once. Without
   visiting each, verify all behave as owned cards and the floating dock remains
   correct. Escape must restore each original ordinary window geometry.
3. Visit one card as Active, return to Card Line, then release; original ownership
   records must survive. Check normal monitor admission/Bento handoff, dock clearance
   and the persistent tray control. Run `tests/verify-live-control.sh` after install.

Stop at a concrete failed case or J acceptance. No animation scaling, stack
extraction, reorder-zone or other roadmap work belongs to A1.

## Physical acceptance and publication — September 16

J passed tests 1–3, including persistent control, committed tablet release and
immediate initial ownership. PM authorized publication. The unchanged candidate
was replayed as **02e6a6f** on **8d3a0b9**, preserving PM's newer roadmap/checklist.
Publication worktree: `../.worktrees/kadunce-ownership-a1-release`.
The earlier pending/source-only statements describe the original handoff.
A2 is a separate bounded ownership candidate and is not authorized for publication.
