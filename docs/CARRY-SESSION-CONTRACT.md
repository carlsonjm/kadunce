# U1 — Headless carry contract

September 11, 2026. Implemented in native/src/CarrySession.h; exercised by
native/tests/CarrySessionTest.cpp. Not included in production runtime yet.

One initiating device/contact owns movement and release. Position preserves a
global grab anchor in both axes, without clamping or magnetic displacement.
Preview replaces only destination intent, never membership. Moving invalidates
the previous preview so the resolver must evaluate the new pose.

Destinations identify a Card Line gap, stack gap or layout slot by output,
stable target identity, revision and position. Geometry/range validation belongs
to the destination adapter. Origin retains card/output identity, membership
revision and an opaque source-owned restoration token, not a duplicate model.

Release freezes the proposed destination while awaiting acceptance. Session and
preview tickets reject stale replies and duplicate release. Tickets are scoped
to one CarrySession instance; adapters must route replies to that same instance.
All calls belong to one owning thread.

Outcomes are consumed once:

- ReadyToCommit: acceptance and revisions match; adapter may attempt commit.
- ReturnToOrigin: absent/rejected/stale destination or cancellation.
- NeedsRecovery: source changed or its output disappeared; do not blindly restore
  stale geometry or membership.
- SourceGone: carried window closed; do not resurrect it.

These are decisions, not executed transactions. U2 must provide source rollback
records, stable target revisions/reservations and final revalidation immediately
before mutation. Source removal must not precede destination acceptance. A target
can disappear after the outcome is consumed; the adapter still owns recovery.
Input-router release draining remains separate from this contract.

Tests cover both devices, foreign devices/contacts, free 2D anchor following,
invalid input, all three destination kinds, replacement and stale previews,
duplicate/late release and acceptance, source/target revision changes, cancellation,
window closure and output removal before/after release. They do not prove actual
model rollback, native geometry acceptance, rendering or physical gestures.

## U2 first slice — source preparation

CardWorkspaceState now prepares a removal using a private candidate model and
membership list. The live stack, detached-member restoration record, selection
and membership stay unchanged. Rejection discards the candidate. Commit requires
the same living state owner and unchanged revision; successful commit invalidates
all copies. Every semantic state command invalidates previous preparations,
including a change that returns to the same visible state. State owners cannot
be copied. A weak lifetime identity rejects tickets from destroyed owners.

This candidate is a short-lived transaction value, not a second live registry.
It must only be committed after a destination reservation is valid. CarrySession
outcome routing, reservation release, cancellation draining and final native
placement are not wired yet. Existing handoff methods still use their old path.
The source API does not independently prove destination acceptance.

Workspace tests exercise real stacked/detached state, rejection without edits,
original-stack restoration, successful/duplicate/foreign/expired commits,
all semantic-command invalidations and last-card removal. Next: make tablet
admission acknowledge success and Bento reserve a visible slot (not overflow),
then join source and destination under one cancellation/commit boundary.

## U2 destination feasibility slice

Bento transfer admission now solves on a private Session copy and requires the
arrival in the visible assignment. It searches smaller arrangements if necessary;
an impossible arrival is rejected without modifying the destination session.
Ordinary reflow retains its existing optional-overflow policy. Existing occupants
may still enter overflow on successful reflow; this is not a no-displacement rule.
An already-recorded overflow window is not reported as visibly accepted.

Tablet admission returns bool through its controller/host boundary: invalid or
ineligible clients are rejected, and active-stage success requires membership and
the tablet output. With the stage inactive, success means ordinary native output
placement, not creation of a card workspace. This acknowledgement does not undo
native changes on failure. Existing callers do not yet transact on that result.

Ten tests, source/control checks and read-only live safety pass. Added solver
coverage: impossible arrival, smaller feasible layout, non-leading required
candidate, invalid target/index/dimensions and ordinary-layout regression cases.
Controller lifetime/native acceptance still needs isolated compositor testing.
No installation or repair promotion. This is synchronous preparation, NOT a held
destination reservation. Next: coordinate source/destination commit and recovery,
including lifecycle reentrancy and rejected native geometry; legacy remove-first
handoff methods still need replacement before claiming transactional transfers.

## U2 first connected path — managed Bento to managed Bento

DesktopStageController now uses prepareBentoSessionTransfer for this path.
The shared helper copies both sessions, requires visible destination admission,
then removes/reflows the source candidate. Missing/duplicate identity, invalid
snapshot, same output and either planner failure reject without touching live
sessions. Last-card departure produces an empty source session for removal.
Planning is synchronous: no callbacks, event pumping or native mutation are
permitted. These values are not tickets that may be retained across events.

The controller publishes both models before applying native geometry. A rejected
titlebar drop reapplies the original source layout, because KWin has already
moved the client by the time it reports move completion. No blanket native
snapping or tablet-neighbor clipping change is made.

Eleven tests and source/control/live-safety checks pass; the new test exercises
the production helper with value handles and the actual Bento solver. It checks
rejection, overflow, source-plan failure, preserved originals, duplicate transfer
and final-member departure. Installed plugin and repair hashes are unchanged.

NOT complete: Card Line uses its legacy removal path; tablet and unmanaged
desktop handoff remain legacy. No CarrySession routing yet. Native geometry
acknowledgement, output/window loss or Disable reentrancy during applySession
need isolated compositor tests and recovery work. The borrowed-session risk
identified here is addressed by the following guard slice. Do not install this as the completed
transaction redesign. Next: harden native application/lifetime recovery, then
connect remaining adapters and repeat the held-card safety gate.

## U2 native application guards

applySession copies its plan before native calls, uses weak output/window/client
handles and checks a generation between placement setters. Session restoration,
removal, reflow, admission, stopping work and manual-move entry invalidate it.
Cleanup re-fetches by key and only clears the matching application token, so an
older pass cannot clear a newer pass's applying flag. The settle loop iterates
copied keys and uses the same guarded application in geometry-only mode; its
90 ms/four-attempt policy and correction-based early exit are preserved.

Interrupted application stops callers from applying the next transfer output or
scheduling old settling work. No session reference is accessed after a native
call in applySession; activation logging no longer reads an expired reference.
RestoreAll stops/invalidate settling before restoration. Restore itself remains
separate and still needs its own lifecycle audit; this is not complete physical
transfer rollback or acknowledgement of asynchronous configure acceptance.

WindowHandlingTest injects invalidation after each of six native placement
setters using the production sequence helper, verifies no subsequent setter and
successful fresh-generation retry. All eleven tests, source/control checks and
read-only live safety pass. Installed/repair unchanged. Next: isolated compositor
interruption/restore validation and configure-failure recovery, then remaining
Card Line/tablet adapters. No installation or publication.

## U2 restore guard follow-up

Bento restoration holds an admission barrier across the restore pass. Weak
client/window/output predicates are checked between restore setters; loss or
manual takeover stops further mutations for that snapshot. Restoration order
and the Active wrapper are unchanged. Nested callbacks cannot recreate a Bento
session that the old restore then overwrites. A real KWin reentry attempt and
fresh admission afterward pass, alongside nine injected restore interruptions
and the two-output full-candidate regression. See NATIVE-INTERRUPTION-VALIDATION.md.

This is safe interruption, not complete recovery: output loss can leave an
unfinished restoration with no retained retry; asynchronous configure refusal
and on-stack controller destruction remain unproven. Remaining adapter wiring
is still pending. Installed plugin and trusted repair are unchanged.

## U2 bounded geometry fallback

The initial retry policy below is superseded by reconcile-once: native placement
once per operation, one delayed read-only observation at 450 ms, then either keep
the accepted layout or restore the mismatched output session once. The retry
counter and geometry-only application path have been removed. Isolated native
contention now additionally asserts no observed geometry reassertions before
fallback. See NATIVE-INTERRUPTION-VALIDATION.md for current evidence and limits.

After exhausting four existing settle corrections, a final read-only check gets
one interval for the last configure to arrive. Persistent mismatch restores that
output's saved desktop session rather than leaving an unfulfilled Bento layout.
This does not reverse a committed transfer to its previous output or invent new
stack ownership. Real-KWin injected contention, normal settling, fresh activation
and two-output regression pass; see NATIVE-INTERRUPTION-VALIDATION.md. Output-loss
retry during restoration and actual asynchronous refusal evidence remain open.

## U2 topology recovery follow-up

Mid-restore output loss now carries the original snapshot through a finite
deduplicated list of surviving candidates; each candidate is attempted once.
Only topology loss retargets, not client closure or native interaction. The
removal notification takes precedence over a stale KWin output list. Fallback
geometry is clamped and old-output restore coordinates are not preserved there.
An injected native callback/removal test passes; physical unplug remains parked.
No candidate means native KWin placement and a warning, not a background retry.
See NATIVE-INTERRUPTION-VALIDATION.md. Remaining work is adapter integration and
real hotplug/delayed-client validation; source is still not install-ready.

## U2 Bento-to-native-desktop adapter

prepareBentoDeparture now serves both managed-to-managed preparation and ordinary
desktop departures. It validates unique source ownership, prepares removal and
source reflow without mutating the live session, and handles last-window depletion.
The ordinary-desktop runtime path validates its live output/geometry before
publication and checks weak handles/generation between native placement setters.
Source reflow follows native placement; motion and tablet admission remain unchanged.

Tests cover rejected/missing/ambiguous/last-window departures. The full-candidate
two-output test now waits 700 ms after native handoff before checking that the
remaining source pane stays managed and the destination remains unmanaged.
Evidence: `/tmp/kadunce-bento-candidate.8l76pt/session.log` and
`/tmp/kadunce-unload-test.XaDG01/session.log` (interruption/recovery regressions).
Eleven native tests and source/control/live safety pass; installed/repair unchanged.
This is prepared logical departure plus guarded placement, not physical atomicity:
interruption after publication does not roll back to the source. No install/push.
Next: tablet/Card Line adapter preparation and commit coordination.

## U2 Card Line→monitor commit coordination

finishCardGrabOnOutput now uses CardWorkspaceState::prepareRemoval/commitRemoval.
DesktopStageController::transferCardWindow solves existing Bento admission before
invoking the synchronous source-model commit. Destination publication precedes
source visual cleanup and native placement. Failure cancels the grab without
source removal or native fallback; no Bento means ordinary desktop placement.
The established target geometry/motion remains unchanged. Existing Bento has
priority regardless of open-space drop; absent Bento, future confirmed edge snap
must create a layout. That preview/intent/creation path is not yet implemented.

Evidence: `/tmp/kadunce-unload-test.LhrYsX/session.log` exercises the production
destination adapter with real windows and test callbacks: source-commit rejection
does not move/admit the window, destination membership exists before release,
successful existing-Bento and no-Bento native placement work. It does not drive
the complete CardStageController touch gesture; prepared source tickets retain
their separate headless coverage. `/tmp/kadunce-bento-candidate.CLgKns/session.log`
passes full-candidate two-output regression. Eleven native tests and source/control/
live safety checks pass; installed/repair unchanged. No install/push. Next: reverse
tablet admission coordination, then explicit edge-snap intent/creation. Physical
touch/hotplug and post-commit interruption acceptance remain pending.

### Bento→tablet acceptance gate (2026-09-11)

Source departure is value-prepared before receiver admission. Receiver validates
eligibility, then invokes a synchronous source commit once. Rejection and stale
source generations keep the original source session. Receiver placement checks
weak window/output handles and a generation invalidated by tablet release or
another admission. Existing centered arrival remains unchanged; membership still
publishes after native placement. Full destination preparation is the next slice.

Evidence: `/tmp/kadunce-unload-test.08hlaB/session.log` passes rejected/stale
admission, one-shot accepted source commit and placement using the production
Bento sender with a test tablet receiver. This does not exercise the production
CardStage receiver or its release-interruption behavior directly. Earlier probe
failures were traced to the test receiver omitting destination geometry; the
final receiver applies both output and geometry before checking placement.
`/tmp/kadunce-bento-candidate.9ZQp7s/session.log` passes full-candidate two-output
regression. Eleven native tests, source checks and control/live safety checks pass.
Installed plugin and trusted repair hashes remain unchanged. Nothing installed
or pushed; physical touch/monitor acceptance remains pending.

### Prepared tablet membership primitive (2026-09-11)

CardWorkspaceState now prepares admission by value and commits it only against
the originating destination identity/revision. Source rejection leaves destination
unchanged; stale, foreign, expired and duplicate tickets never invoke source
commit. The callback is synchronous and source-model-only, followed by destination
publication. Immediate append and prepared admission share one implementation.
Outstanding detached carries are rejected to preserve their rollback.

WorkspaceStateTest covers empty/single/pair/stacked destinations, centered and
noncentered insertion, selection/neighborhood/stack equivalence, source rejection,
one-shot commit, foreign/expired tickets, and every semantic mutation invalidating
a pending destination plan. All eleven native tests plus source/control/live
safety checks pass. The first window-handling run was blocked by local D-Bus
socket permissions; the authorized rerun passed. Installed/repair hashes unchanged.
This is a state-layer slice only: no production arrival wiring, visual changes,
install, push, or new hardware acceptance. Next: separate tablet arrival's
presentation preparation/cleanup from membership publication and use this gate.

### Tablet arrival wiring (2026-09-11)

Active tablet sessions now use prepared admission before native placement.
The source-model callback and destination publication precede managed-window
connection, input cancellation, prior Active restoration and placement. Held-card
arrival rejects without consuming that grab. Old preview geometry is captured
before publication. finishNewArrival shares the established guest/animation/Active
completion between transferred windows and ordinary launches. Inactive tablets
still receive native windows; already-admitted members retain selection handling.

`/tmp/kadunce-unload-test.igckVF/session.log` runs the production Bento sender
and CardStage receiver with test host boundaries and real KWin clients. At the
receiver connection checkpoint, source ownership is gone, destination membership
exists, and native placement has not begun. Two tablet members remain registered.
The test separately waits for native configure before checking the destination
screen; immediate screen checks failed despite successful logical publication.
The probe releases its local CardStage after the synchronous checks, so this is
not animation/arrival-timer acceptance or full Effect lifecycle coverage.
`/tmp/kadunce-bento-candidate.sinxaE/session.log` passes full-candidate external
transfer/restoration/unload regression. Eleven tests and source/control/live safety
checks pass. Installed and repair hashes unchanged; no install/push. Physical
touch/hotplug, interrupted receiver cleanup, guest visuals and Active-arrival
acceptance remain open. Next: explicit edge-snap intent and destination creation.

### Explicit monitor destination contract (2026-09-11)

MonitorDropIntent resolves existing Bento to LayoutSlot, confirmed first-edge
placement to NewLayoutEdge, and otherwise to NativeDesktop. Existing Bento wins;
invalid existing target metadata rejects with no fallback. NewLayoutEdge requires
an explicit typed edge, not release coordinates. Native/new-layout targets bind
to output identity/revision; a future runtime adapter must advance/revalidate that
revision when topology, geometry or layout presence changes. This adds no second
state machine: preview/release/cancel still use CarrySession.

CarrySessionTest covers pointer/touch, all four edge values, existing-Bento
priority, native acceptance, missing/invalid edge data, changed output revisions,
movement invalidating edge preview, output removal and cancellation. All eleven
native tests, source checks and control/live safety checks pass. Installed plugin
and repair hashes unchanged. No runtime gesture/preview, new-layout creation,
installation or push occurred in this slice. Next: prepare the first-layout
destination adapter; U4 edge/page/seam and companion-commit choices stay explicit.

### Shared first-layout preparation (2026-09-11)

prepareBentoAdmission prepares both empty first-layout and existing-layout
candidates. It requires a named output, matching valid restore record, no
duplicate arrival and visible planner acceptance. Only the explicit arriving
card is appended; it does not discover/adopt unrelated desktop clients.
Card Line→existing Bento and Bento→Bento now call the same helper.

BentoSessionTransferTest covers successful first admission, unchanged source and
empty destination, hidden/rejected placement, missing output, mismatched/invalid
snapshot, duplicate membership and preparation of both sides of a first-layout
transfer. All eleven native tests, source checks and control/live safety checks
pass. `/tmp/kadunce-bento-candidate.7N0ojJ/session.log` passes full-candidate
external transfer/restoration/unload. Installed and repair hashes unchanged.
No install/push or runtime edge-created layout. Edge geometry, preview revision
validation and first-session publication are not implemented by this helper;
the one-card solver result does not settle the U4 companion/edge UX choices.
