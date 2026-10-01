# Durable decisions

Why each rule holds and what it rejected, grouped by subsystem. The rules
themselves are stated in `CARD-LIFECYCLE.md` (ownership and presentation),
`ARCHITECTURE.md` (structure) and `INPUT.md` (inputs); an entry here gives the
reasoning and cites them rather than restating them. Superseded reasoning is in
Git history.

## Product and authority

### A dialog waits with its application

Decided 23 September 2026. A dialog is part of the application that opened it,
never a card or a Bento pane; when that application is not in front, the dialog
waits hidden and unfocused and the application is marked as wanting attention
(`CARD-LIFECYCLE.md` §4). Rejected: bringing the application forward on its own,
which lets any application interrupt whatever you are doing. The cost is a
dialog that can go unnoticed, which is why the mark matters.

The mark is Plasma's standard attention flag, so Kadunce and whatever shows it
never depend on each other. Ambient is the place a waiting application shows,
rather than a notification that pulls you to it: a waiting dialog is ongoing
state with an owner and a truthful action, which is Ambient's admission rule,
while Temperance keeps passing events. A Wayland dialog that names no parent is
indistinguishable from an application and is admitted as one; such dialogs are
found by survey rather than guessed at.

### Switching Kadunce on puts the tablet in cards

Decided 23 September 2026. While Kadunce is on, the display that can own cards
holds its windows as cards, and switching on shows the window in use as the
Active card. Rejected: opening Spread instead, which costs a tap after every
sign-in and every toggle.

The rule it replaced waited for a first edge action, so every sign-in began on
the plain desktop and you met the least reliable path, the first carry, every
day. A display with no card left starts again with the next window to open
rather than keeping an empty session, so no state exists that §2 does not name.

### A side snap admits one card; Bento needs two named windows

A window is a Bento pane only because a gesture named it: the carried window and
one partner (`CARD-LIFECYCLE.md` §3). Rejected: letting one side snap fill every
remaining pane from whatever else was eligible. That made the solver, not the
person, decide membership, and every window it could not place became retained
overflow that §14 gives to the card stage while the implementation kept it
inside the session. Requiring a deliberate pair removes that class of defect at
its source. A side snap that pairs with nothing presents its window as an
ordinary Active card, because a one-window Bento must not exist.

### Spread order selects the partner; the gesture places it

The order answers who, and the gesture answers where (`CARD-LIFECYCLE.md` §3).
The carried window takes the edge it was released into and the partner the
opposite side, whichever side of the order the partner came from. Rejected:
letting the partner keep the side it sat on in Spread, which reads well until the
two disagree and a window dragged to the right edge lands on the left. The
gesture is what you just did; the order describes a view you may not
have open.

The partner walk wraps round the order, stepping over entries until it returns
to where it started; the row's two ends (`CARD-LIFECYCLE.md` §6) are
presentation and do not stop it. Which side a two-entry Spread draws its second
entry on is presentation too, and reaches neither partner identity nor pane
placement.

Partner eligibility (`CARD-LIFECYCLE.md` §4) is one question asked in one place,
by both pairing cases, so neither can drift into its own idea of who may be
paired. It differs from the adoption question, which is asked of native windows,
and neither stands in for the other. A composed group or a stack is broken only
by a partner you named, never by one a walk found.

### Overflow is deleted, not reconciled

A window Kadunce owns and cannot currently show is an individual card; there is
no fourth place for it to be. A per-session overflow container once made a
state §5 never allowed and nothing could name: retained by a session, owned by
nobody.
Rejected: keeping the container and teaching each transition to reconcile it,
which preserves the state it was meant to remove and grows with every new
transition. Removing it means a full Bento displaces rather than parks, and
displacement is one of the six directed transitions.

A window a layout cannot show moves to the display that can hold cards, and the
person meets it in Spread. Rejected: leaving it Native, which puts a native
window inside a display Kadunce is composing, the nameless state this rule
exists to remove.

### A refusal is how a layout stays honest

A solve that cannot place every window its session owns awake is refused, and
the caller shortens its batch and gives what it drops to card ownership before
asking again. Rejected: letting the solve succeed and report the remainder,
which is the overflow container under another name, rebuilt from restore records
on every later solve and invisible to the ownership view.

Displacement is therefore caller shortening: the value layer states one law and
knows nothing about sides, and the publisher, the one place that knows the
contacted side, removes the yielding window before asking. A solve still reports
and never moves an owner. The window leaves while still owned, so the card it
becomes carries the record it had before Bento placed it, not the pane rectangle.

### A layout ends into card ownership, never into Plasma

When Bento falls to one visible pane, that pane becomes an individual card
(`CARD-LIFECYCLE.md` §5). Rejected: restoring it to the desktop, the smaller
change, because §13 reserves the native desktop for explicit release and
disable. The remaining pane leaves by the same eviction a window the layout
cannot show uses, which gives it its pre-Bento record. Where no display can own
cards, §5's destination does not exist and the session keeps its single pane.

### The top edge means the same thing from a pane as from a card

§5 sends a pane carried to the top edge out of Bento, and §10 makes a window
carried there one independent Active card. They are one answer, so the edge
decision reads the same for a live layout as for an owned display and only the
source differs. Rejected: separate rules per edge, which is what once put a pane
carried to the top edge back into the layout it was leaving.

### Two cases the contract does not answer

First entry on a display that cannot own cards is answered by § A display
without cards organizes everything it shows. An evicted window reaches the card
display through the adoption a carried card uses, so an idle card stage begins
presenting Spread, while a refused launch becomes the Active card. A different
answer would change which entry point an eviction uses, not the ownership it
produces.

### A display presenting a layout answers every arrival with the layout

A window is never shown on top of live panes (`CARD-LIFECYCLE.md` §8). Rejected:
growth-only admission, which answered a full layout by drawing the arrival in
front of it, the one state §2 does not name: a card over live panes, the layout
running underneath. A person who wants one window alone has Spread and stacks.
Both arrival paths, launch and activation, retire the layout into a Spread group
before an arrival no slot can hold becomes a card, and the card stage refuses a
new window outright while its display presents Bento, so the order is enforced
by the code rather than remembered by each caller.

The accepted cost: a window that opens on its own can take a pane. On a display
with room the layout grows and nothing is displaced; on a two-pane display the
exchange is visible and the displaced window is one Spread entry away. Hiding a
called window behind a layout was judged worse.

### The arrival claims one slot, and only its occupant leaves

A full layout is not re-solved around an arrival (`CARD-LIFECYCLE.md` §8).
Rejected after physical review on 20 September: re-solving while keeping the
most recently used resident, which moved two windows when you asked for
one and changed the survivor's size and side. Fit alone answers what recency was
for: a window that fits either pane takes the narrower, so a small tool never
evicts the large window. What fit cannot answer is a small window you
want in the wide pane; the side gesture outranks the rule where it exists, and on
the tablet a Spread drop names its pane (`CARD-LIFECYCLE.md` §5).

What a display owns and can hold decides what a gesture means; what the display
is called decides nothing. Rejected: scoping automatic admission to external
displays, which keyed behaviour to a name-prefix guess at hardware when the
property that matters is whether the work area has room.

### The Bento shortcut names a pair, and targets a display rather than the pointer

It targets the display `CARD-LIFECYCLE.md` §3 gives it: the external display
while one is attached, otherwise the tablet. It used to read the pointer, which
on a touch tablet is wherever the pointer was last left, so it acted on a
display you were not looking at; that path was removed rather than kept as
a second rule. On a display that can own cards it names a pair the way a side
snap does, for the reason § A side snap admits one card gives.

## Card model and ownership

### Bento owns only the visible pane combination

Ownership is scoped to what is visible, not to a remembered group: a window the
person cannot see is one they will look for elsewhere, and a retained association
would have to be reconciled on every minimize and every displacement. A window
§7 puts to sleep leaves as a sleeping card; only where no display can own a card
does the session keep it, because dropping it would lose the record release
needs. The group is one Spread entry, and neither it nor a window inside it
passes the partner test, so a new layout cannot begin by taking a pane from the
live one.

### The tablet keeps two panes; the three-pane grammar stays dormant

Decided 19 September 2026. A three-pane grammar was built as far as its shapes
and contact mapping and raised questions not worth answering to reach an
install: which resident holds which pane, what the shapes mean in portrait, and
whether the mapping extends to the monitor. It stays in the source behind
`BentoContactGrammarPaneCap`, which no display's cap reaches; every admission
path reads one pane cap per display. Raising the cap to three reopens those
questions and owes the coverage the retired `column-runtime` scene carried.

### Both stages own windows on the same display

Card Stage owns individual cards and Desktop Stage the display's Bento panes at
the same time, and neither releases the other's windows. Rejected: alternate
whole-display owners, under which starting Bento released every card to Plasma
and resuming a group restored its Spread neighbours, which made §6's
"individual cards remain independently owned and hidden" unimplementable. A
third card presentation names what Card Stage does while panes are on screen;
it is presentation, not ownership, so there are still three owners and six
transitions.

### The Active card is remembered as presentation context

Which card is Active survives entering Spread, because §3 reads it two ways:
as the partner when something else is carried, and as where the partner search
starts when it is the carried window itself. It names one window identity and
nothing more (no parked snapshot, no second membership, no fourth owner), and
retires the moment that window stops being an individual card.

### A prepared ticket carries intent, not a model

Preparation proves a membership, order and grouping delta against a throwaway
model and keeps only the delta; commit re-derives the result from the live
model. Rejected: a ticket carrying a model copy, which would write selection,
page offset, stack face and neighbour side back over whatever you browsed
to while it was held. A delta that names a partner names a window, not a role,
so a pairing approved for one window cannot act on another after you
page.

### Two revisions, because a ticket and a preview fear different changes

`revision()` counts every state command; `ownershipRevision()` counts only
membership, order and grouping. Admission and removal bind to ownership, so a
reservation survives your own paging while a membership change voids it.
Stack insertion keeps the stricter binding because its delta names a visible
stack face. Splitting the counter is safe only because tickets no longer carry
presentation; tried before that, it turned a conservative refusal into silent
view corruption.

### Ownership is a value that can see all three owners

Individual cards live in the card stage, panes in the desktop stage's sessions,
and Native only as absence from both, so no object could evaluate §14's first
invariant. `CardOwnership.h` reduces both containers to identities and evaluates
it in one place. It observes and never repairs: a violation is a pre-existing
defect in the caller, not a reason to refuse the caller's work.

### Ownership changes only by one of six directed transitions

Three owners make exactly six directed moves. `CardOwnershipLedger` accepts only
those, and refuses a move to the owner a window already has rather than absorb
it, so a caller cannot use the ledger to paper over a container it failed to
update. It reports disagreement with the stages' containers and never repairs
it, because a disagreement is a defect, not a difference to average away.

### A prepared type is not always an ownership transition

Of the five prepared types only admission and removal change an owner. Stack
insertion is grouping inside card ownership, prepared drops carry geometry and
intent, and a carry source is read-only provenance. Expressing those as
ownership transitions would put placement back inside ownership tickets, which
narrowing the ticket payload removed.

### The card display shows cards or the desktop, never both

A window let go at its bottom edge lands on the desktop, shown with it; the rest
waits in Spread (`CARD-LIFECYCLE.md` §2). Rejected: windows over cards, which
the keys, dock and Spread assume away; only the last card leaving.

## Transfer and native placement

### A displaced pane arrives through a different door than a carried window

The transfer door makes an arrival the Active card: Active geometry, selection,
leaving Bento presentation. A pane that yielded its slot needs none of that; §5
makes it a nonselected card and §2 keeps it hidden behind the panes it left.
Sent through the transfer door, it inflated over the live layout and collapsed
the display into one group. `admitDisplacedPaneAsHiddenCard` takes membership
and the pre-Bento record and nothing else.

### A minimized pane leaves through a third door, asleep

Neither existing door can take a minimized pane: one presents its window as
Active, the other takes an awake card hidden behind live panes. A sleeping card
is behind everything by being asleep, so it arrives in whatever presentation the
display has and is never woken, selected or given geometry. Eligibility for card
ownership therefore cannot require the window to be awake: `isCardWindow` answers
what the effect can present, and the sleeping door asks what card ownership can
hold. The eviction reads the window's record from the session before the
departure publishes, because the desktop stage refuses that request for a
minimized window, and without it release would re-minimize a window it had
woken.

### Destination acceptance precedes source removal

A transfer is destination-led (`ARCHITECTURE.md` § Transfer transaction)
because only the destination can evaluate output, minimum size and its own
revision. Rejected: a source-led move, which must undo itself after a rejection,
and an undo run after native geometry changed is not reliably reversible.

### Planning is not a workspace change, and a transaction reads before it claims

A live carry is stamped with the desktop stage's deferred-command generation, so
anything that advances it reads as a stale carry. Issuing a transaction's own
token and re-planning on a value copy advanced it for no workspace change and
made an eviction refuse the carry it was committing, silently. So a value-copy
plan does not invalidate, and a transaction reads the carry before claiming the
guard; its token covers the rest of the span. A probe that stubs a check the
gesture makes is not coverage of that check; prepare a real carry source.

### Handoff order is carried by a shared sequence, not by source order

Publication before native placement, restore capture between output placement
and Kadunce's own geometry, and projection retirement before a resume commits
are duck-typed sequences in `OwnershipHandoff.h` that a caller cannot reorder.
Checks assert the observable order and what a refusal leaves undone, never where
a call sits in a file, so a controller may be restructured freely.

## Input and gesture ownership

### Spread comes from the bezel and three fingers; the keys from their tray entry

Decided 28 September 2026. A swipe up from the bottom bezel opens Spread under
the finger. Three fingers stay, down for Spread and up for Table, keeping KDE's
own gestures off the screen; Table's top edge stays its first way in, since a
hand placed for three fingers scrolled pages by accident. The keys come from a
text field and from the Keyboard's own tray entry, which asks through Kadunce so
the request is yours. `INPUT.md` states each gesture. Rejected: the bezel
bringing the keys, since one finger up from the edge is the most natural way
into Spread and the dock has no room for a handle; keys that follow the finger,
which talk to Kadunce every frame; and the recognizer reaching over the dock.

### Kadunce's keys sit on Meta and are never written

Decided 28 September 2026. `Meta+S` opens Spread, `Meta+W` Table, `Meta`'s
arrows move through cards while they are shown, `Meta+B` starts Bento and
`Meta+Esc` lets go; an input filter ahead of KDE's shortcuts takes them only
while Kadunce runs (`INPUT.md`, whose controls map lists them). `Ctrl` stays
with applications, `Alt+Tab` with KDE, and `Meta+G` and `Meta+E` with
Tettegouche. Rejected: KGlobalAccel, which writes every key to your
settings and gives a shared key to its oldest owner; and `Ctrl` keys, which took
Save, Bold and word moves from every application.

### KDE's Overview is set aside while Kadunce runs

Decided 28 September 2026. Kadunce unloads KDE's Overview while it runs, and
with it Overview's fingers, `Meta+W`, `Meta+G` and corner, and loads it again
when it stops unless you turned it off. Rejected: sharing the fingers,
since KWin runs every registration of a swipe, and taking Overview's gestures
one by one, which KWin offers no way to do. Sideways three-finger desktop
switching is built into KWin and stays KDE's.

### One gutter on every edge

The Active card and a Bento layout keep the same gutter on all four sides.
Rejected: a doubled bottom gutter, left over from a dock that floated above the
work area; the Shuffle dock reserves its own space, and the doubled gap was the
one edge that did not match (22 September 2026). The keys never change a card's
gutter.

### The platform's answer wins where it has one

Where the desktop already answers an interaction, Shuffle uses that answer: a
task is reached through the ordinary right-click menu, and an application is
pinned, unpinned and reordered the way every dock does it; touch reaches the
same menu by a long press. Novelty is spent where Shuffle exists to change
things (cards, Spread, Bento, Table), because a bespoke gesture elsewhere makes
you relearn what you know. Decided 22 September 2026, against a hold-to-pin
gesture built for the Shuffle Dock.

The Shuffle Dock starts from everything Plasma's Icons-only Task Manager does; a
stock feature it lacks is a defect. One dock lists every display's windows, so a
window a monitor layout sent there is picked up where you already look (24
September 2026, against a dock on each display).

### Spread gives every move one meaning

Decided 27 September 2026 from a trial of the whole map. In Spread sideways
travels, up closes, down takes out, a hold carries and a tap opens. A Bento
group, which can hold eight applications, never closes by a flick; Spread has no
window menu. On the centred Stack a slow stroke scrubs its cards and a quick one
moves the row. Bento comes only from carrying the Active card to a side.
Rejected: the long reorder sweep and edge-dwell paging. Moves told apart by a
held card's travel need a band between them a hand can feel; a shorter reorder
that closed it passed every automated check and failed by hand (`wip/reorder-
push-20260921`).

### A stack takes no arrival from the desktop

A window carried in from the native desktop becomes an individual card and never
lands in a stack. This is the intended shape (21 September 2026): an arriving
window needs card status, which an edge already grants, and organizing cards
into a stack is a second, separate act. It keeps arrival's rules in §8 and stack
membership in §9, and removes the atomic membership-and-insertion transaction an
arrival into a stack would need.

### The Active card makes room for the keys

Decided 24 September 2026. The keys never
reserve workspace; the Active card they type into gives up the room they take:
its top edge, width and place stay and its bottom edge rests a gutter above the
keys, so a message box at an application's bottom edge lands on them. Rejected: panning the contents to the text cursor, the
only thing a client reports, which left the rest of the box covered.

While the keys move, the card is drawn ending a gutter above them on every frame
and its client is asked for a size once a motion (26 September 2026). Rejected:
asking every frame, which made a client trail the keys.

KWin's own lift of the focused window would be a second authority over a card,
so Kadunce declines `OverlayVirtualKeyboardOnWindows` while loaded, as
it does edge tiling. Only keys typing into the card or its own dialog make room;
Spread, Bento panes and ordinary windows make none. Cards are laid out in the
work area as it stood when the keys came until the dock takes its room back,
because the dock yields to the keys (29 September 2026). Rejected: the live work
area, which gave a card chosen meanwhile the dock's room. KWin moves only a
window touching an edge that moved, so a panel taking or giving up room places
the Active card again.

### The keyboard comes up for the text, not for focus

Decided 23 and 24 September 2026: the keys come up only when you ask for them,
by tapping a text field or Keyboard in Control Center or the tray. The Keyboard
entry leaves the focus where it was; only a cold start, with no text box ready,
borrows it. An application focusing a field on its own is not a request to type,
so keys KWin raises for it are put back down and the dock stays.

### The dock steps aside for the keys

Decided 23 September 2026: KWin seats the keys on the work area's bottom, so a
dock that stays holds them up by its height. The dock gives up its room while
keys are on screen, not merely asked for (24 September 2026).

### The latched trackpad is the pointer

Decided 24 September 2026: pointing lives in the latch at the space bar's end,
which holds the keys up. Rejected after a build: a space-bar slide, whose clicks
put the keys down.

### The keys' handle takes them away

Decided 24 and 28 September 2026. The handle on the keys carries them away by a
drag down or a tap, and height is the right-edge scrub column's; the keys come
from a text field or their tray entry, and the dock no longer carries a handle.
Rejected: a drag on the handle resizing, which would make the edge that opens
the keys upward shrink them downward.

### Persistent membership is not promised across unload

The context endpoint and live registry disappear with the effect. Rejected:
adding a service solely to preserve membership across unload.

## Rendering and output

### Bento projects as one reversible group card

A layout in Spread is one ordinary card slot showing its live panes in place
(`CARD-LIFECYCLE.md` §2, §9), and choosing it resumes the same session without a
solve, so you get back exactly what you left. The stored pane rects stay
authoritative: a pane whose own client moved it meanwhile is placed back on its
rect rather than refusing the resume and leaving the group unopenable. Placement
comes from the transferred rects, not later client geometry, so live-frame
acknowledgements cannot move a pane inside the group.

### Sessions are output-local

Output-local sessions (`ARCHITECTURE.md` invariant 6) let the tablet change
presentation without disturbing a display you are still reading. Rejected:
one global session, which makes every tablet gesture a multi-display event.

### A display without cards organizes everything it shows

Decided 23 September 2026; `CARD-LIFECYCLE.md` §11 states the rule. A window
with no room waits in the dock, never moves to another display on its own, and
comes back when room frees up (24 September 2026). An application too big for
any slot takes the Active size, because one you just opened has to appear.
Overflow stays in the dock (26 September 2026); rejected: another desktop.

The tablet names each window it pairs; this is the opposite answer on purpose.
On the tablet a person places each card; a monitor is where Plasma windows pile
up under a mouse, and the value is organizing the pile in one move. Rejected:
pairing on the monitor, which traded that for control, and sending overflow to
the tablet, which moved windows between screens without you and had no
destination on a machine without a touchscreen.

### A display coming or going keeps every card on the card display

KWin moves windows itself when a display is plugged in, unplugged or moved, and
Kadunce answers once it has finished: a card KWin moved off the card display
goes back, because §14 returns a card to the desktop only by release or disable,
and a window KWin moved onto it becomes a card. Decided 23 September 2026: the
arrival you used last becomes the Active card, so you pick up where you were on
the monitor; the other arrivals and the previous Active card wait behind it.
Rejected: letting KWin's move stand and dropping the card, which returns a
managed window to the desktop without a release. A client's late answer can take
a card back to the other display, so the return is checked again after that
round trip.

### A placement that does not settle sheds; it does not go back to Plasma

A layout asks for its rects once and reads what arrived. Rejected: returning the
whole session to the desktop when the reading disagreed, which §13 reserves for
release and disable and a person reads as the layout vanishing. A client that
moved itself while the placement arrived has refused nothing, so the layout asks
once more; one that will not take its rect however often it is asked is a window
the layout cannot show, and leaves for card ownership while the settled panes
keep theirs. One retry, bounded by the session's token, separates the two
without guessing.

## Safety and packaging

### The tray control is mandatory

Recovery lives outside the compositor plugin (`ARCHITECTURE.md` § Safety and
teardown), because the failure it exists for is the plugin not loading: a
control hosted by the thing it recovers cannot recover it, which is why no
effect test can stand in for it. It must be reachable on every build that is
tested or shipped (23 September 2026), though not on screen at every moment: it
leaves with the dock while the dock steps aside for the keys.

### A frozen identity needs a guard that watches both directions

`tests/verify-source.sh` rejects retired vocabulary and names the two layer-3
identities as the only permitted occurrences. That catches a regression and
cannot catch a rename: a vocabulary pass once rewrote the frozen `cardLine`
value inside three probe assertions, which then asserted a value the effect
never reports. A frozen wire value is checked on the wire: the source must still
report it, and every value a test asserts must be one the source can report.

### A consumer gets a working plugin from the package manager

KDE publishes no stable KWin effect ABI, so a Plasma update can leave the plugin
unloadable. That failure is safe: KWin declines the plugin, the tray switch
persists and the desktop keeps working. Decided: the plugin ships as a package
from a project pacman repository, built per KWin release in CI, so a corrected
plugin arrives through ordinary system updates. Rejected: the guided repair as
the consumer path, which needs a full C++, Qt, KDE Frameworks and KWin toolchain
on every installation. It is built with the consumer bundle (Block 10); on
development machines the guided repair is adequate.

## Across the suite

### Table ships in 1.0

Table ships in 1.0, replacing the earlier tentative 1.1.

### Cards follow the touchscreen

Decided 23 September 2026. The display a touchscreen drives holds cards, whether
it's a tablet's own panel or a touch monitor; every other display uses Bento
layouts, and a computer without a touchscreen gets Bento and Table only. Kadunce
finds that display from the touchscreen as KWin places it, never from the
display's name, which says nothing about touch. Where two touchscreens drive
different displays, the built-in one holds cards.

### Shuffle Keyboard is required and uses system input plumbing

Shuffle owns layout, resizing, editing gestures and the keyboard to
precision-surface transition; KDE, KWin or another mature input-method stack
owns locale, keymaps and application delivery. A custom input engine is a last
resort.

### The install leaves a person's own applications alone

Decided 29 September 2026: Shuffle styles what Plasma draws and the settings KDE
and GTK share, not a person's terminal or browser. Rejected: installing
Ghostty's palette, now a reference in `ITASCA-VISUAL-LANGUAGE.md` § Terminal
palette.

### App chrome lifts one step, title bar included

Decided 29 September 2026: title bars, toolbars and sidebars sit one step above
content, one frame around it. Rejected: a black title bar over lifted toolbars.
The rule is `../shuffle/docs/APPEARANCE.md`.

### Ghost White is the highlight out of the box

Decided 29 September 2026. Shuffle highlights in Ghost White, as Control Center
does, and an accent you set takes its place, keeping the suite
monochrome until they ask for color. Rejected: the first appearance's teal. The
rule is `../shuffle/docs/APPEARANCE.md`.

### Corners come in three tiers

Decided 29 September 2026: 8 px paper, 12 px floating notes, pills for actions.
The 8 px cut reads modern and cuts less content by accident; notes stay a step
softer so they read as temporary. Rejected: 8 px for every surface, and the
earlier 14 to 18 px boxes. The rule is `ITASCA-VISUAL-LANGUAGE.md` § Rounded
boxes.

### Files is the file manager

Decided 29 September 2026. With Shuffle installed, Files opens every folder,
Plasma's drive pop-up and "Show in folder" included; Dolphin keeps what Files
does not browse. Rejected: Dolphin staying the file manager, which keeps a
folder window beside work. The rule is `../tettegouche/docs/FILES-CONTRACT.md` §
Answering as the file manager, and `../shuffle/install-file-manager.sh` switches
it on.

### Shuffle Lock shows nothing from the session

Decided 25 September: the lock shows the time, the credential field and the
Keyboard's handle, and nothing from the session. The contract is
`../shuffle/docs/LOCK-CONTRACT.md`.
