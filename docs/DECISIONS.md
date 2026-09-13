# Decisions

## September13: animation continuation belongs after paint

Live input/model timing was prompt; source audit found prePaintScreen requested
continuation before KWin6.7.5 cleared the current output layer's repaint state.
Candidate records animation/drop-settle activity in pre-paint and requests its
next frame in postPaintScreen. Latching that activity preserves an endpoint frame
if the interval expires during paint; inactive frames do not self-schedule.
No timers, caches, geometry, input thresholds or native ownership changes.
J's plug/unplug improvement means power/runtime state remains an unproven factor;
do not equate this source correction with demonstrated end-user performance.
Never repeat the failed live showfps+screencast diagnostic; see live evidence.

## September13: live content wins over uniform filled previews

After same/worse readiness performance, J explicitly accepted proportional margins
instead of cropping, stretching or resizing native apps to fill every card.
Remove retained snapshots/readiness/shader-warmup work and use the earlier accepted
KWin live path. Renderer/controller source matches rigid-fan; rebuilt SHA c063 is
identical. Preserve44%, paging, fan order, tilt and ownership. This supersedes the
earlier readiness-before-motion plan: after one physical check, animation work.
Experiments remain archived; no broad rollback of accepted interaction work.

## September13: preview readiness before motion polish

Startup shader preparation is parked: journal shows it skipped during J's better
warm run. Readiness candidate instead fixes capture/current shadow-margin mapping,
rejects unsettled buffer-size captures, and projects visible live damage onto the
tablet repaint region. No waiting or restoration changes. Valid retained Active
images remain static; failed capture falls back live and is not secretly queued
for a native resize/capture later. J's first-visit Zen check gates acceptance;
after that, animation smoothing, not another startup optimization block.

## September13: startup preparation cannot own interaction

Following the retained-preview physical pass, J prioritized natural first use
without pre-capturing all applications. First slice exercises only shared card
shaders with two tiny scratch draws, separately scheduled and abandoned on input.
No application activation/resize/capture, no background app sweep, no wait for GPU
completion and no added persistent cache. Performance benefit is not established
until cold-start comparison; warm stability alone does not identify a GPU bottleneck.
Native input remains immediate; already submitted GPU work is not preemptible.

## September13: preview geometry first slice

After J's geometry audit approval, remove the tilted preview's horizontal bottom
cutoff, retaining the hard output fence and rotated shader aperture. Card Line
content uses proportional contain over a fixed neutral backing; native Active
and cross-display carry rendering remain separate. Backing and content use the
same bottom-right pivot, angle, target and opacity. Vacant seam uses32% black fill
plus established outline. Destination outline material stays explicitly unchanged.
This candidate intentionally precedes44% and fan-motion changes for physical
diagnosis. No claim of reduced texture allocations or fully solved geometry yet.

## September 13: visual slot1 is front, not vector index1

J clarified the touch separation works; confusion is the linear insertion UI
against a cyclic fan. Prepared visual-depth insertion converts depth relative to
the existing face, with depth0 selecting the newcomer as an explicit front drop.
Other depths retain the existing face. No storage reversal or browse-direction
rewrite. Preview depths and commit use that same ordering. Leftward slot motion
goes deeper into the left fan; rightward returns toward the front.

The placeholder is a compositor-only vacant card outline with the established
radius, neutral translucent treatment and slight fan tilt. It is drawn beneath
the real held card, which stays contact-bound. It owns no input/physical geometry.
This candidate is not a new gesture/motion engine or a freeze.

## September 13: placement does not imply selecting the newcomer

J accepted contact-driven paging and requested a local main freeze20f2007 before
this separate pass. Prepared drag insertion now preserves the destination's active
member by identity, adjusting its index when insertion occurs before it. The
stored vector still describes exact placement. Explicit browsing selects members;
legacy direct stack commands retain their explicit newcomer selection behavior.
Existing elevation/paint-order synchronization consumes the preserved selection.

Preview entry/exit uses open browse poses and a matching interpolated envelope,
not a closed fan as its starting point. A text-only slot number reports the live
revision-validated plan without input ownership or native geometry changes.
This is a bounded candidate, not physical acceptance of full geometry/motion.

## September 13: held navigation requires contact intent

Interaction-only candidate: stack entry establishes an insertion anchor without
requesting navigation. A fresh horizontal contact displacement requests one slot
after dwell; reaching an end slot cannot start row paging. Only physical screen
edge contact permits held-row paging, and each repeat revalidates that condition.
Insertion and row paging cancel one another's timers. Size/moving card geometry
must not manufacture navigation intent. Entry-side placement and selected member
remain separate from persistent committed membership order. Geometry/animation
experiments stay out of this candidate until J accepts basic interactions.

## September 12: cross-output native landing

NativeDesktop intent is not restricted to the source output for an ordinary
source. A free external destination may accept it after temporary carry capture.
Managed-source departures, occupied targets and tablet targets retain their
workspace admission paths; they cannot use this cross-output exception.
Preparation and release revalidate the same rule. Live trace showed the rejected
window used an ordinary restore source despite looking Active-sized; do not infer
card membership from geometry to fix a destination-validation error.

## September 12: free-space appearance and bottom landing

Do not hand input back to KWin mid-contact when an ordinary-source carry crosses
tablet and returns to free monitor space. Keep the safe capture, suppress only
NativeDesktop placement outlines, and commit the ordinary destination on release.
Explicit Bento departure still has its outline; real edge previews remain.

For a proven ordinary native drag ending at the physical monitor bottom, schedule
one translation after native finish, retaining size and a reachable title bar.
Only the owner's physical release schedules this; Escape/cancel, extra contact,
unload and a new active native drag do not force a placement. No dock input is
consumed or replayed, no membership is created, and no geometry retry is added.

## September 12: proof is not ordinary-window ownership

A proven native ordinary drag remains KWin-owned until an entry edge or crossing
into a card workspace. Keep the revision-bound contact and original source
reservation, not an input grab. Other input, native finish, stream loss or unload
retires the pending entry. Existing cards retain immediate takeover.

The acquiring input filter runs before KWin applies that motion event. Project
the pending contact delta once into the carried pose; do not move the native
window to catch up. A single-event jump across a display otherwise leaves its
carried rectangle on the source, and destination intersection correctly rejects
the drop. Original restoration and visible pickup geometry remain separate.

Native output drift is allowed only during the pending native move with source
identity, generation and home topology unchanged. Cancellation returns home;
then ordinary strict validation resumes. This does not relax occupied-target
transactions or introduce geometry retries. Physical confirmation remains required.

## 2026-09-12 — Xwayland unload contract corrected against native baseline

The raw release counter originally blocked the Xwayland candidate. Native-only
KWin cancellation, with Kadunce never loaded, produces the same paired mouse-up
on re-entry (/tmp/kadunce-unload-test.DtD7bu). Xwayland's pointer_handle_enter
explicitly synthesizes releases to reset held-button state. Therefore zero raw
releases was not a valid native-equivalence invariant. User explicitly approved
matching native behavior while preventing accidental actions.

Keep the observation, allow at most one matched cleanup release with no new press,
and require separate real QPushButton action tests for native and Kadunce pointer/
touch cancellation over a control, followed by a positive fresh-click check.
Those pass at /tmp/kadunce-unload-test.5AtSWO; full Xwayland entry/rejection/unload
passes at /tmp/kadunce-unload-test.DcWRwX. No production runtime code changed for
this resolution. No retained blocker or synthetic click was added.
Upstream source: https://github.com/mirror/xserver/blob/master/hw/xwayland/xwayland-input.c
This does not prove every toolkit or physical app; keep GPT acceptance outstanding.

## 2026-09-12 — Xwayland client-request correlation

Captured GPT trace demonstrated missing proof for undecorated X11Window moves,
not an app-specific layout error. Observe EWMH move requests after KWin's earlier
RootInfo filter has accepted the native start. Require that new same-turn start,
exact window and sole live contact with saved input-surface ownership. Resolve
the contact revision again before admission. The request is neither consumed nor
replayed; the existing carry handoff remains the only takeover/restore owner.
Expire unmatched starts next event turn; remove observer on X connection teardown.
No global snapping switch, titlebar guess or per-app allowlist. This fills protocol
coverage only; delayed ordinary-window ownership and occupied-tablet admission
remain separate roadmap items. The real private Xwayland client uses Qt's native
pointer request and emits EWMH from an actual TouchBegin for the touch case.

## U2 native application invalidation — 2026-09-11

Never hold a live Session reference across KWin setters. Apply a value plan with
weak native handles and generation checks at each mutation boundary; obsolete
passes stop, and cleanup is token-scoped. Reuse the same guarded path for settle
geometry without changing its timing policy. Logical admission is still not
asynchronous native configure acknowledgement. Restore-loop/compositor validation
and remaining transfer adapters are still required before installation.

## U2 first connected transfer — 2026-09-11

Connect managed Bento-to-Bento first, where one owner can prepare both session
values and publish without intervening native callbacks. Require destination
visibility before preparing source removal; reject without model changes and
replay source geometry after a rejected native drag. Do not generalize this to
tablet/native destinations or claim physical atomicity. Native application still
needs lifetime/acknowledgement recovery before candidate installation.

## U2 visible destination admission — 2026-09-11

Transfer-specific Bento search requires the arriving candidate rather than merely
preferring it. Solve on a session copy before publishing/applying; ordinary Bento
reflow stays unchanged. Tablet admission now acknowledges output/membership
success through bool. These are preparation/acknowledgement boundaries, not yet
source/destination atomicity or a persistent reservation. No install/push.

## U2 source preparation — 2026-09-11

Prepare removal on a private CardWorkspaceState candidate; leave the live source
unchanged until commit. Guard commit by owner lifetime and semantic revision,
not mutable row indices or visible-state equality. Do not reorder the native
handoff calls alone: tablet admission currently returns void, and Bento admission
can park the arriving window. These require explicit destination acceptance and
recovery before production transfer uses the new API. Installed build unchanged.

## U1 carry contract — 2026-09-11

Use a headless CarrySession with device/contact ownership, free global pose,
versioned destination intent and one-shot outcomes. No native pointers or model
mutation. Acceptance means ReadyToCommit, not committed: U2 adapters must reserve,
revalidate and recover transactionally. Source changes require recovery instead
of blindly restoring a stale record. See CARRY-SESSION-CONTRACT.md. Production
dragging, installation and trusted repair are unchanged.

## Unified-card direction — 2026-09-11

User approved Card Line, stacks and Bento as arrangements of the same card
identity, with free 2D carry across displays, destination-led morphing, and
invalid-drop restoration of order/focus. Only the carrier may cross the tablet
fence. Safety and unrelated native input remain independent. Implement in
UNIFIED-CARD-BLOCKS.md order. U1 source audit found horizontal clamping,
center-bound insertion, mouse/touch destination asymmetry and premature source
removal. No runtime changes in this planning/audit turn. Rail/companion UI and
focused-versus-expanded return behavior remain explicit design checkpoints.

## Block 2 implementation decision — 2026-09-11

Local candidate: extract CardWorkspaceState as the sole membership/model owner,
with explicit state commands and const views. Keep weak native handles at the
adapter boundary and UUIDs in external snapshots. Do not duplicate a second
identity registry or change release/reset policy in this behavior-preserving
step. Timers, restoration and output policy remain in the stage controller.
Nine native tests and safety/source checks pass; no candidate installed or repair
snapshot refreshed. Persistence across release/unload remains separate work.

Reconciled 2026-09-11 against current source and CashyOS Pillow Talk's September
10–11 messages. Discussion suggestions are distinguished from implemented behavior.

## ADR-001 — Repository memory

Status: adopted workflow. CURRENT_STATE is the compact starting point;
ARCHITECTURE describes current ownership; this file records decisions; research
and dated test records are read on demand. Source/runtime evidence overrides
stale narrative. Each substantial session updates state before handoff. This
reduces repeated reconstruction, not a guaranteed percentage of account usage.

## ADR-002 — Effect architecture correction

Status: verified fact plus approved investigation. Kadunce already subclasses
KWin::OffscreenEffect and installs a native KWin plugin. Card Line already uses
compositor transforms instead of placing client windows off-screen. The proposed
comparison is custom OffscreenEffect rendering/input versus a KDE QtQuick scene
effect path, not converting a script into an effect. Choose after a minimal spike.

## ADR-003 — Workspace truth independent of presentation

Status: desired architecture; not implemented. Window identity, stack order,
selection and output membership should survive hiding/reopening the presentation.
KWin remains authoritative for real clients, focus and physical outputs.
Extract a domain model and action interface first. Whether a separate service is
needed depends on required survival across effect unload and compositor restart;
do not add IPC to each animation frame. No cross-login identity matching by title.

## ADR-004 — Companion boundaries

Status: existing ownership retained, backend lifetime change proposed. Tette owns
search and invokes workspace actions through a stable contract. Temperance owns
system status/actions. The Effect presents workspace state. Preserve context v1
and guest v3 during extraction; any service move needs explicit discovery,
capabilities and loss recovery. Existing standalone Tette must continue working.

## ADR-005 — KDE reuse before more touch implementation

Status: approved investigation. Study Desktop Overview, Plasma Mobile and QtQuick
input first. Map every candidate by input device, progress/cancel lifecycle,
output behavior, dependency stability and installed version. Global swipe handlers
are not assumed to be one-finger card-drag handlers. Reuse percentage unmeasured.

## ADR-006 — Snap intent opens composition

Status: product direction from Pillow Talk; prototype pending. Partial native
snapping should offer companion cards for remaining space on tablet or monitor.
Maximize/fullscreen is complete intent and should not summon a chooser. Ctrl+B
remains an explicit entry. KWin owns tiling/constraints; Kadunce owns suggestions
and composition intent. Always-present tablet behavior must preserve emergency
disable and deliberate release. No permanent lock-in is authorized.

## ADR-007 — Acceptance and repair provenance

Status: required delivery discipline. Build success and unit tests do not establish
motion quality. Track candidate, installed, loaded and user-accepted separately.
Repair must resolve to accepted source associated with an identifiable build.
Rejected touch code must not reappear through installer or repair. Existing
monitor-only repair snapshot requires reconciliation, not an assumed acceptance.

## ADR-008 — Cancellation follows the owning input stream

Status: first local candidate implemented, lifecycle work pending. Touch cancel
must not reset pointer state or pointer-owned timers. Shared holds retain their
initiating device; a canceled touch grab rolls back once. Forward cancellation
when a client received any contact in the stream. Do not confuse this targeted
separation with complete simultaneous-device arbitration or lifecycle cleanup.
See INPUT-OWNERSHIP.md for tests and remaining work. No timing changes or install.

Lifecycle slice: controller invalidation uses a typed host callback. Canceling an
action does not relinquish delivery ownership of a consumed contact: the router
drains its release. Client-forwarded contacts are not claimed. Normal completion
relinquishes action ownership before potentially reentrant callbacks. Effect
unload and native takeover need further reconciliation, not blanket success.

Native takeover slice: native KWin move/resize wins at router event/timer
boundaries, including over stale pointer drains. Consumed touch contacts still
drain separately. The first workspace gesture owns navigation; competing-device
workspace presses are inert until release, without claiming external desktop
input. Multi-button and unload/filter-chain reconciliation remain pending.

Pointer-chord slice: forwarded client/Tette input is tracked by all held buttons;
first release does not end the stream. Extra outside-guest buttons drain without
stealing the primary gesture's motion. View toggle, Active entry and guest entry
invalidate actions using the existing host boundary. Async admission/activation,
queued commands and unload remain review/validation items.

Deferred-command slice: input activation carries both a generation ticket and the
original weak window identity. Host invalidation/paging/admission revoke queued
intent; callbacks cannot activate a replacement selection. Arrival expansion
rechecks tablet ownership and manual move/resize without changing durations.
Existing guest generation checks remain. Physical acceptance is still pending;
Plasma Mobile research must be unpacked before touch redesign.

Unload gate: explicit router cancellation/destruction now precedes restoration
and controller destruction. Real KWin/client delivery tests and a full-effect
virtual-tablet fixture pass pending/lifted-input unload and fresh-input recovery.
The fixture's sole output-classification change is not production code. Hardware
and visual acceptance remain separate; see UNLOAD-VALIDATION.md.

Safety follow-up: physical testing found that the dock can open its tray during
a touch hold but Disable is unreachable. Temperance uses a separate AppletPopup;
the input exemption only recognized Dock. Extend the existing panel hit-test to
shown, current-desktop/activity AppletPopup surfaces, not arbitrary dialogs or
background desktops. Keep owned pointer drags and competing card presses intact.
Router regression covers dock → popup mouse delivery during a lifted touch,
explicit cancellation, and inert finger release. Physical acceptance still required.

U2 restoration ownership: Bento rejects activation/admission during a restore
scope instead of allowing synchronous native callbacks to create a new session
under an old restoration pass. Check weak handles between native restore setters;
abort invalid continuation without changing the established restoration order.
This barrier is synchronous, not a new mode or input timing delay. Interrupted
output/configure recovery remains separate; no implicit installation approval.

U2 bounded geometry failure (superseded below): retain the existing four 90-ms correction attempts.
If the final attempt corrected geometry, schedule one additional read-only
observation, rather than classifying its pending configure as an immediate
failure. A still-mismatched session returns to saved desktop state on that output;
other outputs stay managed. No endless resize fight, no automatic overflow or
unrequested transfer back to a prior display. This is bounded fallback, not
proof of client configure acknowledgement; slow clients and interrupted restore
need further validation before installation.

User-confirmed reconcile-once boundary supersedes the corrective settle loop:
request native placement once per semantic operation, wait once for 450 ms,
then observe. Keep matching sessions; restore mismatched output sessions once.
Remove the geometry-only application path and retry counter rather than retaining
an unused enforcement mechanism. Native user move/resize still stops pending
observation. The 450-ms grace retains the prior maximum observation budget;
it is not a touch hold duration or evidence of protocol acknowledgement.
Future acceptance-driven refinement must not reintroduce timer-based resize fights.

Restore topology recovery: retain each original RestoreSnapshot across a finite,
deduplicated list of weak output candidates. Only output loss permits advancing
to another candidate; closed clients or native user takeover abort. Each output
is attempted at most once, no timers or resize retry. Prefer the recorded output
(or existing removal replacement), then tablet, then remaining current outputs.
Clamp fallback geometry to its work area and do not replay stale floating/
fullscreen restore coordinates onto a different display. Removal notifications
exclude an output immediately, even before KWin updates its list; screen-added
clears that exclusion. If all candidates disappear, warn and leave native placement
to KWin; there is no cross-unload pending restoration service. Physical hotplug
and all-output-loss recovery remain unproven.

Bento-to-ordinary-desktop adapter: ordinary native desktop acceptance means a
live non-retired destination and valid target geometry, not a Bento layout slot.
Prepare source departure by value with the same helper used by Bento-to-Bento;
reject ambiguous membership or impossible source reflow before publishing.
Publish departure, issue guarded native placement once, then reflow remaining
source panes and observe. Remove the unreachable duplicate destination branch.
Post-publication interruption stops further mutations, but does not constitute
transaction rollback or asynchronous geometry acknowledgement. Tablet admission
remains a separate legacy path; no change to gesture or animation policy.

Confirmed monitor drop priority: existing Bento receives the incoming card;
without Bento, open-space drop is native desktop and an explicit edge snap
creates Bento placement. Do not treat open space on an already-managed monitor
as an override, and do not infer a confirmed snap solely from release coordinates.
Edge preview/intent and first-layout creation remain pending.

Card Line→monitor commit boundary: destination solves its value candidate first.
Synchronous commitSource callback may only commit the prepared source model
removal; destination publishes immediately afterward without native calls between.
Only then does releaseSource clean up source visuals/shortcuts, followed by
guarded native placement. These callbacks are not queued or stored. A rejected
destination/source commit leaves ownership intact and cancels the grab back to
its original detached state. Existing Bento rejection never becomes a native
window. Post-commit native interruption remains guarded, not physically atomic.

Bento→tablet admission gate: prepare the remaining Bento session by value,
then let the tablet receiver validate eligibility before committing source
removal through a synchronous one-shot callback. Rejection/stale generation
preserves the source. After commit, native interruption is consumed rather than
reported as rejection (which would replay stale source ownership). Weak handles
and a receiver generation stop further placement after release/reentrant admission.
This is deliberately not full two-model publication: tablet membership and arrival
still use the existing post-placement path, and closing a launcher guest can occur
before acceptance. Next is prepared tablet membership, not animation tuning.

Prepared admission belongs to CardWorkspaceState, alongside prepared removal,
not to a parallel transfer registry. Reuse the same append primitive for immediate
and prepared admission. Destination lifetime/revision validation precedes the
source-model-only callback; successful source commit is followed immediately by
destination publication. Callbacks must not mutate the destination, emit signals,
or invoke native/presentation operations. Reject duplicate membership and an
outstanding detached carry rather than erasing its rollback. No new persistence
or cross-presentation identity guarantees are implied. Production arrival wiring
is a separate slice; this adds the tested state primitive only.

Tablet production wiring: an incoming new member in an active tablet session
uses prepared admission, committing source removal and destination membership
before connection/cancellation/restoration/placement. Old Card Line geometry is
captured before publication. Reject while a tablet grab is active instead of
discarding its rollback. Inactive tablets retain native-window behavior; already
admitted windows retain the existing selection path. finishNewArrival is shared
with ordinary app admission to avoid a second animation/guest-policy implementation.
Release retains Active restoration until cancellation completes, and invalidates
further transfer placement. These are logical publication guarantees, not native
configure acknowledgement or a new global persistent card registry.

Monitor drop semantics use the shared CarrySession destination, not an independent
edge state machine: LayoutSlot for existing Bento, NewLayoutEdge with explicit
CarryEdge for confirmed first placement, NativeDesktop otherwise. The pure
MonitorDropIntent resolver never consumes coordinates or installs timing policy.
Existing Bento takes priority even with an edge target; malformed/infeasible
existing admission cannot fall back to a native window or another layout.
No-layout targets bind to output identity/revision; adapters must advance that
revision on topology/geometry/layout-presence changes and revalidate before commit.
This is headless contract work, not proof of a runtime revision provider or snap
preview. U4 seam/page precedence and companion commitment remain unchosen.

First-layout preparation reuses prepareBentoAdmission rather than adding a second
solver or calling activate(), which discovers/adopts other desktop windows. The
helper accepts an empty output-scoped Session, appends only the explicit arrival,
and requires visible solver acceptance. Existing Card Line→Bento and Bento→Bento
preparation use the same helper. No new runtime first-layout publication or edge
geometry policy is enabled; the single-card solver result is not a chosen final
edge shape or companion-commit UX. Next checkpoint: preview/commit integration
with the U4 choices still explicit.

Native snap experiment (not production): Effects-priority input can transfer a
native move to CarrySession before native preview/commit, retaining global KWin
options. Keyboard input must participate: Shift before motion otherwise bypasses
adoption. See NATIVE-SNAP-VALIDATION.md for positive controls and limits. Cancel
native movement only after preparing the carry owner, and distinguish its finish
signal from user drop in production. No claim that native dragging continues
after cancellation; visible carry integration must precede runtime enablement.
No global snap configuration edits or bottom-dock input changes authorized here.

User clarification after the snap experiment: a card snapped into Bento on a
monitor with no other windows should take Active presentation and accept later
cards dragged into that Bento layout. Keep layout membership distinct from its
one-card presentation; do not interpret this as releasing the layout to ordinary
desktop ownership. A companion chooser is an idea to revisit through user intent,
not approved automatic UI.

Subsequent user clarification settles the occupied-monitor case: an edge snap
activates Bento for the destination display, promoting all its eligible ordinary
application windows along with the arrival, analogous to Card Line activation.
This is the mouse entry alternative to Ctrl+B. Reuse existing eligibility and
display ownership policy; never recruit other displays, panels or Tette's guest.
Existing Bento accepts the arriving card through its existing layout path. With
no existing Bento, an open-space drop remains a normal window; only the edge snap
requests display-wide promotion. A lone arrival uses Active presentation while
retaining Bento membership. No automatic companion prompt.

This supersedes the earlier arrival-only first-layout target policy, not the
description of today's source. Implement first activation as a prepared batch
snapshot/layout and validate before source removal or native changes. Do not
call a mutating activate() midway through the transfer. Source currently has only
the single-arrival planner; batch activation, pre-snap takeover and visuals remain
pending. Preserve bottom dock and persistent safety control.

Prepared batch activation slice: prepareBentoActivation accepts an empty
output-scoped Session and complete caller-collected restore snapshots. Reject
invalid/duplicate identities, existing sessions, missing required arrival, failed
layout or lost/duplicated visible/overflow membership. Preserve all snapshots;
promotion does not imply unlimited simultaneous panes. Existing Ctrl+B activation
now solves and validates before Session publication, using its existing collector
and optional-preferred policy. Edge callers will require the arrival visible.
No runtime edge activation or changed lone-card geometry in this slice.

Verification: eleven native tests, source/control/live safety checks and full
candidate two-output Bento transfer/restoration/unload pass. Virtual evidence:
/tmp/kadunce-bento-candidate.EIza82/session.log. Build retains pre-existing missing
restore-field initializer warnings in admitCardWindow. Installed plugin and trusted
repair hashes unchanged. Next Heavy task: prepared edge-arrival merge/source commit
and native-to-carry input/rendering integration. Bounded checklist, documentation
and packaging work can be handed off separately; no model/chat changed.

Explicit activation adapter: transferCardWindow accepts OpenSpace (default,
existing behavior) or ActivateBento, never inferred from release coordinates.
First activation collects destination-owned eligible clients, merges the arrival
exactly once with its supplied restore record, prepares the batch with required
arrival visibility, then commits source model and publishes destination before
native/visual cleanup. Rejected source leaves resident membership and geometry
unchanged. Tablet/forbidden outputs and ineligible arrivals reject. No call to
mutating shortcut activate() is used during transfer. Existing Bento admission
still takes priority; failed layout never falls through to ordinary desktop.

Validation: eleven native tests and source/control/live safety checks pass.
Real-controller private test /tmp/kadunce-unload-test.y7GLU5/session.log verifies
publication ordering, rejected source, same-output deduplication, forbidden tablet
activation and a foreign-display witness. Full candidate regression passed at
/tmp/kadunce-bento-candidate.Gxp7bU/session.log. Source callback is a test stub in
the new isolated case; source model semantics have their separate tests. No input
caller enables the new intent yet, no native-titlebar restore-policy change, no
new lone-card geometry or gesture/preview. Installed plugin/repair unchanged.

### 2026-09-11 — Live Card Line carry pose before native adoption

Inspection found production carry still clamped horizontal displacement; touch
did not update destination or call the monitor release adapter. Fix the existing
owned path before enabling native-titlebar cancellation. Router now sends the
actual pickup contact and absolute 2D motion, including outside tablet bounds.
Controller captures the pickup rectangle and contact once; renderer translates
that rectangle without shrink/lift or stack-preview attraction. Destination stack
feedback remains separate, and distant vertical motion cannot arm the old stack
candidate or horizontal reorder. Both devices use the existing transactional
output admission. A missing output cancels instead of reordering the source.

Only an admitted carried tablet card gets transformed painting on other outputs;
ordinary external windows retain native painting and passive tablet neighbors
remain hidden. Each paint pass has a hard output fence. Native window geometry
is untouched during carry. Native titlebar moves remain native: no new snapping
suppression is enabled, no production ActivateBento intent, no new preview
resolver, and CarrySession tickets are still headless. Release settling/animation
and stack pickup rotation continuity still need visual evaluation. Do not call
this the completed unified drag implementation or an install-ready build.

Evidence: build, all eleven tests, source/control/live safety checks pass.
PanelInputTest covers mouse/touch pickup, unrestricted 2D delivery, cross-output
release and foreign contact rejection; existing mixed-input kill-switch tests
pass. WindowHandlingTest covers carried/passive output routing. Private real
CardStageController tests preserve pickup/native geometry during large 2D motion
and cancel; included in productionTabletAdmission at
`/tmp/kadunce-unload-test.kS9ptm/session.log`. Full candidate two-output Bento
regression passed at `/tmp/kadunce-bento-candidate.Vw21YI/session.log`.
Hardware/visual renderer acceptance not performed. Installed and repair hashes
match CURRENT_STATE/ROLLBACK-PROVENANCE. Nothing installed or pushed.

### 2026-09-11 — Native cancellation has an explicit takeover boundary

Added NativeMoveTakeover, exercised by the private native snap probe rather than
copying a raw cancelInteractiveMoveResize call into Effect. It validates a
caller-provided source reservation, holds the observed native snapshot separately
from the source's authoritative restore token/revision, and begins CarrySession
before canceling KWin once. The native finish guard remains published throughout
the synchronous callback even if carry is canceled within it. Recursive adoption
is rejected. Post-callback validation cannot resurrect canceled/invalidated carry.
The adapter never moves/resizes native geometry during motion and returns source
outcomes exactly once. A rejected preflight leaves KWin entirely native.

This is deliberately not activated in Effect: current Active/Bento start/finish
handlers do not yet reserve source ownership, and the probe explicitly supplies
the physical owner. Enabling the boundary ahead of those adapters would turn a
safe native drag into an invisible or unfinishable carry. Next task is prepared
source reservations retaining pre-Active/Bento restore state, then owner discovery
and source-specific renderer/drop/recovery callbacks. Ordinary external windows
must not be adopted merely because the plugin is loaded.

Evidence: `/tmp/kadunce-unload-test.RImJ7t/session.log` passes real native
pointer/touch snap positive controls, intercepted edges and early Shift, guarded
finish, synchronous cancel/source invalidation/reentry, injected output removal,
foreign owner rejection, one-shot token/revision outcomes and rejected source
passthrough. Client closure handling exists but new close-signal coverage and
hardware testing remain open. All eleven existing tests and source/control/live
safety checks pass. Installed plugin and repair hashes unchanged; no install/push.

## 2026-09-11 — Authoritative native-carry source preparation

Active/Bento expose immutable PreparedCarrySource values containing the original
restore snapshot and source identity/revisions. These are read-only reservations,
not locks: no membership mutation, geometry writes or rollback on destruction.
Validate before and after native cancellation, including output geometry and
controller identity. The token rejects stale release/reentry and cross-controller
use. The observed native frame in NativeMoveTakeover is not the desktop restore
state. Full Effect integration remains deferred; the private probe guards native
finish explicitly instead of forwarding it into legacy release/drop callbacks.

Candidate builds; eleven tests and source/control/live checks pass. Normal
Active/Bento takeover, cancel and interruption passed in
`/tmp/kadunce-unload-test.mgO30p/session.log`. Full candidate regression passed
`/tmp/kadunce-bento-candidate.zaoczT/session.log`.

Expanded initially minimized + maximized Bento coverage is deliberately failing.
Saved state remains MaximizeFull but visible restored geometry is 1260x770 rather
than original 1280x800. Showing the client does not repair it. No-drag baseline
reproduces the same mismatch in `/tmp/kadunce-unload-test.POW39k/session.log`;
therefore takeover is not required for the failure. Exact cause remains open
(decoration/configure ordering is a hypothesis). Keep the exact visible-frame
assertion; do not add geometry retries or pretend maximized flags prove recovery.
Next bounded task: isolate/fix restore ordering, rerun expanded private suite,
then physical-owner discovery and renderer/drop integration. No install or push.

## 2026-09-11 — Restoration investigation freeze, not release acceptance

Plain KWin maximize/minimize reproduces the undersized restored frame without
Kadunce controllers. Protocol trace shows correct requested size and buffer but
stale client window bounds. Separating minimization until exact visible geometry
is restored passes. See RESTORATION-FREEZE.md for repeatable controls and evidence.
Requested-state guards and earlier minimization experiments did not fix it and
were reverted. No production workaround retained. A future acknowledgment-gated
minimization operation must explicitly survive/cancel across close, manual input,
output loss and disable/unload; callbacks cannot outlive plugin code. No arbitrary
timer, resize retries, or waived geometry assertions. Freeze today as development
checkpoint with expanded restoration suite red, not as completed restoration.
Eleven standard tests and source/control/live safety checks pass; installed and
repair hashes unchanged. No install or push.

## 2026-09-11 — Observed restoration before minimization; instant disable priority

User approved leaving a previously minimized window visible when disable interrupts
restoration. Do not delay the kill switch. DesktopStageController now owns bounded
RestoredMinimization observers: geometry/state is issued once, then matching native
reported/requested geometry and state permit one minimize request. Already matching
state uses the synchronous path. The deadline cancels, never forces a resize or
minimize. Owners cancel before destruction; output retirement, newer layouts and
native interaction invalidate pending work. No callback belongs to the client or
survives plugin/controller lifetime. No new service or event loop.

Expanded restoration, interruption, owner destruction and fullscreen/minimized
tests pass; the plain KWin negative control still fails. Eleven standard tests and
source/control/live checks pass. Full-candidate two-output restore/unload passes.
See RESTORATION-VALIDATION.md for paths and explicit coverage gaps. In particular,
reported state is not a serial-correlated Wayland acknowledgment; owner destruction
and general full-effect unload were tested separately. Installation remains blocked
on the broader refactor/integration gates. Installed/repair unchanged, no push.

## 2026-09-11 — Contact candidates are not proof of native-move ownership

CarryContacts is an isolated, headlessly tested ledger with lifetime-local tickets.
A single held contact is only a candidate: KWin move-start lacks initiating-device
identity, touch events expose seat IDs rather than device pointers, and spies lack
a cancel callback. Require verified correlation and complete cancellation before
native adoption. Never guess a finger from global pointer position or silently
substitute a newly pressed contact. No production input path changed. See
NATIVE-CONTACT-OWNERSHIP.md for the next private-compositor proof.

## 2026-09-11 — Prove client move serials before choosing a runtime seam

Private KWin probe passes real Qt startSystemMove with mouse/touch, mixed-input
rejection, MoveOp non-correlation, cancel/device removal and post-observer client
delivery. Twelve tests and source/control/live safety checks pass. Existing native
snap probe still passes. No production changes, install or repair promotion.

The window's native-start signal precedes the protocol listener; pointer focus
also disappears before that listener. Capture press provenance before native
movement, and never release Active through the old start callback before adoption.
Do not use XdgToplevelWindow::shellSurface directly: its symbol/metaobject are not
exported. The test uses QObject shell discovery for newly created clients only;
this is evidence, not an approved production dependency. Next resolve discovery
and existing-window lifecycle, then server-decoration correlation. See
NATIVE-CONTACT-OWNERSHIP.md for source links, logs and untested boundaries.

## 2026-09-11 — Exported surface lookup replaces private shell discovery

NativeMoveProtocol uses exported Wayland resource enumeration scoped to one client
and verifies exact surface identity. It returns a weak toplevel handle, works for
already-open windows, and needs no private QObject hierarchy or unexported symbols.
Attach on the compositor thread, never scan per frame. The probe links Wayland::Server;
the production plugin does not consume this helper yet.

Private tests pass observer recreation, same-client multiple surfaces, close/new
client, and genuine Breeze titlebar mouse/touch motion. Decoration dispatch timing
is proven only for motion-driven starts. Delayed holds, XWayland and full keyboard
arbitration remain unsupported; do not infer ownership for them. No live theme or
input edits. Evidence and remaining gates: NATIVE-CONTACT-OWNERSHIP.md.

## 2026-09-11 — Retain source across native-start identification

NativeCarryHandoff stages a prepared source until correlation in the same event
turn. It adopts only after source/contact validation; the synchronous native
finish is not a drop. Rejection falls back once, and cancellation invalidates
queued work. Source membership is never removed by this coordinator. Input proof
is required through adoption, not after physical release during destination work.
Real Active/Bento app-request tests pass acceptance, rejection and synchronous
cancel; fallback tests use a sentinel, not Effect's handlers. Painting/draining
and production routing remain unwired. See NATIVE-HANDOFF-COORDINATOR.md.

## 2026-09-11 — Share tested admission observation, not test instrumentation

NativeMoveObserver now owns passive contact/protocol/decoration correlation.
ContactProbe delegates to it, removing duplicate implementation. Observation and
event consumption remain separate: no experimental filter priority is promoted
into production. It follows multiple window/decorations and device lifetimes;
touchscreen removal invalidates seat-scoped reservations. Protocol-less starts
remain native. See NATIVE-MOVE-OBSERVER.md for the remaining integrated gate.

## 2026-09-11 — Keep physical release separate from destination acceptance

CarryInputRoute retires input intent before returning a terminal action. A canceled
carry retains its swallowed releases; other input modalities remain available.
Acquire only after native cancellation and revalidation of the held contact.
Private Active/Bento mouse/touch tests now connect this route to the handoff;
no-preview release returns to source. Production priority, painting, destination
commit and combined teardown remain gated. See CARRY-INPUT-ROUTE.md.

## 2026-09-11 — Project carry without native geometry writes

CarryPaintPlan keeps frozen pickup size and anchored logical position. Its shared
paint adapter reuses KWin's cover transform and clips per output. Only the private
handoff probe consumes it so far; production routing and aperture integration
remain gated. See CARRY-PAINT.md. No passive-card fence changes.

## 2026-09-11 — Retire accepted Active departure without source restoration

Active carry transfers reuse the existing receiver admission callback boundary.
Source model removal and restore-record retirement precede receiver native work;
receiver rejection changes neither. Remaining source cards stay in Card Line,
and later source release cannot pull back the departed window. This adapter does
not itself validate a preview ticket or enable native takeover in Effect. See
ACTIVE-CARRY-DESTINATION.md.

## 2026-09-11 — Reserve receiver state before source commitment

DesktopStageController owns opaque, one-attempt drop reservations. Validate output,
work area, layout presence, generation and residents again before source commitment;
do not reinterpret stale previews as another destination. Prepared Bento proposal
calculation must not invalidate the live application generation. Existing unprepared
callers retain their behavior. The private Active departure probe uses this boundary;
CarrySession ticket resolution and Effect integration remain pending.

## 2026-09-11 — Resolve carry before native receiver placement

NativeCarryHandoff binds the exact preview ticket and destination to receiver
validation and a guarded synchronous transaction. Owner release resolves that
ticket before native placement, retiring source observers which would otherwise
interpret the accepted output change as invalidation. ReadyToCommit is not
placement success: transaction rejection reports return, and logical publication
is separately recorded. Cancellation during validation prevents commitment.
The private Active/native pointer/touch route now consumes this boundary; full
Effect and Bento-source routing remain pending. See ACTIVE-CARRY-DESTINATION.md.

## 2026-09-11 — Reuse Bento handoff behind reserved release

Bento source release validates source and receiver reservations before entering
the existing value-copy handoff transaction. It does not create another placement
implementation. Native-desktop and existing-layout proposals leave controller
generation untouched until publication/application; rejection preserves source
validity. The adapter rejects edge activation and same-output drops rather than
inventing missing routing. Pointer/touch release integration is isolated-probe-only.

## 2026-09-11 — Connect native carry in the production Effect

Effect owns NativeCarryRuntime for admitted Active/Bento native moves. Source
reservation precedes cancellation; synchronous native finish cannot release the
stage. Input routing, compositor-only projection and reserved receiver commit
are connected together. Unsupported starts retain legacy handling. Passive tablet
clipping is unchanged. Existing Card Line gestures remain a separate route until
the remaining unified destinations and stack interactions are connected.

The real plugin passes virtual Bento pointer/touch transfer and held-unload tests,
plus full restoration regression. Active adapter tests separately pass; virtual
outputs do not establish production tablet acceptance. Eager redirect at adoption
was removed after a virtual-compositor crash; existing shader-aware draw owns it.
No install or repair promotion. See PRODUCTION-CARRY-INTEGRATION.md.

## 2026-09-11 — Reuse receivers for tablet return and edge-created Bento

Native carry now resolves tablet arrival through the existing Bento-to-tablet
acceptance callback. Edge-created Bento uses the existing complete display batch
planner and prepared source departure, publishing both before native placement.
Existing layouts retain priority. Motion recognizes top/left/right intent; bottom
48 logical px and panel hits stay excluded. Leaving an edge discards that preview.
No new visual preview, same-output reorganization or Card Line router conversion
is claimed. Full-plugin mouse/touch tests cover open, edge and withdrawn-edge.

The Virtual-0 tablet predicate fixture caught stale reported maximize state during
Active entry. Use requested native state to identify new intent; native-start owns
move/resize admission, not frame-geometry notifications. Full Effect Active pickup
and tablet return pass after this change. Production panel identification unchanged.

## 2026-09-11 — Share Card Line receivers, expire stack targets

Card Line motion now reserves DesktopStage destinations using the same edge
recognition and receiver validation as native carry. Release cannot invent a
new target; it commits the reserved projected rectangle or cancels. The existing
router still owns input lifetime. Tablet edge paging stops on foreign outputs.

An armed stack is no longer exempt from live target checks. Commit additionally
checks workspace revision. No-target release restores a detached member's original
stack rather than leaving it detached just because it moved. Full Effect tests
cover external destinations, withdrawn stacks, original order, and left/right
insertion with touch/pointer. Existing dwell timing and visuals are unchanged.
Evidence: /tmp/kadunce-unload-test.i0yIFT/session.log. Nothing installed.

## 2026-09-11 — Plain-card local release uses presentation origins

Capture the held card's displayed rectangle before committing local order and
resetting the contact. Reuse the existing 280 ms OutCubic Card Line transition;
do not interpolate contact motion or write native geometry. Cancellation never
starts settling. Current scope requires an unstacked line with no armed stack
target or launcher guest. Fan translation/rotation need pose-aware capture first,
so stack and external release remain unchanged rather than claiming continuity.
Read-only carry diagnostics expose the base line rectangle and animation status.
Mouse/touch tests verify held-to-home continuity and completion; production
transfer/restoration/unload and safety-control checks pass. Unload during settling
then reload leaves no held input or animation. Evidence:
`/tmp/kadunce-unload-test.BF2Yon/session.log`. Nothing installed.

## 2026-09-11 — Receiver-owned monitor footprints

DesktopStage now shares its card-admission solve with the read-only footprint
query. Ordinary drops use the reserved rectangle; Bento uses the admitted pane
from the same pixel-layout conversion as placement. Infeasible layouts have no
outline. Tablet arrivals remain with CardStage and do not borrow a fake pane.
Reservation validation includes current minimum sizes and native source bounds.

Effect owns a rounded, color-managed outline/faint fill without a window, input
grab, native outline API or geometry writes. Motion calculates the footprint;
paint checks reservation validity and clips to the destination work area and
bottom exclusion. Shader unavailable means no outline, not a failed drop.
Projection follows KDE's viewport conventions, checked against primary source:
https://raw.githubusercontent.com/KDE/kwin/master/src/core/renderviewport.cpp
and the ShowPaint effect. Physical shader appearance remains unverified.

Both runtime sessions compare footprint to actual post-drop geometry for mouse
and touch open/edge/withdrawn-edge transfers. Evidence:
`/tmp/kadunce-unload-test.1yv3iR/session.log` (Card Line) and
`/tmp/kadunce-unload-test.ylDrh8/session.log` (native Bento).
Full restoration/transfer/unload, 14 CTests and independent switch checks pass.
Installed and repair hashes remain unchanged. Nothing installed or pushed.

## 2026-09-11 — Committed monitor settling has no input ownership

Native and Card Line transfer callbacks capture the held rectangle and receiver
footprint by value, commit through the existing transaction, then optionally
start 220 ms OutCubic presentation. Logical success alone is insufficient: KWin's
requested output and geometry must match. An initial reported-frame gate skipped
Card Line animation because clients acknowledge size later; requested placement
is the proper native-intent boundary. There is no configure wait or retry.

Rendering reuses the carry transform/output clipping and reads the current client
buffer. It neither changes geometry nor keeps the drag alive. Fresh pointer/touch
presses retire presentation without consuming the new stream; cancellation,
manual state/native move, output changes, closure, focus elsewhere and unload
also retire it. Effect activity/scanout blocking includes the finite presentation
lifetime, even when the source stage becomes empty.

Tests: `/tmp/kadunce-unload-test.RpqMV9/session.log` covers Card Line monitor
settling and fresh pointer/touch cancellation. `/tmp/kadunce-unload-test.2CwVgg/session.log`
covers native drops with input released, plus unload during committed settling.
Restoration/transfer suite: `/tmp/kadunce-bento-candidate.yK81kD`.
14 CTests, source/switch checks pass. Hardware pixels/frame pacing remain untested.
Tablet-arrival and stack-fan transitions remain separate gaps. Nothing installed.

## 2026-09-11 — Stack release captures final poses, not base slots

Moved the existing fan pose calculation from Effect into a shared read-only
CardStageController method. Rendering and release capture use it unchanged.
Capture includes held-card displacement and neighbor fan translation/rotation;
full-pose interpolation runs after destination fan geometry, bypassing base slot
interpolation. This avoids double transforms. Visible departures fade in place;
new visible members fade in. Ordinary transition capture can preserve an ongoing
pose transition. Input face geometry/pickup follows the presented rectangle.

Same 280 ms timing, cover rendering, membership/order commands and dwell policy.
Cancellation starts no animation; guests remain separate. The source guard now
checks the shared pose boundary instead of requiring its old code location.
Tests cover stack join/return rotation, left/right insertion and unload during
stack settling: `/tmp/kadunce-unload-test.hCHwQN/session.log`.
14 CTests, source guards, switch checks and full transfer/restoration/unload pass
(`/tmp/kadunce-bento-candidate.XRoO4F`). No installation or repair promotion.
Hardware fan appearance and large-stack fade quality remain physically unverified.

## 2026-09-11 — Tablet Card Line receives the released face

Native carry supplies an optional copied rectangle before guest/source cleanup.
After committed tablet admission, CardStageController uses it as the incoming
card's origin in its existing center/expand transition. No second animation,
placement write, input hold or monitor-layout inference. Only an existing Card
Line arrival sequence consumes it; Active/inactive arrivals remain unchanged.

Full-plugin pointer/touch open/edge departures and returns with a tablet resident
pass in the Virtual-0 fixture: `/tmp/kadunce-unload-test.S9yCZf/session.log`.
Tests verify initial carried-width continuity, free input and final Active state.
The fixture creates the resident before any layout owns it; it does not manually
move a Bento-owned resident behind the controller's back. Full candidate
transfer/restoration/unload also passes (`/tmp/kadunce-bento-candidate.kUFQ1C`).
14 CTests, source and independent switch checks pass. No install/push/repair change;
physical appearance and Active/inactive tablet feedback remain unverified.

## 2026-09-11 — Same-output Bento pane exchange

Native carry on its source Bento output now reserves the visible pane under the
contact, excluding the panel/bottom band. A local value plan exchanges the two
occupants and their snapshot ordering without changing pane rectangles or restore
record contents. Both clients must fit their destination minimum sizes. Existing
reservation generation, native geometry and resident checks remain in force.
Preview and commit use the same plan; local placement cannot call cross-output
departure/admission. Returning home consumes the reservation without native writes.
Exchange publishes before guarded session application; carried-face settling reuses
the monitor path. The other pane's animation remains follow-up work.

This is deliberately pane exchange, not stack insertion, arbitrary layout reshaping,
or a new persistence policy. Full-plugin mouse/touch exchange, return after previewing
another output, dock exclusion and original-window restoration pass:
`/tmp/kadunce-unload-test.iZm54p/session.log`. Cross-output runtime tests also pass
(`/tmp/kadunce-unload-test.hHCMuc/session.log`), along with candidate transfer,
restoration/unload (`/tmp/kadunce-bento-candidate.MMFIUe`), 14 CTests, source guards
and independent kill-switch checks. Installed/repair hashes unchanged; no install
or push. Physical feel remains untested.

## 2026-09-11 — Shared stack browsing preserves fan poses

The existing pageStack action serves touch, mouse and wheel. It now captures full
fan poses before changing selection, reusing the 280 ms release transition and
preserving interrupted presentation. It cancels pending arrival expansion first;
explicit navigation wins. No gesture thresholds, hold delays or native placement
changed. Delayed insertion steps now independently check the live candidate and
workspace revision rather than relying only on router/final-commit validation.

Full-plugin interrupted touch-to-mouse browsing preserves selection round-trip and
stack order without detachment; insertion-end, rollback and unload checks still
pass: `/tmp/kadunce-unload-test.uamDiP/session.log`. 14 CTests, source guards and
independent safety checks pass. Installed and repair hashes unchanged. Semantic
insertion unification and physical motion acceptance remain open; no install/push.

## 2026-09-11 — Stack insertion commits the preview's prepared order

CardWorkspaceState prepares stack insertion from an identity-bearing destination
and explicit slot. The model is copied during preparation; live membership,
selection and detached rollback remain untouched. Controller preview admission and
slot stepping replace the prepared plan; release commits it only after the existing
live-target guard. Owner/revision checking prevents stale, foreign and replayed
publication. Cancel/new-grab/reset clears the plan. Existing dwell/gesture timings,
rendering indices and return behavior remain unchanged.

Headless tests cover beginning/middle/end insertion, preparation purity, selected
identity, foreign/replayed commits and cancellation restoring exact order. Full
plugin mouse/touch insertion, browsing, departure disarm and unload pass:
`/tmp/kadunce-unload-test.U8l5jW/session.log`. 14 CTests, source guards and independent
safety checks pass. Installed and repair hashes unchanged. No installation/push.
Native carries still do not produce stack slots; this is the shared commit boundary,
not a claim that all input routes or physical motion acceptance are finished.

## 2026-09-11 — Fresh-source integration gate and explicit finish line

Added tests/verify-integrated-carry.sh: capture source/tests and hashes, build fresh
production plus a disposable tablet fixture using a checked-in one-line patch,
then run all four full-plugin runtime suites, native tests, restoration/unload,
source guards and independent safety checks. It stops on failure, never installs,
and only reads live control registration. No reliance on manually refreshed /tmp
fixture binaries. Full gate passes at `/tmp/kadunce-integrated-carry.GESYbS`.

Release audit distinguishes ordinary desktop entry from card-source transfers:
Effect stages only prepared Active/Bento sources; plain desktop clients fall back
to legacy native movement. Thus same-output desktop-edge Bento activation is not
covered by passing card-to-edge tests. Native tablet arrival also does not select
a stack slot. INTEGRATION-RELEASE-GATE.md names these missing contracts plus motion
and physical acceptance, without treating historical extraction tasks as new scope.
No production behavior changed in this gate/audit turn; installed/repair unchanged.

## 2026-09-11 — Ordinary desktop entry and opt-in test candidate

Eligible monitor windows without a Bento session now produce a typed ordinary
PreparedCarrySource with controller identity, generation, output bounds and native
pickup restore record. Existing contact proof/cancellation owns adoption; native
resize and unsupported sources remain native. This is not fake Bento membership.
The existing receiver planner handles local/cross-monitor edge creation and open
space; admission preserves the ordinary pickup record instead of the temporary
carried rectangle. Tablet admission uses the existing guarded host acceptance.
No additional per-frame geometry writes, new gesture thresholds or source-layout
removal are introduced. Panel/bottom exclusion remains intact.

Full fresh-source gate passes: `/tmp/kadunce-integrated-carry.nxEOpI`; added ordinary
mouse/touch local/cross-monitor edge, withdrawn preview, dock and held-unload tests
verify recruited-window and pickup restoration. Additional ordinary/Bento-to-tablet
return test passes at `/tmp/kadunce-unload-test.foQquH`. Private tests explicitly
use software rendering and disable Qt reconnect after an initial graphics-startup
timeout with no assertions; no physical performance claim follows from them.

User wants physical testing tonight. Prepared a separately hash-pinned opt-in
installer (`../install-kadunce-carry-test.sh`) and durable source/binary/accepted
rollback bundle (`../work/kadunce-carry-test-20260911`). Read-only --check passes;
no install/reload/push/repair promotion was performed. The previous installer is
unchanged. Candidate and trusted baseline identities are in CURRENT_STATE.
This is a test boundary, not completion: native-to-stack atomic admission and
remaining neighbor/tablet presentation still need implementation and physical
acceptance. CARRY-TEST-TONIGHT.md gives the short couch pass and parked monitor pass.

## 2026-09-12 — Bento entry without display-type restrictions

User's first physical pass accepts the touch foundation, not stack timing or
full completion. Tablet edge entry was explicitly excluded and Ctrl+B rejected
the tablet while an external output existed. Remove that display restriction.
Local tablet edge entry reserves a feasible destination during motion, validates
on release and uses the existing full-stage Bento activation path, not a transfer
of one member into a competing owner. The transition releases only Card Stage,
preserving other outputs' Bento. Preserve the Active owner's native restore record
before release: current frame geometry can still be Active-sized immediately
after restoring. No sleep/retry, new timing or native per-frame writes.

Found a separate concrete external-entry defect: server-side decoration proof was
wrongly gated on an xdg-toplevel protocol, rejecting Xwayland. Remove that gate
only; application-request serial validation remains Wayland-specific. Snap bands
remain contact-based (12 logical pixels, bottom 48 excluded); no guessed geometry
threshold changes. Real private Xwayland titlebar tests cover mouse/touch edge
creation and restoration; an abstract socket fixture avoids modifying the host
X11 directory. Tablet tests cover shortcut destination, Active/Card Line edge,
withdrawal, lone-card size and exact restoration. Stack timing/bias stays separate.

Existing tablet Bento is also the receiver for incoming ordinary/Bento native
carries; it must not invoke Card Line admission merely because the output is a
tablet. Both return paths pass the private entry test. The initial combined test
stopped on fixture reuse: unload correctly restored the arriving window to its
monitor pickup. Fresh explicitly positioned clients remove that assumption.
Production source stayed unchanged. Final evidence and the opt-in candidate hash
are recorded in CURRENT_STATE; dated bundle keeps rollback to the user's first
positive touch build, without promoting trusted repair. Physical entry acceptance
remains pending.

## 2026-09-12 — Ordinary tablet entry and explicit Bento departure

Physical feedback confirms monitor entry and card/Bento movement, but ordinary
tablet windows were still excluded. Admit ordinary pickup on either output.
Found a concrete renderer lifecycle gap: isActive/blocksDirectScanout omitted the
carried face when there was no layout. Include it, and clear preview on teardown.
This explains a possible native-drag/render mismatch; physical blur acceptance is
still required, not proven by private compositor tests.

User chose bottom-edge detachment instead of ambiguous 10px gaps. A monitor Bento
card gets an explicit NativeDesktop reservation in the 24px strip above the dock
exclusion; show a normal-window outline and Return to desktop label. Withdrawn
preview cannot commit. Same-output departure bypasses layout admission, prepares
source removal by value, publishes once, then uses guarded placement/reflow.
Remaining cards stay in Bento; lone departure ends it. An ordinary window moved
locally stays native even with Bento present; cross-display arrival still joins
existing Bento and explicit side/top edges re-enter it. No blacklist, magic delay,
or permanent disable is needed. Existing touch/stack timing is unchanged.

Focused pointer/touch exits, withdrawal, dock exclusion, multi-card survivor and
post-release renderer/input lifecycle pass at /tmp/kadunce-unload-test.WFSpho.
The first snapshot gate /tmp/kadunce-integrated-carry.wMpCCa passed all runtime
routes but its source guard forbids GLTexture in card controllers. The text-only
HUD is now isolated in DesktopExitLabel, which takes no window or card image;
that guard stays unchanged. Final snapshot gate /tmp/kadunce-integrated-carry.Sitj0g
passed: 14 CTests, nine real-plugin routes, restoration/unload, source and safety
control gates. Opt-in installer rolls back to the physically accepted entry build.
No live install/reload, repair promotion or push was performed.

## September 12 resumed: distinguish native ownership from wallpaper blur

At the user's reproduced blurred desktop, the live effect exported no stage,
carry, preview, settle or input owner and rendererActive=false. KWin activeEffects
contained shapecorners only, not Overview. The installed smart-video-wallpaper
configuration has BlurMode=1 and its main.qml implements blur when a visible
window is active. No wallpaper setting was modified. Do not use background blur
as a proxy for retained card membership.

Ordinary native movement is still adopted too early; this is not fixed by hiding
an outline. Before widening native proof, expose bounded lifecycle evidence for
app-drawn versus server-decorated titlebars. nativeMoveTrace retains 48 events in
memory only, without titles, coordinates, persistent logs or per-frame events.
It records start, source staging, pointer/touch proof, adoption/rejection,
destination transitions, commit result and cleanup. Existing proof rules remain.

The user's revised departure target is now implemented in uninstalled source:
bottom 24 logical pixels of the actual monitor output, only for an already-owned
local Bento card. Panel geometry does not veto that carry's exit. The preview's
normal-window bounds remain inside the work area. Ordinary dock input is not
changed. Existing tests that expected no exit at y=780 on an 800-high output were
outdated; their non-edge dock test now uses y=760, and the explicit exit test uses
y=799. This does not establish a physical dock/auto-hide/backend pass.

## September 12: native touch selection is move-lifetime-bound

A's K1 takeover reproduced a KWin defect independently of Kadunce: after native
finish/cancel while touch is held, MoveResizeFilter's saved ID can suppress a
subsequent differently numbered contact. Fix this at the native state owner,
not by changing Kadunce's input priority or completing a stuck move after release.

The private KWin6.7.5 patch keys touch selection by weak Window identity plus
the existing interactiveMoveResizeCount. Motion and up validate that lifetime;
the existing native movement/release code remains authoritative. No new timer,
signal callback, input replay, geometry controller or Kadunce-specific KWin API.
Kadunce production source remains the accepted checkpoint. Six new native cases
fail before and pass after; the selected native suite has35 passes, and all16
Kadunce runtime routes pass (see CURRENT_STATE for the interrupted/resumed gate).

This validates the native correction, not system packaging or physical K1
acceptance. Preserve the installed distro recipe/rollback, obtain installation
approval, and test the original pointer/touch report before advancing to K2.
The artifact and exact evidence are in patches/kwin/README.md.
# Dock/launch batch resource boundary — September 12, 2026

## September13 held-size follow-up

Retained Active imagery: use a renderer-owned bounded CrossFadeEffect adapter,
capture before explicit toggle restores native state, never retain native Active
geometry just for thumbnail appearance. Preserve aspect and rigid rotation.
Eviction/unsupported capture falls back to live preview. Non-GL smoke exposed
an unguarded helper cleanup crash; prohibit capture without a valid GL context.
J must verify actual GPU output; the available virtual backend only proves safe
fallback and lifecycle. No promotion based on that fallback-only test.

Rigid-fan follow-up: common baseline and shallow fanPose for browsing/insertion;
remove controller extra rotation so paint and reserved envelope agree. Preserve
ordered seam/commit logic. Envelope computation covers only visible5 positions.
Outline-only preview replaces solid fill per J. Retained Active imagery is a
separate renderer lifetime task, not permission to delay native restoration.

Follow-up approved: travel begins beyond the resting center card shoulders,
500ms dwell; physical edge remains300ms initial/350ms repeat. Inward contact
stops travel. Armed insertion remains a slot interaction until explicit edge exit.
Use resting geometry while held to avoid a feedback loop from the moving card.
Solid black placeholder and prompt OutCubic slot settling replace translucent
fill and delayed-start InQuart; do not change canonical stack order or ownership.
These are product timing choices informed by platform patterns, not claimed
universal Android/iPad constants. Frame-time improvement remains unproven.

J explicitly requested the proven44% held size merged into current geometry,
without waiting for remaining performance investigation. Extract only anchored
size interpolation from the earlier experiment; preserve current gesture and
ordering policy. Capture the displayed size at release and transfer. Correct the
new backing draw's scissor-state leak and add bounded contact traces; neither is
proof that all reported frame/input delays are resolved. Physical acceptance
remains J's, and the installed geometry predecessor is the rollback.

J requested dock safety, then explicitly added visible new-app Bento admission
to the same batch before freeze/push. MVP-RELEASE-SCOPE.md supersedes the old
mandatory K0–K6 progression. Cover tablet-only as a default product expectation;
use focused checks plus safety and J's physical acceptance, not another broad gate.
Bottom landing remains one translation through existing ownership paths. New-app
admission reuses value-planned layout and original restore snapshots; no unconditional
parking. Rejected minimum-size admission stays visible/native. No architecture
rewrite, general launcher framework or unrelated feature is authorized here.
# September12 evening: automatic edge ownership and independent idle gesture

J explicitly supersedes the ordinary-window native-edge exception: while Kadunce
is enabled, automatic top/side/corner placement belongs to Bento. Open-space moves
remain ordinary. Use KWin's existing in-memory electric-border tiling/maximize
options with reload-aware restoration on unload; do not rewrite user settings or
undo native geometry after a snap. Explicit Shift-custom tiling remains separate.
This does not establish the cause of Konsole's reported misalignment.

Dock safety, held-card bottom departure, and idle swipe-to-Card-Line have distinct
lifetimes. Idle reach includes dock depth plus the36px band above it, with existing
intent/cancellation thresholds. No filter-priority or held-drop ownership change.
Preserve cross-output fixes and the new-app candidate; physical acceptance gates
freeze/push. The installed6475 candidate is not an accepted release.
## September 13 — Active retires temporary stack elevation

Every successful enterActive path now synchronizes elevation after publishing
Active, not only the shortcut caller. Timed arrivals and externally activated
stack members must become ordinary native-layer windows so Plasma applet popups
remain above them. Do not compensate by misclassifying Temperance as an OSD or
making every window keep-above. Focused source/build and safety checks pass.
J accepted the installed QoL batch with a full pass and authorized publication.

## September 13 — Row paging motion is presentation, not navigation ownership

Connect ordinary and held Card Line paging to one220ms retargetable full-pose
transition. Model selection changes immediately, never waits for animation.
Departing members may paint until completion but retain model/input invisibility.
Wrapped shoulders leave and enter at opposite edges, not through the center.
Capture current rendered poses on interruption; do not queue navigation. The
held card stays contact-anchored and release clears detached-row interpolation.
Reuse live redirected surfaces and the accepted post-paint continuation latch;
do not introduce snapshots, preparation gates or native geometry mutations.
This candidate is deliberately separate from stack timing and later animation
polish. Physical acceptance is pending; accepted main/installed binary unchanged.

## September 13 — Bounded live neighbor preparation experiment

J requests opaque full-window arrival instead of additional fading. Reuse KWin
OffscreenEffect, not retained snapshots: its maybeRender runs before final clip,
so a qualified base draw with empty Region populates the live texture without
screen output. Invoke only inside our draw callback, where downstream draw-chain
iteration is valid. Prepare next hidden selected face on either side, at most
one per frame,32MiB each. Preserve output fences and input/model visibility.
Retire hidden preparations outside the neighborhood or Card Line lifetime.
No timer, native move, all-app sweep or readiness wait; two finite continuation
frames serve an idle neighborhood. Fast input may outrun this best effort.
Normal live dirty updates remain KWin-owned. Preparation has GPU cost and is
not claimed hitch-free before J's test. Rollback is the passable59f0 motion build.

## September 13 — Coordinate row travel after preparation pass

J accepts8b766377 preparation as V GOOD; requests removal of neighbors apparently
touching the center during motion. Anchor travel to the arriving center's current
captured position. Incoming origins/departing destinations reuse that displacement.
Circular outgoing/incoming representations share the same220ms easing progress,
switching when the outgoing rectangle clears the output, not on an independent
half-clock. Preserve captured interruptions and existing fan endpoint geometry;
no dwell, ordering, offscreen preparation or native ownership changes. Plain-card
gutter and shared-wrap displacement tests pass both directions; real mixed-stack
geometry and rapid reversals remain J's physical gate. Rollback preserves8b766377.

J subsequently reports “passed, freeze and push.” The installed a422853a binary
is the accepted row-motion freeze; see FREEZE-20260913-ROW-MOTION.md.
