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

### A side snap admits one card; Bento needs two named windows

A side snap onto a display Kadunce does not yet own starts ownership without
starting a layout: the carried window becomes an individual Active card. Bento
begins only on a display Kadunce already owns, and then the snap names exactly
two windows, the carried one and one partner. No third window joins.

The rejected alternative let one side snap fill every remaining pane from
whatever other windows were eligible. That made the solver, not the user,
decide membership, and every window it could not place became retained overflow
that `CARD-LIFECYCLE.md` §14 gives to the card stage but the implementation kept
inside the session. Requiring a deliberate pair removes that class of defect at
its source rather than reconciling it afterwards: a window is a pane only because
a gesture named it — the window carried to the edge and the one partner that
gesture selects — never because a solver found room for it.

A side snap that pairs with nothing therefore presents its window as an ordinary
Active card rather than holding its requested half, because a one-window Bento
state must not exist at all.

### Spread order selects the partner; the gesture places it

A side snap that begins a layout pairs two named windows. When the carried window
is not the Active card — another individual card, or a window still on the native
desktop — the partner is the Active card. When the carried window is the Active
card itself, the partner is the nearest eligible card on the contacted side of it
in Spread order, passing over entries that are not eligible. The walk is cyclic,
because Spread is cyclic: paging runs off one side of the order and returns on
the other, so there is no first or last entry to stop at, and the walk visits
every other entry once. Where exactly one other card is eligible, both sides reach
it, and that is the one pair available; a second entry the walk passes over, an
ordinary stack or the Bento group, leaves no pair at all. If no eligible card is found,
nothing pairs and Active is unchanged.

Placement does not follow from the partner's position. The carried window takes
the edge it was released into and the partner takes the opposite side, whichever
side of the order it came from. The rejected alternative let the partner keep the
side it sat on in Spread, which reads well until the two disagree: the user drags
to the right edge and their window lands on the left because the order said so.
The gesture is what the user just performed; the order describes a view they may
not have open. So the order answers who and the gesture answers where.

The side a two-entry Spread draws its second entry on is presentation. It flips
as the user pages, it describes the view, and it reaches neither partner identity
nor pane placement. The other rejected alternative was to give the cyclic order
ends, a first and a last entry, so that a neighbour search could stop at a
boundary. That invents structure the user cannot see to answer a question cyclic
paging already answers.

Partner eligibility is one question asked in one place. The window is on the
current display and the current virtual desktop, Kadunce owns it as an individual
card, and it is awake. Which windows a display adopts is a different question,
asked of native windows, and neither answer stands in for the other. The Bento
group entry and every window inside a live layout fail the partner question, so a
pairing cannot begin a layout by taking a pane out of the live one. A sleeping
card fails it too, because a minimized card enters Bento only when the user puts
it there. Both pairing cases ask this one question, the Active card
included, so neither can drift into its own idea of who may be paired with. They
differ in one place only. The Active card qualifies whether it stands
alone or is a stack's selected member, because the user made it Active; a search
walking the order passes over stacks entirely. A composed group is broken only by
a partner the user named, never by one a walk found.

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
six directed transitions.

The violation rule that reported the state retires with the container; a rule
still worth reporting would mean the container had moved rather than gone.

A window a layout cannot show is an awake individual card, which is an owner the
contract already had. Card ownership lives on one display, so a window leaving a
layout on a display that cannot hold cards moves to the one that can, and the
user meets it in Spread rather than losing it. The rejected alternative left it
Native, which reads tidy until a native window is sitting inside a display
Kadunce is composing — the nameless state this rule exists to remove.

### A refusal is how a layout stays honest

A solve that cannot place every window its session owns awake is refused, and
the caller shortens its batch and gives what it drops to card ownership before
asking again. The rejected alternative was to let the solve succeed and report
the remainder, which is the container's shape with a different name: the
remainder is rebuilt from the session's own restore records on every later
solve, and nothing in the ownership view can see it.

Displacement is therefore spelled as caller shortening. The value layer states
one law and knows nothing about sides or gestures; the publisher removes the
yielding window before asking, which puts §5's rule at the one place that knows
the contacted side. It also keeps "a solve reports, it never moves an owner"
true, because the shortening is a value operation and the eviction is a
published cross-stage transaction.

A window leaves while it is still owned here, which is what gives the card it
becomes the record it had before Bento placed it rather than the pane rectangle
it is sitting in.

### A layout ends into card ownership, never into Plasma

`CARD-LIFECYCLE.md` §5 ends Bento when it falls to one visible pane and makes
that pane an individual card. Ending it by restoring the pane to the desktop
would have been the smaller change and is wrong: §13 reserves the native desktop
for explicit release and disable, so a rule that returns a window there on its
own puts Kadunce back in the state §14 exists to forbid. The remaining pane
therefore leaves by the same eviction a window the layout cannot show uses,
which is what gives the card it becomes the record it had before Bento placed
it rather than the pane rectangle it was sitting in.

Ending is asked of a display that can own cards. Where none can, §5's
destination does not exist, so the session keeps its single pane exactly as
before rather than shedding a window with no owner to become — the same answer
§5 already gives a layout that cannot place what it sheds.

One case is deliberately left out. A session still holding a §7 sleeping window
does not end, because ending it would have to give that window to card
ownership too and card ownership cannot yet hold a sleeping window. That is the
same gap that keeps a minimized pane inside its session, and closing it closes
both.

### The top edge means the same thing from a pane as from a card

`CARD-LIFECYCLE.md` §5 sends a pane dragged to the top edge out of Bento and §10
makes a window carried to the top edge one independent Active card. Those are
one answer, not two, so the edge decision reads the same for a live layout as
for an owned display and only the source differs: a pane gives up Bento
ownership where a card gives up card ownership. Every other snap into a live
layout is §5's displacement, which the entry decision still declines.

Giving the two edges different rules was the alternative, and it is what made
the symptom: a live layout refused the whole entry decision, so a pane carried
to the top edge fell through to the ordinary placement reservation and was put
back into the layout it was trying to leave.

### Two cases the contract does not answer

`CARD-LIFECYCLE.md` §5 says nothing leaves where no display can hold a card,
which is a rule about a live layout shedding. It has no twin for first entry.
A display with more eligible windows than its layout can show, on a system with
no card-owning display at all, therefore adopts what it can show and leaves the
rest where they are. Refusing the whole entry would take the product away from
that hardware entirely, and adopting a window the layout cannot show is the
state this block removes.

§5 puts the user in Spread to find an evicted window, and the adoption that
gives it card ownership is the one a carried card already uses, so a card stage
that was not presenting begins presenting Spread. §8 asks for an Active card
instead, and a launch it refuses gets one, because the host hands a refused
launch to the card stage's own new-window path rather than to that adoption.
Both are recorded rather than designed around; a different answer changes which
entry point an eviction uses, not the ownership it produces.

### A display presenting a layout answers every arrival with the layout

A window is never shown on top of live panes. An arrival at a live Bento joins
the layout: it grows into a free pane, or takes a pane and hands that pane's
window back to card ownership. Only an arrival no slot can hold becomes an
individual Active card, and the layout then leaves the screen as a Spread group
rather than staying behind it.

This replaces growth-only admission, which answered a full layout by putting the
arrival in front of it. That answer produced the one state `CARD-LIFECYCLE.md` §2
does not name: a card drawn over live panes, with the layout visible in the
margins around it and still running underneath. A user who wants one window alone
has Spread and ordinary stacks; the display presenting a layout is not the place
for it.

Both arrival paths ask the same question and need the same answer, including the
answer for an arrival no slot can hold: the layout retires into a Spread group
before the card stage puts a card where the panes were. Only the activation path
had it, so a *launched* application was still drawn over a running layout —
found by physical review on 20 September after the activation path had already
been accepted. `retireLayoutIntoSpreadGroup` is that step, and Card Stage's
new-window path now refuses outright while the display presents Bento, so the
ordering is enforced by the code rather than remembered by each caller.

The cost is accepted deliberately. A window that opens on its own can now take a
pane, which growth-only existed to prevent. On a display with room the layout
simply grows and nothing is displaced; on a two-pane display the exchange is
visible and the displaced window is one Spread entry away. Hiding a called window
behind a layout was judged worse than moving a pane the user can see leave.

### The arrival claims one slot, and only its occupant leaves

A full layout is not re-solved around an arrival. The arrival takes the smallest
slot its minimum size permits and that slot's occupant becomes a card; every
other pane keeps its window, its size and its place.

The first attempt re-solved instead, keeping whichever resident had been
activated most recently and letting the solver rebuild the shape around the
arrival. It satisfied fit and it was wrong to use. Physical review on 20
September: with a wide window and a narrow one live, calling a third window that
needed the wide pane evicted the *narrow* one and squeezed the survivor from the
wide pane into the narrow one — two windows moved when the user had asked for
one, and the survivor changed both size and side. Keeping the most recently used
window is not worth rearranging the panes around it.

Fit alone also answers what recency was there for. A window that fits only the
wide pane takes the wide pane; one that fits either takes the narrower and leaves
the wide pane alone. So a small tool never evicts the large window, which was the
case recency was introduced to protect.

What fit does not answer is a small window the user wants in the *wide* pane.
There is no way to say so on the tablet, which has no side-edge drag. The side
gesture outranks the rule wherever it exists, and letting a Spread drop name the
pane it replaces is the shelved Block 4 item that would give the tablet one.

The rejected alternative scoped auto-admission to external displays. That would
have keyed product behavior to `isTabletOutput`, a name-prefix guess at hardware,
when the property that actually matters is whether the work area has room. A
tablet at its cap and a monitor with no free pane should behave the same way.

The rejection is general, not local to auto-admission. What a display owns and
what it can hold decide what a gesture means; what the display is called decides
nothing. A display Kadunce already owns answers a side snap the same way whether
it is a tablet or a monitor.

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
association would have to be reconciled on every minimize and every displacement.
Scoping to visible panes removes that reconciliation instead of automating it.

One window a session owns is not a visible pane: one §7 put to sleep. It stays
because a minimized window is not an eligible card window, so card ownership
cannot take it, and dropping it would lose the record release needs. That is a
named state with a rule of its own, not a remainder, and it is the only snapshot
a session may hold without showing.

Scoping to visible panes also settles what a pairing may name: the group is one
Spread entry, and neither it nor a window inside it passes the partner test, so a
new layout cannot begin by taking a pane out of the live one.

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
reads it in two ways while Spread selection chooses only what is carried. When
the carried window is something else, the Active card is the partner. When the
carried window is the Active card itself, that identity is the position the
partner is measured from: the search for a neighbour starts where that window
sits in Spread order. Both readings name one window identity and nothing more: no
parked snapshot, no second membership, no fourth owner. It retires the moment
that window stops being an individual card, so a pane or a released window can
never be offered as a partner and can never be where a search begins.

### A prepared ticket carries intent, not a model

Preparation proves a membership, order and grouping delta against a throwaway
model and keeps only the delta; commit re-derives the result from the live one.
A ticket that instead carried a model copy would write selection, page offset,
stack face and neighbour side back into the state that issued it, discarding
whatever the user browsed to while it was held.

A delta that names a partner names a window, not a role. Commit re-derives the
result by applying that name to the live model; it never asks the live model again
which card is Active. A reservation deliberately survives the user's own paging
and selection, so re-reading the role at release would let a pairing approved for
one window act on another.

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

### A displaced pane arrives through a different door than a carried window

`admitTransferredWindowToTablet` exists to make an arriving window the Active
card: it applies Active geometry, leaves Bento presentation and selects the new
member. A pane that yielded its slot needs none of that. §5 makes it a
*nonselected* card, and §2 keeps it hidden because the display is still
presenting the panes it just left.

Sending it through the transfer door is what produced the state physical review
found on 20 September: the displaced window inflated to full screen on top of the
live layout, and the stage left Bento presentation, so every window on the
display collapsed into one group. The journal shows it as two consecutive lines —
the called card joins the layout, then the displaced one is announced as the
Active card.

`admitDisplacedPaneAsHiddenCard` takes membership and the pre-Bento record and
nothing else. No geometry, no selection, no change of presentation. The host
routes to it while the card stage presents Bento and falls back to the transfer
door elsewhere, where there is no layout in front of the window and an ordinary
arrival is right.

### A minimized pane leaves through a third door, asleep

§7 makes a minimized pane a sleeping individual card at once, and §5 says Bento
retains no minimized pane. Neither existing door can take one. The transfer door
presents what it takes as the Active card; the displaced-pane door takes an
awake card that must hide behind live panes, and asks the card stage to be
presenting them. A sleeping card is behind everything by being asleep, so it
arrives in whatever presentation the display already has and is never woken,
selected or given geometry.

Two things follow from where the door sits. Eligibility for card ownership
cannot require the window to be awake, because §7 is the section about a window
that is not: `isCardWindow` answers what this effect can *present* and the
sleeping door asks a separate question about what card ownership can *hold*.
And a pair takes both of its cards, so the stage that gave them up owns nothing
and is not active; the door has to make it active without presenting anything,
which §2 answers, because the display is showing the panes the window just left.

The record travels differently too. The awake door asks the desktop stage for
the window's pre-Bento record, and that request refuses a minimized window, so
the eviction reads the record out of the session itself before the departure
publishes. Without it the state §13 restores the window to would be the one the
user just asked for, and release would re-minimize a window it had woken.

What this unblocks is §5's one-remaining-pane rule. A session still holding a
sleeping window could not end, because ending hands everything to card ownership
and there was nowhere for that window to go. Where no display can own a card
there still is not, and §5's own answer applies instead: nothing leaves, and the
layout keeps the combination it has.

### Destination acceptance precedes source removal

`ARCHITECTURE.md` § Transfer transaction states the order. The decision is that a
transfer is a destination-led transaction rather than a source-led move, because
only the destination can evaluate output, minimum size and its own revision. A
source-led move would have to undo itself after a rejection, and an undo that
runs after native geometry has changed is not reliably reversible.

### Planning is not a workspace change, and a transaction reads before it claims

A live carry's source identity is stamped with the desktop stage's deferred-command
generation, so every advance of that generation reads as a carry that has gone
stale. Two things advanced it for no workspace change: issuing a transaction's own
token, and re-planning a departure on a value copy that publishes nothing.

Both made an eviction refuse the carry it was committing. The visible effect was a
pane dragged to the top edge showing its Active preview and then returning to its
slot, with nothing logged, because a refusal at that point is silent and
indistinguishable from a gesture that never reached the edge.

So a value-copy plan does not invalidate, and a transaction reads the carry before
claiming the guard. Its token then covers the rest of the span: anything that
advances the generation between the read and the commit fails the token check,
which is where a re-read of the carry would have failed.

Automated coverage did not see any of this. Every test of the extraction path
supplied a source-validity stub that always agreed, so the one check that failed in
the field was the one check no test made. The probe now prepares a real carry
source and validates it the way the gesture does.

### Handoff order is carried by a shared sequence, not by source order

Publication before native placement, restore capture between the accepted output
placement and Kadunce's own geometry, and projection retirement before a resume
commits are expressed as duck-typed sequences in `OwnershipHandoff.h`. A caller
supplies the steps and cannot reorder them. Checks therefore assert the observable
order and what a refusal leaves undone, never where a call sits in a file, so a
controller may be restructured without weakening the invariant.

### Existing Bento has destination priority

An incoming card first targets the existing Bento session on the display it
arrives at, whichever display that is. Invalid existing
admission rejects; it does not silently fall through to ordinary desktop placement.
A deliberate edge target that starts a new layout prepares exactly the two
windows the gesture names and recruits nothing else on the display.

### Settling is presentation only

After committed native placement, a short carried-face settle may bridge delayed
client acknowledgement. It cannot write geometry, retain input, or defer the commit,
and ends when the native request diverges or the environment changes.

### Minimum sizes are hard constraints

Bento never forces a client below its useful minimum. It may use the small pane
when the minimum fits. Where cards can exist, a window whose minimum it cannot
satisfy is an individual card rather than parked overflow.

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

### Spread opens from the bezel, not from the dock

Settled by J on 22 September. The bottom swipe that opens Spread is an edge
swipe: it starts at the bezel. A swipe that starts on the dock or in the gutter
above it is not an edge, because that ground belongs to the dock's own touches
and to the Keyboard's handle. The tablet's direct recognizer had widened its
reach to the dock and 36 pixels above it; that reach is withdrawn rather than
arbitrated gesture by gesture.

### One gutter on every edge

The Active card and a Bento layout keep the same gutter on all four sides, the
bottom included. Both once doubled it at the bottom to leave room for a dock
that floated above the work area; that dock is gone, the Shuffle dock and the
Keyboard's handle reserve their own space, and J saw the doubled gap on 22
September as the one edge that did not match. The keyboard never changes a
card's gutter: it lies over the card, and a covered line pans inside it.

### Dock input and dock clearance are separate

Panel controls retain their input. Destination recognition may use the physical
bottom edge, while landing uses work-area and visible-dock geometry plus clearance.

### The platform's answer wins where it has one

Where the desktop already answers an interaction, Shuffle uses that answer rather
than inventing its own. A task is reached by the ordinary right-click menu carrying
the ordinary window actions; an application is pinned, unpinned and reordered the
way every dock does it. Touch reaches the same menu through a long press rather
than through a different behavior. Novelty is spent on the interactions Shuffle
exists to change --- cards, Spread, Bento, the Table --- because a bespoke gesture
elsewhere makes the user relearn something they already know and quietly drops
everything the standard route carries. Settled by J on 22 September, against a
hold-to-pin gesture built for the Shuffle Dock in place of the native menu.

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
and authoritative restore records. Activating the group validates ownership,
membership and shape, commits the reverse ownership transfer, and publishes the
same Bento session without a solve. A pane whose own client changed its frame
while the group was projected no longer refuses the resume: the stored rects
remain authoritative, and after the transfer commits the pane is placed back on
the rect it left. Placement is the stored layout only, and only for a session
with no participation change owing; resume still writes no geometry of its own
and puts nothing to sleep. Rejection leaves the group card and neighboring
Spread ownership intact.
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

### A placement that does not settle sheds; it does not go back to Plasma

A layout asks for its rects once and then reads what arrived. What it used to do
when the reading disagreed was return every window in the session to the native
desktop, which `CARD-LIFECYCLE.md` §13 reserves for release and disable, and
which a user reads as the layout vanishing.

Two things can make the reading disagree, and they deserve different answers. A
client that moves itself while the placement is arriving -- a terminal
reflowing, a scale change re-rounding an edge -- has not refused anything, so
the layout asks for its rects once more and that client settles. A client that
will not take the rect however often it is asked is a window the layout cannot
show, which §5 already answers: it leaves for card ownership, the panes that did
settle keep theirs, and a layout that falls to one pane ends into a card. Where
no display can own a card nothing leaves, which is §5's own answer.

One retry, bounded by the token the session was published with, is what
separates the two without guessing. `settle-runtime` gates both halves: a
one-shot self-move keeps its layout, and a client that keeps moving loses its
pane rather than the display losing the layout.

### Gestures split by distance need the band between them

Reordering and stacking are told apart by how far a held card travels. Stacking
is available up to 48% of a card's width and the reorder committed at 82% of the
row's pitch, which left a band of roughly a third of a card in which neither
fired. That band is not slack. It is the part a hand can feel.

A shorter reorder was built to remove the long sweep, at a quarter of the pitch,
with the row paging under the held card so the result was visible before the
release. It passed every gate it had --- its own runtime probe, the route
matrix, `./verify.sh` --- and physical review rejected it, because the new
distance sat inside the stacking window and closed the band to nothing. The
router then had to suppress stacking outright once the row moved, which is a
correct rule and an unfeelable one: two adjacent thresholds with no gap read as
one ambiguous region.

So the rule is not "the reorder distance was wrong". It is that this row cannot
hold a third distance. Two gestures already spend the travel a card has, and
anything placed between them takes the band from one of them.

A held card compounds it by being drawn from its slot rather than from under the
finger, so identical travel shows a different picture depending on where the
card was picked up. Physical review found that before it found the thresholds.

Block 7b answers this by removing the question: a row that follows the finger
continuously has no thresholds to separate. `wip/reorder-push-20260921` keeps
the attempt and its measurements; it is not promoted, and it should not be
rebuilt as a variant.

### A stack takes no arrival from the desktop

A window carried in from the native desktop becomes an individual card. It never
lands in a stack, and no gesture will be built to put it there.

The plan had carried this as an open product decision, on the reasoning that the
stack seam is reachable only by a card already in Spread and that an arrival
therefore had a destination it could not ask for. J ruled on 21 September that
this is the intended shape, not a gap: what an arriving window needs is card
status, and edge snapping into one or two panes already grants it. Organizing
cards into a stack is a second, separate act the user performs once the window
is a card like any other.

This keeps arrival's destination rules in §8 and stack membership in §9 rather
than letting one gesture decide both, and it removes the atomic
membership-and-insertion transaction that an arrival into a stack would have
required.

### A dock that steps aside is not a display that grew

The keyboard asks Plasma's bottom panels to autohide while it is up and restores
them when it goes. For the length of that round trip the reserved work area is
the whole output, and a layout placed in that window expands 52px into room the
dock is about to take back, then settles when it returns. The Active card does
the same at 894 before landing at 832.

Reading the reservation as real is what produces the excursion. Placement should
hold the reservation the dock had when the keyboard episode began and re-read it
only once the episode has closed. Recorded on 21 September against measurements
from a physical pass; the implementation is deferred with the rest of Block 12a.

### The keyboard overlays; a covered line pans inside a still card

Settled by J on 23 September. The keyboard never reserves workspace and never
moves or resizes a card. KWin lifts the focused window for the keyboard itself,
pinning it to the top of the work area and cutting it off at the keys; that is
a second authority over a window Kadunce owns, and KWin's own
`OverlayVirtualKeyboardOnWindows` setting declines it. Kadunce holds that
setting in memory while loaded and restores the user's value on unload, the
way it holds edge tiling. KWin still puts a lifted window back to its
geometry at keyboard-open when the keyboard goes; with the lift declined that
is the card's own placement, the same one Kadunce restores.

When the keys would cover the Active card's text cursor, the window rises by
the least that shows the cursor a gutter above them and is drawn only from the
card's own top edge down, so the frame stays still and the contents pan like a
blind in a fixed window. Chosen over sliding the whole card. The contents roll
further when the typed line or taller keys need it and never back down until
the keyboard goes, because a view that shifts on every tap reads as busy. The
placement they pan from is the card's own at keyboard-open, not one read from
the work area, since the dock yields to the keyboard and the area grows while
the room does not.

Only the Active card pans. Bento panes and ordinary windows are left covered,
and the person scrolls. A client that reports no cursor is left alone: giving
such a card the keyboard's height, with a tap inside it raising the keyboard,
was built for Ghostty and taken out on 23 September, when a candidate carrying
it refused carried drops on the tablet and no further keyboard work on
cursorless clients was wanted.

### The keyboard comes up for the text, not for focus

Settled by J on 23 September. A pull on the handle leaves the focus where it
was, so a ready text box is typed into and panned into view; only a cold
start, with nothing ready to type into, borrows the focus to get the keys on
screen. Focusing a card is not a request to type: a client that has KWin raise
the keyboard whenever it gains focus has that keyboard put back down when the
stage focused the card itself.

### The dock steps aside for the keys

Settled by J on 23 September, against keeping the dock in place and seating the
keys above it. KWin seats the keyboard on the bottom of the work area, so a dock
that kept its reservation would hold the keys up by its own height, and a dock
under the keys is worse for typing. The dock therefore gives up its room while
the keys are up, and the hand-off that costs is answered by the arrival motion
rather than by keeping the dock.

### Persistent membership is not promised across unload

The public context endpoint and live registry disappear with the effect. Do not add
a service solely to preserve an implementation extraction boundary.

## Safety and packaging

### The tray control is mandatory

`PRODUCT-CONTRACT.md` § System control states the behavior. The decision is that
recovery lives outside the compositor plugin, because the failure it exists for is
the plugin not loading. A control hosted by the thing it recovers cannot recover
it, which is why no effect test can stand in for this one.

It has to be reachable on every build that is tested or shipped; settled by J on
23 September. It does not have to be on screen at every moment: the Bottom
Surface takes it off screen with the dock while the dock steps aside for the
keys, and it returns with the dock. Losing it on a tested build was an early
defect that has not recurred.

### Private tests and live evidence remain distinct

Private KWin sessions use isolated display, runtime, and D-Bus state. Their success
does not establish live control availability, physical touch behavior, frame pacing,
or installed provenance.

### A frozen identity needs a guard that watches both directions

`tests/verify-source.sh` rejects retired vocabulary returning and names the three
layer-3 identities as the only permitted occurrences. That catches a regression
and cannot catch a rename, because a vocabulary pass moves the other way: it
takes the frozen spelling out. Block 1b's mechanical pass rewrote the `cardLine`
presentation to `spread` inside three probe assertions, which then asserted a
value `Effect::workspaceContext` never reports. All three failed from that day,
one of them inside `verify-integrated-carry.sh`, where it made the route matrix
unpassable.

A frozen wire value is checked on the wire: the source must still report it, and
every value a test asserts must be one the source can report. Naming a permitted
spelling protects the word, not the interface.

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
