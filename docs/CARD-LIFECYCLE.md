# Card lifecycle

Who owns each window, how it is presented, and what every transition does to
it. Which input causes each transition is `INPUT.md`.

## 1. Core model

Every eligible application window has exactly one owner:

1. **Native**

   - Plasma owns the window.
   - It is outside Kadunce.

2. **Individual card**

   - Kadunce owns the window as an independent card.
   - It may be Active, visible in Spread, sleeping, or hidden behind another
     selected state.

3. **Bento pane**

   - Kadunce owns the window as part of the display's current visible Bento
     combination.
   - In Spread it appears inside the single Bento group card, or as its own
     card on a Connected display (§11).

A window cannot be an individual card and a Bento pane at the same time.

## 2. Presentation states

Ownership and presentation are separate.

### Active

One individual card is shown as the current window.

Other cards remain owned and hidden.

### Bento

The display's current Bento pane combination is shown.

Individual cards remain owned and hidden.

### Spread

Spread shows:

- Every individual card
- Every ordinary card stack
- The display's single Bento group, if Bento exists

The Bento group is one Spread entry, whatever its pane count.

### Sleeping

A minimized individual card remains owned but is not presented. Spread draws
it dimmed in its place; selecting it there, by touch, click or keys, wakes it and presents it
as Active.

### Desktop

The display that owns cards shows the ordinary desktop, with the window you
returned to it at its bottom edge (§10). Every card and the Bento group stay
owned, held aside out of sight and reach. Spread brings them back, and going
back from it returns here; choosing a card or opening an application presents
one again, and asking for a returned window shows the desktop again.

### Native desktop

Released from Kadunce, the window is neither owned nor hidden by it.

## 3. Display admission

Switching Kadunce on starts ownership of the display that can own cards, for
the current virtual desktop. The window in use becomes the Active card, and
every other eligible window there becomes an individual nonselected card. When
that display holds no card, the next application window to open there starts
ownership the same way and becomes Active.

Every virtual desktop has its own ownership session, begun the first time it is
shown. A window KWin sends to another
desktop, Table's moves included (`TABLE.md`), leaves its session where it
stands; that desktop takes it as a card once shown.

On a display that cannot own cards, the first deliberate Bento action starts
ownership. The edge entries below still answer a window carried onto a display
Kadunce does not own.

Kadunce atomically adopts every eligible window on that display,
each awake card held in the Active card's place.

Kadunce owns a display from then until its session ends. Adoption happens
once; later edge actions read the ownership it has.

A window arriving from another display starts ownership the same way, carried
there or leaving a layout that could not show it. It arrives as one
card; arrival adopts nothing else.

### First edge entry

On a display Kadunce does not yet own, a top, left or right snap gives the same
result:

- The carried window becomes an individual card.
- It becomes Active.
- Every other eligible window becomes an individual nonselected card.

### Pairing into Bento

On a display Kadunce already owns, with no live Bento layout, a left or right
snap requests a Bento pair of exactly two named windows. One is the carried
window. The other is the partner, and one of two rules names it.

When the carried window is not the Active card — a window still on the native
desktop — the partner is the Active card. A card carried in Spread never pairs.

When the carried window is the Active card, the partner is the card Active just
before it, from either side. Failing that, it is the nearest eligible card on the
contacted side in Spread order, walking cyclically from the entry holding the
carried window.

Placement comes from the gesture:

- The carried window takes the side it was released into.
- The partner takes the opposite side.
- Both become Bento panes.
- No other window joins. Kadunce never fills an unrequested pane.

The partner is named when the edge is contacted, and a presentation change
during the carry never substitutes another window.

When no eligible partner is named, nothing pairs. The carried window becomes an
individual Active card, unless it already is the Active card, in which case
nothing changes.

Only the side edges pair. A top snap on a display Kadunce already owns makes the
carried window an individual Active card.

An entry commits completely or not at all. If adoption cannot be prepared safely,
every window remains Native. If a pair cannot be prepared safely, the carried
window and the partner keep the ownership and presentation they had.

### The Bento action

A Bento action is a request without a carried window or a contacted edge
(`INPUT.md` § Bento, `Meta+B`). It belongs to Spread's display with cards on
every display (§11), else to the largest display that cannot own cards, else
the one that can.

On a display that can own cards, it pairs like a side snap from the right:
the Active card keeps the left side. With no card to pair, nothing happens.

On a display that cannot own cards, it composes across the display.

A Bento action on a display with a live layout ends it; with cards on every
display its windows stay cards.

## 4. Eligible windows

### Eligible for adoption

Kadunce adopts normal top-level application windows on the current display and
current virtual desktop.

Kadunce does not create independent cards for:

- Panels or docks
- Desktop surfaces
- Tooltips or menus
- Temporary popups
- Child dialogs that must follow a parent
- Windows explicitly excluded from normal task switching
- Windows set to appear on every virtual desktop
- The screenshot tool's capture, floating over cards

A dependent dialog follows its owning application card. Over that card, or
over the pane its application holds, it floats at its own size. When that card
is not in front, the dialog waits with it, hidden and unable to take focus, and
the application is marked as wanting attention; bringing the application
forward shows the dialog on top. In Spread a waiting dialog is drawn on its
application's card, where it stands over the application. A card carried away
takes its dialogs along, each landing in the same place on its application, or
on its middle when wider than it, and wholly on the display.
Switching Kadunce off gives every waiting dialog back. A dialog that names its application only once it is shown, as
Electron's message boxes do, follows it from then on, even if it was first
taken for a card.

### Eligible as a Bento partner

The partner is the card Kadunce pairs with the carried window. It must be:

- On the current display
- On the current virtual desktop
- Owned as an individual card
- Not sleeping

The carried window is never its own partner. The Bento group is never a partner,
and neither is a live pane. A window on the native desktop is never a partner; it
can only be the carried window.

The Active card is a partner whether it stands alone in the Spread or is the
selected member of an ordinary stack. Pairing takes a named stack member out of its
stack and leaves the rest of the stack unchanged, whether that member is the
partner or the carried window.

A partner search passes over every Spread entry that fails this test, and over
every ordinary stack. A card leaves a stack for Bento only when the user named
it by using it, never because a search walked past it.

## 5. Bento rules

Each display can have only one live Bento layout.

Bento owns only the windows visible in its panes: never hidden overflow, nor
minimized or displaced panes.

### Pane limit

Each display has one maximum visible pane count, read by every admission path.

The tablet's maximum is two; a larger display, what minimum sizes allow, or its
zones (§11).

Layout orientation follows the work area's own proportions, never the display's
hardware identity.

### Shape, not size

A preset fixes how panes are arranged, not their proportions. A preferred
proportion is a starting point, clamped by each window's minimum size, and the
user then moves any shared boundary with its rail. A shape the minimums cannot
satisfy is not used.

### Choosing a shape by side contact

A side snap carries a size intent, larger or smaller, taken from where the edge
was touched (`INPUT.md` § Active card and § Bento). Contact near the edge's midpoint
keeps the previous choice.

The carried window keeps the edge it was released into and takes the share its
half asked for; Connected, the half names a place instead (§11). This holds whether the carried window comes from the native
desktop, from an individual card, or is itself the Active card.

### When a pane leaves Bento

The window becomes an independent Spread card when it is:

- Taken out by you, at the top edge or from the group in Spread
- Minimized
- Displaced by a new pane combination
- Removed because it no longer fits
- Excluded from the current visible layout
- Moved to another workspace

It keeps no Bento position or group, and stands in the Active card's place (§3).

A window leaves for the display that can hold it as a card, keeping its
restore record so release still returns it where it began. You find it in
Spread there and may carry it back into the layout.

Where no display can hold a card, nothing leaves. The layout keeps the
combination it has rather than shedding a window with no owner to become.

### One remaining pane

When Bento falls to one visible pane, Bento ends.

The remaining pane becomes an individual card.

### Returning a card to a live Bento

A display with a live Bento layout does not begin a new pair. A left or right
snap targets that layout.

An individual card returns to a live Bento through an explicit left/right-edge
action, by the user calling it forward, or by a drop on its group in Spread.

If the Bento combination is full, the displaced pane becomes an individual card.
Where the return was a side release, the pane that yields is the one occupying
the side the card was released into. The user can see which pane will yield while
dragging, and no interaction history decides it.

A card called forward releases into no side, so §8 decides which pane yields.
Every display answers a call forward; not every display offers the side gesture.

In Spread, the group's pane under a held card gives way to a cutout and the
card slides under it; let go, the card takes that pane's place, size and side,
the layout opens, and the pane becomes a card just after the group (§9). A pane
too small for the card does not give way.

## 6. Spread selection

### Spread order

The Spread has one order. Its entries are individual cards, ordinary stacks and
the display's Bento group. The row that shows it has two ends, and moving along
it stops at them. When the card used before the Active card cannot pair, that
order names a Bento partner, the search wrapping and stepping over entries until
back where it started.

Which entry is selected, and which side a two-entry Spread draws its neighbour
on, are presentation, deciding neither the partner nor a pane's side.

### Selecting an individual card

That card becomes Active, or on a Connected display a pane (§11). Everything
else stays owned and hidden; no window returns to the native desktop.

### Selecting the Bento group

Only the Bento group resumes. Individual cards stay owned, hidden and out of it.

### Returning to Spread

The same peer entries return:

- Independent cards remain independent.
- Ordinary stacks preserve their membership.
- Bento returns as one grouped entry, or on Connected one card per pane.
- Selection remains on the most recently presented entry.

## 7. Minimize behavior

### Minimizing an individual card

- It remains an individual card.
- It becomes sleeping and nonselected.
- Its user-minimized intent is preserved.

### Minimizing a Bento pane

- It immediately leaves Bento.
- It becomes a sleeping individual card.
- Bento reflows its remaining visible panes.
- Selecting the sleeping card wakes it as Active.

A minimized card never silently returns to Bento.

## 8. Arrivals at a live Bento

A display presenting its Bento layout answers every arrival with that layout. An
arrival is a new window, or an individual card the user calls forward. Nothing is
ever shown on top of live panes.

### Admitting an arrival

- If the layout can grow to show it, Bento recomposes and the arrival becomes a
  pane.
- If the layout is full, a pane yields and becomes an individual card, and the
  arrival takes its place.
- If no pane the layout can offer satisfies the arrival's minimum size, the
  arrival becomes an individual Active card and the layout becomes a separate
  Spread group. It does not remain on screen behind the card.

An arrival is never parked.

### Which pane yields

The arrival claims one slot, and only that slot's occupant leaves. Every other
pane keeps its window, its size and its place. One window changes.

The slot is the smallest one whose size the arrival's minimum size permits. A
window that fits only the wide pane takes the wide pane; one that fits either
takes the narrower and leaves the wide pane alone. On a Connected display (§11)
it is the pane used longest ago among those that fit.

The layout does not reshape to make room. Where no slot can hold the arrival,
Bento does not take it at all and the rule above for an arrival that fits
nothing applies instead.

A stated intent outranks this. Where the user releases a card into a side edge,
§5 gives the yielding pane to that side.

### While an individual card is Active

The new window becomes a new individual Active card.

The previous Active card remains an individual Spread neighbor.

### While Spread is open

The new window becomes an individual card and becomes the selected Spread entry.

## 9. Ordinary stacks

Individual cards may be organized into ordinary Spread stacks.

A stack:

- Has one selected member
- Remains separate from Bento
- Brings its members forward in turn, a ring side steps never leave
- Keeps its fan's room wherever it stands; landing on it moves nothing
- Can release a member back into the Spread
- Becomes an individual card when one member remains
- Shows its active position in the label row
- Takes no arrival from the native desktop: a carried window becomes a card
  under §8, and joining a stack is a separate act

A Bento group:

- Is not an ordinary stack
- Cannot page its pane members
- Cannot be inserted into another stack
- Cannot retain removed panes

### Releasing a member

A released member becomes an individual card just after the stack, which keeps
its place, its order and the selection; a released front card gives way to its
nearest neighbour. A release that does not complete leaves the stack unchanged.
A Bento group gives up a pane the same way, and is never closed whole from
Spread.

### Reordering a member

A member reordered within its stack stays with it and never reaches the row or
an edge. It takes the place it is let go at, counted from the front, and the
stack then shows whichever member is in front; let go where it began, the stack
is unchanged.

## 10. Edge actions

An edge action is what a carried window's release at a screen edge means
(`INPUT.md` § Active card and § Bento).

### Top edge

Make the carried window an independent Active card. The top edge never pairs.

### Left or right edge

§3 governs a side snap, §11 one on a Connected display, and §5 one into a live
Bento layout.
Spread's side edges on the display that owns cards are not edge actions.

### Bottom edge

Release the carried window to the ordinary Plasma desktop.

On the display that owns cards, §2's Desktop is then shown with it and the rest
waits in Spread; a last card ends the display's session, as §12's last closure
does.

### Cancel

A cancelled carry returns the window to its exact ownership and presentation
state from before the carry.

An edge action commits only when released inside its valid edge zone.

## 11. Multiple displays

Each display, on each desktop, has its own ownership session. An edge action
reads that display's state: its Active card, Bento layout and pane cap.

Every display shows its windows as cards, and one holds Spread: the one you
chose while it is attached, else the one a touchscreen drives, else the largest
(`DECISIONS.md` § One display holds Spread). It may contain:

- Individual cards
- Ordinary stacks
- At most one Bento layout

Unplugged, its cards move to the next (§3), returning with the chosen display
(`kaduncerc` `[Spread] Display`).

### Connected

The card display is sized Active or Connected: Connected when its logical size
holds more than two panes (§5), unless chosen for that display (`kaduncerc`
`[Sizing]`). Active is the rest of this document.
On Connected:

- Spread opens zoomed out, every card whole, and each pane is a full card of
  its own; there is no Bento group.
- A card chosen joins the layout: paired with the card in use when none shows,
  else under §8.
- A Stack fills one pane, its other cards hidden behind it. Stepping it, or
  closing its front card, puts the next card in that pane's slot.
- A side snap names a place, never a size: the edge's upper half beside, a
  full-height column at that edge; its lower half under, at the bottom of that
  side's column, what it holds moving up. Alone it takes that side's half, or
  its lower part, the rest open and cards held aside. An arrival fills open
  room, else the layout is laid out again; a leaving pane opens its room.
- A pane leaves as §5 says.

### Every display

Each other display holds its own cards, sized by its size. Spread shows every
card, giving back those not chosen; one picked opens there. Sized Active, it
shows one card: one carried there is that card, closing it brings back the one
before, and a placed Stack steps there. Connected, it plays cards into its
layout. A window opens on Spread's display. Unplugged, its cards stay in
Spread. Let go (§13), windows stay windows until one opens.

`kaduncerc` `[Spread] OtherDisplays=windows` keeps other displays as ordinary
windows or one Bento layout, an edge snap organizing every window there
(`DECISIONS.md` § A display without cards organizes everything it shows).

Switched to zones (Meta+Shift+B or the tray), zones drawn with Meta+T, but
not KWin's default, are the layout: each
window keeps a zone it fits, and KWin owns the edges until release.

Moving a card between displays, with its restoration record, is a transfer under
`ARCHITECTURE.md` § Transfer transaction. A failed transfer changes neither
display.

## 12. Window closure

Closing a window removes its card identity.

### Closing an individual card

The card disappears from Spread.

### Closing a Bento pane

Bento reflows.

If one pane remains, Bento ends and that pane becomes an individual card.

### Closing the Active card

Kadunce returns to Spread when other owned cards remain.

If nothing remains, the display's Kadunce session ends, and the next window to
open there starts ownership again under §3.

## 13. Release and disable

Explicit bottom-edge release affects only the carried window.

Full Kadunce release or disable:

- Restores every owned window, on every desktop, exactly once
- Restores original geometry, clear of a panel's room taken since
- Restores output and virtual desktop
- Restores quick-tile, maximize, fullscreen, and minimized state; a window
  minimized while held stays minimized
- Clears individual cards, stacks, Bento, and presentation state
- Returns complete authority to Plasma

KWin moving the Active card releases everything only once it travels. KWin
placing it on another display, or its application filling the screen, lets
only that card go, where KWin put it. With cards on every display, release gives windows back on their own.

Kadunce does not persist card or Bento membership across unload or restart.

## 14. Required invariants

- One window has one owner.
- One display has at most one Bento layout.
- Bento owns visible panes only.
- A window outside the visible Bento combination is an individual card.
- A Bento pair begins as exactly two named windows.
- An edge gesture decides which side a pane takes; Spread order never does.
- A pair commits with the partner it was prepared for, or does not commit.
- Active is a presentation state, not a separate ownership class.
- Selecting one entry never releases its neighbors.
- Presentation changes never consume or overwrite restoration records.
- Cross-owner transfers follow `ARCHITECTURE.md` § Transfer transaction.
- Failed or cancelled transitions preserve the exact prior state.
- Other displays and desktops remain untouched, except where Table moves a
  window.
- Only explicit release or disable returns a managed window to the native desktop.

