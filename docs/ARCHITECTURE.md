# Architecture

## Authority boundary

KWin owns real windows: lifecycle, focus, native move/resize, output assignment,
virtual desktops, and recovery. Kadunce owns the spatial interaction model and the
presentation derived from it. Kadunce must not create a competing window identity,
desktop backend, or persistent geometry authority.

Spread is compositor space. It presents live windows as cards without storing or
applying off-screen client coordinates. Bento is an output-local desktop layout and
therefore uses real, reversible client geometry. The order a physical ownership
change follows is `Transfer transaction` below.

## Runtime owners

The native plugin has four principal owners:

- `WorkspaceInputRouter` recognizes pointer and touch gestures, owns contact
  transactions and timers, and dispatches semantic commands. It cannot edit models
  or compositor state directly.
- `CardStageController` owns Active, Spread, membership, stacks, selection,
  arrival, and reversible card transactions through `CardStageHost`.
- `DesktopStageController` owns output-local Bento sessions, admission, layout,
  divider resize, transfer acceptance, placement observation, and restoration.
- `Effect` owns KWin registration and lifecycle, output discovery, controller
  coordination, context publication, shortcuts, and card rendering.

`CardWorkspaceState<Handle>` is the mutable authority for Spread membership and
its `SpreadModel`. `SpreadModel`, `SpreadLayout`, `BentoLayout`, and the value
transfer planners are headlessly testable domain primitives. Rendering consumes
controller views and never mutates a controller.

The remaining renderer inside `Effect` may be extracted behind a typed boundary,
but extraction must not create a second model, event bus, or persistence service.

## Presentation model

Active presents one interactive card. Spread presents an ordered center and
neighbors, including visibly ordered stacks. Both are output-local views of logical
membership. Bento presents simultaneously interactive real windows. Which windows
a Bento session owns, and what becomes of the rest, is `CARD-LIFECYCLE.md`; it is
the canonical state and transition contract and this document does not restate it.

Live card content uses KWin's `OffscreenEffect` texture path with contain scaling,
the established fan rotation/aperture, and black backing where aspect ratios differ.
Proportional margins are accepted product behavior. There is no retained snapshot
cache or capture-readiness state machine. Motion may retain a departing face only as
ephemeral paint state; it cannot change membership or input visibility.

Spread motion captures the currently presented pose, so interruption and
retargeting continue from what the user sees. Model and input updates do not wait for
animation. Paint scheduling requests only the frames needed to reach an endpoint.

## Restore and ownership records

Every native geometry mutation has one authoritative restore record owned by the
controller that owns the physical layout. Card membership keeps its restore record
when presentation changes. Spread or selection changes do not restore the native
window. Explicit release, unload, or an accepted transfer consumes the record.

Card Stage release clears its live registry. Identity is stable by window handle or
UUID while the effect is loaded; array indices are compatibility data, not durable
identity. Kadunce does not currently promise persistence across a complete effect
unload.

Constrained new windows use the Bento admission solver or prepared individual-card
ownership. Presentation changes never release a managed window to the native
desktop; `CARD-LIFECYCLE.md` defines what each presentation owns.

## Transfer transaction

All cross-owner transfers follow one order:

1. Input identifies a semantic destination.
2. The source controller creates a reversible, revision-bound transfer value.
3. The destination validates current output, membership, minimum sizes, visibility,
   and its own revision, then prepares admission on a value copy.
4. The source removes membership only after destination acceptance.
5. Controllers publish state before guarded native placement.
6. Rejection or cancellation preserves exact source membership, stack order,
   selection, and restore state.

Existing Bento has priority for an incoming monitor transfer. An invalid existing
layout rejects instead of falling through to ordinary desktop placement. A deliberate
new-layout edge destination prepares the complete eligible batch. Open monitor space
without Bento remains ordinary native desktop space.

Committed monitor placement may retain a short compositor-only settle while KWin's
requested output and geometry still match the reservation. The input route is already
released. The settle cannot issue geometry writes and ends on new input, topology or
manual state change, divergence, cancellation, or teardown.

Native-to-stack admission is still incomplete as one atomic destination transaction;
see `CURRENT_STATE.md`.

## Input ownership

Each input stream has one owner until release or explicit cancellation. Panel input,
application input, Tette guest input, Kadunce card input, and native KWin move/resize
must not steal one another's releases. Touch identities and pointer buttons drain on
their original route even when their actions are canceled.

Ordinary client contact remains native until a deliberate reserved edge or crossing
proves Kadunce intent and KWin's native interaction is canceled once. Native source
proof is bound to the exact window, initiating contact, move lifetime, generation,
and topology. Failed proof stays native.

Automatic electric-border tiling and maximize behavior are suppressed in memory
while Kadunce is active and restored on unload. Explicit Shift custom tiling and
keyboard/manual window operations remain KWin-owned.

The detailed routing table is `INPUT-OWNERSHIP.md`.

## Output and dock rules

Spread and Bento are presentations, not display-type restrictions. Bento may run
on the tablet and monitor. Sessions are output-local; changing or releasing one
output must not release another output's session.

Dock safety uses the actual work area plus visible bottom dock frames. The bottom
edge remains an intentional destination strip, while ordinary panel input remains
untouched. Native releases get one bounded position correction with 10 px clearance;
oversized windows retain a reachable title bar instead of being resized.

## Integration boundaries

Tettegouche reads Kadunce through the versioned, read-only context endpoint in
`TETTEGOUCHE-CONTEXT.md`. Companion applications cannot join Kadunce's card registry
or mutate compositor ownership. The guest-card protocol is opt-in, versioned, and
separate from ordinary context publication.

Table must use KWin's existing virtual-desktop authority. Shuffle Keyboard must use
the system input-method stack for keymaps, locale, and application delivery. Their
product contracts do not grant either component a second window or input backend.

## Safety and teardown

The out-of-process tray controller is the persistent recovery surface. Disabling
requires KWin to confirm safe effect unload; failure restores the enabled setting.
On unload, input routes are canceled and destroyed while controllers remain alive,
then every managed client is restored before the effect disappears.

Private compositor tests must use isolated runtime directories, display sockets,
and D-Bus. They do not prove live-session safety or physical behavior. Follow
`TEST-ENVIRONMENT-PROCEDURE.md` and `INTEGRATION-RELEASE-GATE.md`.

## Invariants

1. A client has one physical KWin output and one owning native restore record.
2. Spread never mutates real client geometry.
3. Destination acceptance precedes source removal.
4. Rendering reads state and never edits controller models.
5. Input routing emits semantic commands and never edits models directly.
6. Other outputs retain their independent sessions.
7. Companion applications cannot enter compositor ownership through context APIs.
8. Disable restores clients before unloading the effect.
9. Safety-control failure blocks candidate promotion.
10. Ownership and presentation transitions conform to `CARD-LIFECYCLE.md`.
