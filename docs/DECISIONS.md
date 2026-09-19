# Durable decisions

This ledger records current architectural and product decisions. It is grouped by
subsystem. Superseded discussion and candidate evidence are preserved in
`docs/archive/` and Git history.

`CARD-LIFECYCLE.md` is the canonical product authority for card ownership,
presentation states, transitions, release, and Shuffle navigation. This ledger
records the architectural rationale that implements that contract.

## Product and authority

### KDE remains authoritative

KWin owns real windows, focus, outputs, native movement, virtual desktops, and
recovery. Kadunce adds a spatial interaction and presentation layer. It must not
create parallel window identity, geometry, virtual-desktop, or input-method systems.

### Spatial levels have distinct jobs

Active means one window. Spread and Bento mean a set of windows. Table means a
set of existing KDE virtual desktops. Displays and virtual desktops are independent
dimensions and require an explicit integration design.

### Release is a command

Release returns managed clients to safe ordinary Plasma windows. It is not a saved
layout mode. Complete unload clears live card membership after restoration.

## Card model and ownership

### Membership has one mutable owner

`CardWorkspaceState` owns membership and `SpreadModel`. Controllers expose typed,
read-only views. Indices are compatibility values; window identity is stable by
handle or UUID while the effect is loaded.

### Restore records follow ownership

Every native geometry mutation has one controller-owned restore record. Presentation
changes do not consume it. An accepted transfer moves the authoritative record;
release or unload applies it. Observed native geometry after takeover is not a
substitute for the pre-presentation restore record.

### Bento owns only the visible pane combination

`CARD-LIFECYCLE.md` states the rule and its transitions. The decision is to scope
ownership to what is visible rather than to a remembered group: a window the user
cannot see is a window they will look for somewhere else, and a retained
association would have to be reconciled on every minimize, displace and overflow.
Scoping to visible panes removes that reconciliation instead of automating it.

### Admission is value-first and idempotent

New-window and transfer admission is prepared on value copies, checked against
identity and revision, and published once. Failure leaves the client native and does
not partially mutate a session.

## Transfer and native placement

### Destination acceptance precedes source removal

`ARCHITECTURE.md` § Transfer transaction states the order. The decision is that a
transfer is a destination-led transaction rather than a source-led move, because
only the destination can evaluate output, minimum size and its own revision. A
source-led move would have to undo itself after a rejection, and an undo that
runs after native geometry has changed is not reliably reversible.

### Handoff order is carried by a shared sequence, not by source order

Publication before native placement, restore capture between the accepted output
placement and Kadunce's own geometry, and projection retirement before a resume
commits are expressed as duck-typed sequences in `OwnershipHandoff.h`. A caller
supplies the steps and cannot reorder them. Checks therefore assert the observable
order and what a refusal leaves undone, never where a call sits in a file, so a
controller may be restructured without weakening the invariant.

### Existing Bento has destination priority

An incoming monitor card first targets an existing Bento session. Invalid existing
admission rejects; it does not silently fall through to ordinary desktop placement.
A deliberate new-layout edge target prepares the eligible destination batch.

### Settling is presentation only

After committed native placement, a short carried-face settle may bridge delayed
client acknowledgement. It cannot write geometry, retain input, or defer the commit,
and ends when the native request diverges or the environment changes.

### Minimum sizes are hard constraints

Bento never forces a client below its useful minimum. It may use the small pane when
the minimum fits or park overflow when it does not.

## Input and gesture ownership

### One stream has one owner

Pointer buttons and touch contacts remain with their initiating route until release
or explicit cancellation. Cancellation removes actions and timers while retaining
enough identity to drain the stream safely. Foreign releases cannot activate cards.

### Native interaction remains native until proven

Ordinary move/resize stays with KWin until a deliberate Kadunce destination and exact
contact proof succeed. Proof is bound to the source window and one native move
lifetime. Unsupported, ambiguous, resize, keyboard, or stale requests stay native.

### Edge policy is temporary and reversible

Kadunce suppresses automatic electric-border tiling/maximize behavior in memory
while enabled and restores the latest preferences on unload. Explicit Shift custom
tiling and keyboard/manual operations remain KWin behavior.

### Dock input and dock clearance are separate

Panel controls retain their input. Destination recognition may use the physical
bottom edge, while landing uses work-area and visible-dock geometry plus clearance.

## Rendering and motion

### Live textures are the card source

Cards render through KWin `OffscreenEffect` live textures with contain scaling,
established fan geometry, and black backing. Aspect-ratio margins are accepted.
Kadunce does not maintain a retained snapshot store to hide them.

### Bento projects as one reversible group card

A Bento composition entering Spread occupies one ordinary logical card slot.
Its pane-visible live surfaces map from their current native frames into one
centered proportional view of the authoritative KWin work area, preserving outer
gutters, pane gaps, pane geometry, and dock clearance. A single 22%-opacity black
backdrop fills that mapped work area without capturing wallpaper. The group cannot
fan, page members, accept a stack insertion, or retain windows outside its visible
pane combination.

Pane placement comes from the transferred normalized Bento rect, not later absolute
client geometry. Current expanded-versus-frame margins preserve live decorations and
shadows for transformation, while the authoritative stored frame clips pixels out of
pane gaps and outer gutters. Its rounded GPU aperture uses the same Spread radius
scaled through the workspace composite and physical output scale; a rounded region
is the shader-unavailable fallback. Successful exact resume clears projection
provenance, offscreen sources, and per-frame aperture state before native Bento can
repaint; rejection retains all projection presentation.

The transfer carries visible pane order and rects, lead, side metadata, stacking,
and authoritative restore records. Activating the group
validates current native geometry, commits the reverse ownership transfer, and
publishes the same Bento session without a solver or native geometry write.
Rejection leaves the group card and neighboring Spread ownership intact.
On success, non-group Spread neighbors remain owned and hidden; they do not
restore as ordinary desktop windows. Ordinary Spread rendering and layout remain
unchanged.

### Model truth does not wait for animation

Motion interpolates a captured visible pose, can be interrupted or retargeted, and
never delays input or model changes. Departing paint state has no input authority.

### Motion must follow platform accessibility

Custom durations should respect platform animation scaling and reduced-motion
preferences. This is a live gap, not permission for a broad renderer rewrite.

## Output and lifecycle

### Sessions are output-local

Kept as `ARCHITECTURE.md` invariant 6. Output-local sessions are what let the
tablet change presentation without disturbing an external display the user is
still reading; a single global session would make every tablet gesture a
multi-display event.

### Teardown order is part of correctness

On unload, Kadunce first cancels and destroys input routes, then restores managed
windows while controllers still exist, then unregisters the effect. No callback may
outlive its owner.

### Persistent membership is not promised across unload

The public context endpoint and live registry disappear with the effect. Do not add
a service solely to preserve an implementation extraction boundary.

## Safety and packaging

### The tray control is mandatory

`PRODUCT-CONTRACT.md` § System control states the behavior. The decision is that
recovery lives outside the compositor plugin, because the failure it exists for is
the plugin not loading. A control hosted by the thing it recovers cannot recover
it, which is why no effect test can stand in for this one.

### Private tests and live evidence remain distinct

Private KWin sessions use isolated display, runtime, and D-Bus state. Their success
does not establish live control availability, physical touch behavior, frame pacing,
or installed provenance.

### Native KWin patches are version-bound

The touch-lifetime correction binds a saved touch identity to one window and one
native move/resize lifetime. It must be reviewed and rebuilt against the exact KWin
package version; Kadunce's installer never replaces KWin or silently pins it.

## External contracts

### Context is read-only and versioned

Tettegouche consumes normalized workspace state through the documented D-Bus
contract. It cannot enter Kadunce membership or compositor ownership.

### Guest presentation is opt-in

Guest-card negotiation is a separate versioned protocol. Unsupported clients and
older Kadunce builds retain standalone behavior.

### Shared visual grammar preserves component identity

Suite-owned action chrome uses the pinned Lucide subset and Ghost White foreground.
Tette Dot, the animated Temperance Bell, Weather, provider identity, and Kadunce's
stacked-card tray mark remain protected custom work.

## Future product contracts

### Table is required and uses KDE virtual desktops

Table is the workspace level above Spread/Bento. It must manipulate existing KWin
virtual-desktop membership and preserve current display ownership. Feasibility,
gesture conflicts, and lifecycle behavior must be proven before implementation.

### Shuffle Keyboard is required and uses system input plumbing

Shuffle owns layout, resizing, editing gestures, and its keyboard/precision-surface
transition. KDE/KWin or another mature system input-method stack owns locale,
keymaps, and application delivery. A custom input engine is a last resort.
