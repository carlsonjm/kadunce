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

There is one card stage and one layout stage per virtual desktop, each reaching
the effect through that desktop's `SessionHost`. Table adds three owners beside
them: `TableGesture` turns a stroke into an action, `TableLayout` places the rows
so hit tests and drawing agree, and `TablePresenter` draws `table/Table.qml` in
the compositor's frames through `OffscreenQuickScene`, taking no input. Table
creates, renames and removes desktops through KWin; Kadunce keeps only which
names are yours (`TABLE.md`).

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
controller that owns the physical layout. Explicit release, unload, or an accepted
transfer consumes the record. A presentation change never consumes it and never
returns a window to the native desktop; `CARD-LIFECYCLE.md` §14 owns both rules.

Card Stage release clears its live registry. Identity is stable by window handle or
UUID while the effect is loaded; array indices are compatibility data, not durable
identity. Nothing persists across unload (`CARD-LIFECYCLE.md` §13).

Constrained new windows use the Bento admission solver or prepared individual-card
ownership.

## Transfer transaction

All cross-owner transfers follow one order:

1. Input identifies a semantic destination and every window identity it names.
2. The source controller creates a reversible, revision-bound transfer value.
3. The destination validates current output, membership, minimum sizes, visibility,
   the identities the input named, and its own revision, then prepares admission on
   a value copy.
4. Every generation and topology check repeats at commit, and the source removes
   membership only after destination acceptance.
5. Controllers publish state and release the input route before guarded native
   placement, which only a real-geometry destination applies.
6. Rejection or cancellation preserves exact source membership, stack order,
   selection, and restore state.

After acceptance, an interruption belongs to the destination and cannot restore
stale source state.

Existing Bento has priority for an incoming transfer. An invalid existing layout
rejects instead of falling through to ordinary desktop placement. A same-output
pane exchange swaps identities and restore records through one prepared layout,
and a pane returned to its original slot consumes the drop without a native
placement.

A deliberate edge destination that means a pair prepares only the two windows
`CARD-LIFECYCLE.md` §3 names. Nothing else on the display is an input to the
solve, the preview, or the revalidation, and a plan with any other pane set is
rejected. A display the pairing rules do not reach prepares its eligible
resident batch plus the arrival. Open space without an existing Bento remains
ordinary native desktop space on a display that cannot own cards; on a display
that can, an arrival there is admitted to the card stage, publishing membership
before presentation cleanup, and may seed Spread's center-and-expand motion from
its released pose. A stack takes no arrival from the native desktop
(`CARD-LIFECYCLE.md` §9).

Committed monitor placement may retain a short compositor-only settle while KWin's
requested output and geometry still match the reservation. The input route is already
released. The settle cannot issue geometry writes and ends on new input, topology or
manual state change, divergence, cancellation, or teardown.

### Carry session

A carry is the transfer value a live move between Spread, Bento and the native
desktop creates. It identifies:

- the source window and controller;
- authoritative restore state captured before presentation ownership changes;
- the initiating input device or contact and the pickup pose;
- source membership, stack order, selection, and generation;
- the current topology and output generation;
- a semantic destination, every window identity it names, and any prepared
  destination plan.

A carry holds no mutable window model of its own. Presentation may derive a
carried pose from it but cannot commit membership or geometry, and the carried
face never becomes a second source of restore geometry. Each output clips its own
paint route.

- One carry has one outcome and one source restore record, and retires exactly
  once, leaving no callback behind.
- Preparation never invokes a mutating shortcut path and recruits no unrelated
  client, panel, other output, or companion guest.
- Commit admits the exact identities the carry named and revalidated; none is
  re-read at release.
- Native geometry is never written repeatedly during motion.
- Paint and preview cannot make acceptance decisions.

## Input ownership

Each input stream has one owner until release or explicit cancellation. Panel input,
application input, Search's guest input, Kadunce card input, and native KWin move/resize
must not steal one another's releases. Routing never infers ownership from paint
state.

| Sequence | Owner until termination |
| --- | --- |
| Native desktop move/resize | KWin, unless exact Kadunce takeover proof succeeds |
| Pointer press on a Plasma panel | Plasma through release |
| Contact inside a companion guest | Guest through release |
| Contact outside an open guest on the card display | Kadunce; stationary release may dismiss, movement cancels dismissal |
| Contact on Spread or Active chrome | Kadunce through the semantic transaction |
| Provisional bottom-edge touch | Client until deliberate upward intent and successful native cancellation |
| Foreign or unmatched release | Original route; it cannot activate a card |
| Open Table | Kadunce: the keys grabbed and every touch Table's; a name being typed takes text focus |

Touch and pointer state are independent. Canceling one device cannot clear the
other device's hold, grab, timer, or forwarded ownership. Each timer and delayed
action carries the initiating device and generation, and its callback revalidates
both before acting. A canceled contact stays in a drain set until physical
release, so its release drains on the original route and cannot fall through to a
new one. Forwarded pointer ownership retains every held button; releasing one does
not end the route while another remains held.

Ordinary client contact remains native until a deliberate reserved edge or
crossing proves Kadunce intent and KWin's native interaction is canceled once.
Kadunce takes over a native move only while all of these still match:

- the exact weak window identity;
- the initiating pointer button or touch identity and its source surface;
- the same native move/resize lifetime;
- controller and workspace generation and output topology;
- an eligible move rather than a resize, keyboard, or unsupported request;
- a valid deliberate Kadunce destination.

Wayland application moves use the xdg-toplevel serial. Xwayland client-side moves
correlate `_NET_WM_MOVERESIZE` after KWin has accepted the request. The observer
neither consumes nor replays the client request. Failed or ambiguous proof stays
native.

Automatic electric-border tiling and maximize behavior, KDE's per-display desktop
switching, its desktop-change pop-up, and Overview's top-left corner unless you
gave it elsewhere are held in memory while Kadunce is active and given back on
unload. Explicit Shift custom tiling and
keyboard/manual window operations remain KWin-owned.

Source close, output loss, topology change, manual takeover, view release, effect
unload, or competing input cancels the affected actions and timers.

## Output and dock rules

Spread and Bento are presentations, not display-type restrictions. Bento may run
on the tablet and monitor. A deliberate edge destination takes its meaning from
the state and capability of the display it is released into, never from that
display's hardware identity. Sessions are per output and per desktop; changing or releasing one
output must not release another output's session.

Dock safety uses the actual work area plus visible bottom dock frames. The bottom
edge remains an intentional destination strip even where a dock occupies it; that
grants Kadunce no ordinary dock hit testing, and panel input stays with the panel.
Native releases get one bounded position correction with 10 px clearance;
oversized windows retain a reachable title bar instead of being resized.

## Integration boundaries

Tettegouche reads Kadunce through the versioned, read-only context endpoint in
`TETTEGOUCHE-CONTEXT.md`. Companion applications cannot join Kadunce's card registry
or mutate compositor ownership. The guest-card protocol is opt-in, versioned, and
separate from ordinary context publication. Placement requests (`REQUESTS.md`)
name an application and a point; Kadunce decides what the point means and places
the window through the same transitions a carried window takes.

Table uses KWin's virtual desktops for membership, switching and names. A preview
stops other desktops' windows painting rather than switching, and entering runs
as the full-screen effect so KDE's slide does not play. Shuffle Keyboard uses the
system input-method stack for keymaps, locale, and application delivery. Neither
gains a second window or input backend.

## Safety and teardown

The out-of-process tray controller is the persistent recovery surface, never a
second workspace authority. Its single checked switch loads the installed native
effect when enabled. Disabling first requires KWin to confirm a safe effect
unload and persists the disabled state; if KWin cannot confirm the release, the
control restores the enabled configuration rather than risk a half-disabled
workspace. Its Settings window holds preferences without adding preference state
to the compositor plugin. On unload, input
routes are canceled and destroyed while controllers remain alive, then every
managed client is restored, as `CARD-LIFECYCLE.md` §13 lists, before the effect
disappears. No timer, native observer, or callback survives its owner.

What each check proves, and the rules for private compositors, are
`TESTING.md`.

## Invariants

1. A client has one physical KWin output and one owning native restore record.
2. Spread never mutates real client geometry.
3. Destination acceptance precedes source removal.
4. Rendering reads state and never edits controller models.
5. Input routing emits semantic commands and never edits models directly.
6. Other outputs and desktops retain their independent sessions.
7. Companion applications cannot enter compositor ownership through context APIs.
8. Disable restores clients before unloading the effect.
9. Safety-control failure blocks candidate promotion.
10. Ownership and presentation transitions conform to `CARD-LIFECYCLE.md`.
