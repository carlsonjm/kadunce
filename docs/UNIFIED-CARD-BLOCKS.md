# Unified cards — next execution blocks

Approved direction: September 11, 2026. Card Line, stacks and Bento become
arrangements of cards, not competing interaction modes. This supersedes keeping
Bento outside the refactor. Work one bounded block at a time; no implicit
installation, publishing, global snapping change or trusted-repair refresh.

## Product contract

- A lifted card follows its initiating contact in two dimensions, preserving
  the grab anchor. Magnetism aligns destinations, never steers the user's hand.
- The carried card may cross displays and morph into a Bento placement. Passive
  tablet neighbors remain contained. Identity survives changes of arrangement.
- Membership/order is recoverable until destination acceptance. Invalid drop
  preserves order and returns focus to the card's original space.
- Stack insertion works from either side with the same logical order. Passing
  over a stack must not automatically insert. Targets must not demand precision.
- Kadunce owns admitted card-drag interpretation; KWin remains the native
  geometry executor. Unrelated native input and system controls remain usable.
- Safety can cancel any carry; release afterward is inert. Tette retains its
  guest contract and must not silently become an application card.

## U1 — Ownership and destination rules

Map native snapping/movement, card paging, stack timers, Tette, tray and transfer
paths. Define one drag owner and one prospective destination at a time:
None, explicit Card Line insertion, StackGap or LayoutSlot. Crossing an output
changes the destination search region, not source membership by itself.

Deliver a headless session contract: source identity/membership/restore token,
device/contact, source output, global grab anchor, current pose, preview target,
generation and outcome. Native-to-Kadunce adoption needs an explicit boundary;
do not globally disable native snapping or run both previews for the same drag.

**Status:** source inventory completed in UNIFIED-CARD-OWNERSHIP.md. Headless
CarrySession contract and pointer/touch tests implemented; no runtime wiring.
Tests cover duplicate release, stale callbacks, changed/closed source, changed
target, output loss and canceled acceptance. See CARRY-SESSION-CONTRACT.md.

## U2 — Reversible carry and placement

**Status:** prepared removal/admission are wired for Card Line→monitor and
Bento→tablet. Bento→Bento/native uses value-prepared departures. Placement is
guarded and observed once; native configure acknowledgement and hardware
acceptance remain open. MonitorDropIntent and CarrySession now represent explicit
new-layout-edge and native-desktop intent headlessly. First-layout creation,
runtime preview/revision providers and CarrySession input wiring remain pending.
See CARRY-SESSION-CONTRACT.md for exact test boundaries.

Implement the tested session and narrow destination adapters. Destination
accepts before source removal commits. Preserve stack/order/selection rollback;
a session token is not a second mutable registry. Both devices use the same
transaction. Define lifecycle across arrangement changes without a new daemon.

**Gate:** rejected geometry, window/output loss and Disable cannot strand,
duplicate or silently reorder a card. Commit once; cancellation restores once.
No new timings or renderer needed to establish this boundary.

## U3 — Free-carry presentation experiment

Compare current painting with OffscreenQuickScene in a private two-output
compositor, using the same model/pose. QuickSceneEffect remains a reference, not
an assumed replacement. Preserve the global grab anchor and per-output scaling;
allow only the carrier through the tablet fence. Active stays native.

**Gate:** no lift jump, no horizontal leash, no neighbor leakage, normal external
input and held-card tray Disable. Measure thumbnail content latency separately
from position latency. Keep existing painting if scene reuse does not justify
its integration costs. No production migration before evidence.

## U4 — Edge placement versus browsing

Monitor entry intent is now explicit: edge snap is the mouse alternative to
Ctrl+B, promoting that display's eligible ordinary windows and the arrival into
Bento. Prepare this batch before committing; do not adopt windows from other
displays or system/guest surfaces. One member uses Active presentation but keeps
Bento membership. Existing Bento accepts new arrivals; open-space drop without
Bento stays native. No automatic companion chooser.

Edge approach produces one valid layout preview, respecting work area and
minimum window sizes. Distinguish an outside display edge from an inter-display
seam. Invalid release returns to origin without reordering.

**Design checkpoint:** select a distinct way to reach offscreen stacks while
carrying. An exposed Card Line rail is a proposal, not approved UI. Also decide
any optional companion-picker UI is useful later. First entry no longer waits
for a companion: display-wide promotion/lone Active presentation is the rule.

**Gate:** no simultaneous KWin/Kadunce preview or ambiguous page-versus-snap
dwell. Every failed placement has an explicit return path.

## U5 — Ordered stack insertion

Use destination-stack geometry and visible insertion gaps rather than fixed
center-line zones. N members provide N+1 insertion positions. Approach side
changes presentation, not order. Generous hit targets and preview retention
absorb finger jitter without pulling the held card away from its contact.

**Gate:** insert before/after every member from both sides; reorder within the
same stack; remove/reinsert its last member; cancel after browsing; target closes
mid-drag. No conflict with layout placement or row browsing.

## U6 — Card Line ↔ Bento ↔ displays

Wire destination adapters so dropping can create/extend Bento without separate
mode entry. Picking a Bento card up reverses the journey. Test tablet→monitor,
monitor→tablet and monitor→monitor, mixed scale, minimum-size rejection,
fullscreen restoration and unplug during carry/commit. Keep one physical
window owner and a continuous carrier.

**Gate:** identity/order/restoration and Tette context remain coherent. Guest
disappearance cannot invalidate an unrelated held app. New Tette drag APIs need
a separate contract. Virtual tests first; monitor hardware testing stays parked
until docked. A virtual pass is not hardware acceptance.

## U7 — Motion and touch finish

After ownership is correct, implement continuous paging, settling, reversal and
regrab with measured position/velocity. Coordinate pair/triple geometry and guest
replacement through a shared pose snapshot. Separate immediate press feedback,
page intent and deliberate hold-to-reorder. Evaluate the Qt/KDE primitives in
KDE-REUSE-AUDIT.md; don't shorten the 300 ms hold to conceal ownership problems.

**Gate:** slow drag, quick flick, short reversal, repeated regrab, cancellation,
window admission/closure mid-settle, frame stalls and different refresh rates.
Bounded traces plus physical testing; no universal “iPad timing” claim or
unverified mandatory haptics. Repeat safety checks before candidate acceptance.

## Choices not yet approved

1. Failed-drop focus is provisionally interpreted as centered/focused Card Line,
   not automatic expansion to Active. Confirm before final return animation.
2. Carry-time browsing, edge/corner precedence and companion commitment remain
   design checkpoints, not reasons to postpone headless transaction tests.
3. Stack feedback/dwell is to be prototyped after geometric targeting.
4. Pickup from native/Bento titlebars versus client surfaces needs an explicit
   ownership boundary; don't swallow application content interaction.

Each block ends with evidence and a next bounded step. Update CURRENT_STATE,
preserve installed/repair provenance, and stop on ambiguity rather than adding
another timer. The roadmap does not authorize installation or publication.
