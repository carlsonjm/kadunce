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

### A side snap admits one card; Bento needs a pair

A window dragged to the left or right edge becomes an individual Active card and
nothing more. Bento begins only when a second window is snapped to join it.

The rejected alternative let one side snap create the layout and fill the
remaining panes from other eligible windows. That made the solver, not the user,
decide membership, and every window it could not place became retained overflow
that `CARD-LIFECYCLE.md` §14 gives to the card stage but the implementation kept
inside the session. Requiring a deliberate pair removes that class of defect at
its source rather than reconciling it afterwards: a window is a pane only because
someone put it there.

A lone side-snapped window therefore presents as an ordinary Active card rather
than holding its requested half, because a one-window Bento state must not exist
at all.

### Overflow is deleted, not reconciled

A window Kadunce owns and cannot currently show is an individual card. There is
no fourth place for it to be.

`CARD-LIFECYCLE.md` §5 always said Bento does not own hidden overflow, but the
implementation kept a per-session overflow container anyway, which created a
state nothing could name: retained by a session, owned by nobody. The rejected
alternative was to keep the container and reconcile it — teach each transition to
maintain it correctly and report where it drifted. That preserves the state it
was meant to remove and grows with every new transition.

Removing the container removes the state. Bento never takes a window it cannot
show, so a full Bento displaces rather than parks, and displacement is one of the
six directed transitions. The violation rule that reported the state retires with
it; a rule still worth reporting would mean the container had moved rather than
gone.

### A layout is rearranged only by a deliberate gesture

A launching application may grow Bento into a free pane, but it never evicts one.
When the layout cannot grow, the new window becomes an individual Active card.

Auto-admission is what makes Bento seamless at a monitor, and removing it would
hide a window the user just opened behind a layout. Keeping it is safe because
growth-only admission produces a pane or a card and never a parked window, so it
cannot reintroduce the overflow state. Displacement stays with the deliberate
edge gesture, where the user chooses the side and can see which pane yields.

The rejected alternative scoped auto-admission to external displays. That would
have keyed product behavior to `isTabletOutput`, a name-prefix guess at hardware,
when the property that actually matters is whether the work area has room. A
tablet at its cap and a monitor with no free pane should behave the same way.

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

### Both stages own windows on the same display

Card Stage owns individual cards and Desktop Stage owns the display's Bento panes
at the same time, and neither releases the other's windows. The two were
previously alternate whole-display owners: starting Bento released every card to
Plasma, and resuming a projected group restored its Spread neighbours. That made
`CARD-LIFECYCLE.md` §6's "individual cards remain independently owned and hidden"
unimplementable, and it is why a deliberate pair could not leave the rest of the
display alone.

A third card presentation names what Card Stage is doing while the panes are on
screen. It is presentation, not ownership: the stage keeps its membership and its
restore records, hides every card it owns, and gives its paging gestures back to
the display. Ownership still has exactly three owners and six transitions.

### The Active card is remembered as presentation context

Which individual card is Active survives entering Spread, because §3's pairing
names it as the partner while Spread selection chooses only what is carried. It
is one window identity and nothing more: no parked snapshot, no second
membership, no fourth owner. It retires the moment that window stops being an
individual card, so a pane or a released window can never be offered as a
partner, and a card carried to the edge while it is itself Active has nothing
distinct to pair with.

### A prepared ticket carries intent, not a model

Preparation proves a membership, order and grouping delta against a throwaway
model and keeps only the delta; commit re-derives the result from the live one.
A ticket that instead carried a model copy would write selection, page offset,
stack face and neighbour side back into the state that issued it, discarding
whatever the user browsed to while it was held.

### Two revisions, because a ticket and a preview fear different changes

`revision()` counts every state command and keeps its meaning for preview and
carry-provenance owners. `ownershipRevision()` counts only membership, order and
grouping. Admission and removal bind to ownership, so a reservation survives the
user's own paging and selection while a membership change still voids it. Stack
insertion keeps the stricter binding because its delta names the selected source
card and a visible stack face: paging changes what that ticket means, not merely
when it was issued. Splitting the counter is safe only because tickets no longer
transport presentation; it was tried before that and rejected for converting a
conservative refusal into silent view corruption.

### Ownership is a value that can see all three owners

`CARD-LIFECYCLE.md` §1 defines three owners, but individual cards live in the
card stage, Bento panes in the desktop stage's per-output sessions, and Native
only as absence from both. No object could evaluate §14's first invariant, so
every transition maintained two containers by hand. `CardOwnership.h` reduces
both containers to identities and evaluates the invariant in one place.

It observes and never repairs. A violation it reports is a pre-existing defect
in the caller, not a reason to refuse the caller's work, so it logs a shape once
rather than failing a transition. Making it authoritative is a separate step.

### Ownership changes only by one of six directed transitions

`CARD-LIFECYCLE.md` §1 defines three owners, so there are exactly six directed
moves between them and no seventh. `CardOwnershipLedger` is the authority for
who owns a window and accepts only those six; a move to the owner a window
already has is refused rather than absorbed, so a caller cannot use the ledger
to paper over a container it failed to update. It reports where the stages'
containers disagree with it and never repairs them, because a disagreement is a
defect to fix rather than a difference to average away.

### A prepared type is not always an ownership transition

Of the five prepared types, only admission and removal change an owner, and
those name which of the six they perform. Stack insertion is grouping inside
card ownership, prepared drops carry destination geometry and intent, and a
prepared carry source is read-only provenance that removes no membership.
Expressing those three as ownership transitions would put placement and
provenance back inside ownership tickets, which is exactly what narrowing the
ticket payload removed.

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
