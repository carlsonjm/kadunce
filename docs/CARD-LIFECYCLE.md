# Card lifecycle

Who owns each window, how it is presented, and what every transition does to
it. `INPUT.md` lists the input behind each transition.

## 1. Core model

Each display is sized Solo, one card at a time over the work area and a Bento
pair at most, or Connected, a picked card joining its layout (§11).
§3–§10 describe Solo; §11 lists every difference on Connected.

Each eligible application window has one of three owners:

1. **Native**

   - Plasma owns the window, outside Kadunce.

2. **Individual card**

   - Kadunce owns the window as an independent card.
   - It may be Active, visible in Spread, sleeping, or hidden behind another
     selected state.

3. **Bento pane**

   - Kadunce owns the window as part of the display's visible Bento.
   - In Spread it is a card of its own, beside the layout's other panes
     (§6, Spread order).

A window is never an individual card and a Bento pane at once.

## 2. Presentation states

Ownership and presentation are separate.

### Active

One individual card is shown; other cards stay owned and hidden.

### Bento

The display's current Bento pane combination is shown; individual cards stay
owned and hidden.

### Spread

Spread shows:

- Every individual card
- Every ordinary card stack
- Each pane of the display's Bento layout, as a card of its own

### Sleeping

A minimized individual card stays owned but is not shown. Spread draws
it dimmed in its place, or at full strength while a context line says it sleeps
(`TETTEGOUCHE-CONTEXT.md` § Context-line claims). Selecting it wakes it and
presents it as Active.

### Desktop

The display that owns cards shows the ordinary desktop, with the window you
returned to it at its bottom edge (§10) or by maximizing it. Every card and
pane stay owned, held aside out of sight and reach. Spread brings them
back, and going back returns here; choosing a card or opening an application
presents one again, and asking for a returned window shows the desktop again.

### Native desktop

The window is released, and Kadunce neither owns nor hides it.

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
- No other window joins.

The partner is named when the edge is contacted and stays the partner, whatever
the presentation does during the carry.

With no eligible partner, nothing pairs: the carried window becomes an
individual Active card, or stays one.

An entry commits completely or not at all. If adoption cannot be prepared safely,
every window stays Native. If a pair cannot be prepared safely, the carried
window and the partner keep the ownership and presentation they had.

### The Bento action

A Bento action is a request without a carried window or a contacted edge
(`INPUT.md` § Bento, `Meta+B`). It belongs to the display in use with cards on
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
- Windows that keep their own size, like Shuffle Settings, floating centred

A dependent dialog follows its owning application card. Over that card, or
over the pane its application holds, it floats at its own size. When that card
is not in front, the dialog waits with it, hidden, and
the application is marked as wanting attention; bringing the application
forward shows the dialog on top. In Spread a waiting dialog is drawn on its
application's card. A card carried away
takes its dialogs along, each landing in the same place on its application, or
on its middle when wider than it, and wholly on the display.
Switching Kadunce off gives every waiting dialog back. A dialog that names its
application only once it is shown, as Electron's message boxes do, follows it
from then on, even if Kadunce first took it for a card.

### Eligible as a Bento partner

The partner is the card Kadunce pairs with the carried window. It must be:

- On the current display
- On the current virtual desktop
- Owned as an individual card
- Not sleeping

The carried window, a pane's card, a live pane and a native window are never
partners.

The Active card is a partner whether it stands alone in the Spread or is the
selected member of an ordinary stack. Pairing takes a named stack member out of its
stack and leaves the rest of the stack unchanged, whether that member is the
partner or the carried window.

A partner search passes over every Spread entry that fails this test, and over
every ordinary stack. A card leaves a stack for Bento only when you named it
by using it, not because a search walked past it.

## 5. Bento rules

Each display can have only one live Bento layout.

Bento owns only the windows visible in its panes: never hidden overflow, nor
minimized or displaced panes.

### Pane limit

Each display has one maximum visible pane count, read by every admission path.

The tablet's maximum is two; a larger display, what minimum sizes allow, or its
zones (§11).

Layout orientation follows the work area's own proportions, not the display's
hardware identity.

### Shape, not size

A preset fixes how panes are arranged, not their proportions. A preferred
proportion is a starting point, clamped by each window's minimum size, and the
user then moves any shared boundary with its rail. A shape the minimums cannot
satisfy is skipped.

### Choosing a shape by side contact

A side snap carries a size intent, larger or smaller, taken from where the edge
was touched (`INPUT.md` § Active card and § Bento). Contact near the edge's midpoint
keeps the previous choice.

The carried window keeps the edge it was released into and takes the share its
half asked for (§11, A side snap), wherever the carried window comes from.

### When a pane leaves Bento

The window becomes an independent Spread card when it is:

- Taken out by you, at the top edge or by a pull down on its card in Spread
- Minimized
- Displaced by a new pane combination
- Removed because it no longer fits
- Excluded from the current visible layout
- Moved to another workspace

It keeps no Bento position or group, and stands in the Active card's place (§3).
One displaced by a window carried or opened in comes back when
that window leaves, if it still waits there unused.

A window leaves for the display that can hold it as a card, keeping its
restore record so release still returns it where it began. It waits in
Spread there.

Where no display can hold a card, nothing leaves: the layout keeps its
combination.

A pane whose application stops short of its place, as a terminal keeping whole
rows does or one slow to wake answering late, stays: it covers nothing beside
it. Only one that will not fit its place leaves, and one that leaves a layout
still presenting panes waits behind it, never shown over it (§8).

### One remaining pane

When Bento falls to one visible pane, Bento ends and that pane becomes an
individual card.

### Returning a card to a live Bento

A display with a live Bento layout does not begin a new pair. A left or right
snap targets that layout.

An individual card returns to a live Bento through an explicit left/right-edge
action, or by the user calling it forward.

If the Bento combination is full, the displaced pane becomes an individual card.
Where the return was a side release, the pane that yields is the one occupying
the side the card was released into. You see which pane will yield while dragging; no
history decides it.

A card called forward releases into no side, so §8 decides which pane yields.
Every display accepts a call forward, but not every display offers the side
gesture.

## 6. Spread selection

### Spread order

The Spread has one order. Its entries are individual cards, ordinary stacks and
the cards of the display's Bento panes, which stand side by side in the
layout's order, left to right, until you move one. The row that shows it has
two ends, and moving along it stops at them. When the card used before the Active card cannot pair, that
order names a Bento partner, the search wrapping and stepping over entries until
back where it started.

Which entry is selected, and which side a two-entry Spread draws its neighbour
on, are presentation only. They decide neither the partner nor a pane's side.

### Selecting an individual card

That card becomes Active (§11, A picked card or new window). Everything
else stays owned and hidden; no window returns to the native desktop.

### Selecting a pane's card

Its layout resumes, that pane in use. A Stack holding a pane's card fills
that pane with its front card, the rest behind. Individual cards stay owned,
hidden and out of it.

### Returning to Spread

The same peer entries return:

- Independent cards stay independent.
- Ordinary stacks keep their membership.
- Bento returns as its panes' cards, side by side.
- Selection stays on the most recently presented entry.

## 7. Minimize behavior

### Minimizing an individual card

- It stays an individual card.
- It becomes sleeping and nonselected.
- Its user-minimized intent is preserved.

### Minimizing a Bento pane

- It immediately leaves Bento.
- It becomes a sleeping individual card.
- Bento reflows its remaining visible panes.
- Selecting the sleeping card wakes it as Active.

A minimized card never silently returns to Bento.

## 8. Arrivals at a live Bento

On a display showing its Bento layout, every arrival goes into that layout. An
arrival is a new window, or an individual card you call forward. Nothing is
shown on top of live panes.

### Admitting an arrival

- If the layout can grow to show it, Bento recomposes and the arrival becomes a
  pane.
- If the layout is full, a pane yields and becomes an individual card, and the
  arrival takes its place.
- If no pane the layout can offer satisfies the arrival's minimum size, the
  arrival becomes an individual Active card and the layout becomes a separate
  Spread group. It does not stay on screen behind the card.

An arrival is never parked.

### Which pane yields

The arrival claims one slot, and only that slot's occupant leaves. Every other
pane keeps its window, its size and its place.

The slot is the smallest one whose size the arrival's minimum size permits. A
window that fits only the wide pane takes the wide pane; one that fits either
takes the narrower and leaves the wide pane alone (§11, A full layout).

The layout does not reshape to make room. Where no slot can hold the arrival,
the rule above for one that fits nothing applies.

A stated intent comes first: when you release a card into a side edge, §5
gives the yielding pane to that side.

### While an individual card is Active

The new window becomes a new individual Active card.

The previous Active card stays an individual Spread neighbor.

### While Spread is open

The new window becomes an individual card, the selected Spread entry.

## 9. Ordinary stacks

Individual cards may be organized into ordinary Spread stacks.

A stack:

- Has one selected member
- Stays separate from Bento
- Brings its members forward in turn, a ring side steps never leave
- Keeps its fan's room wherever it stands; landing on it moves nothing
- Can release a member back into the Spread
- Becomes an individual card when one member remains
- Shows its active position in the label row
- Takes no arrival from the native desktop: a carried window becomes a card
  under §8, and joining a stack is a separate act

### Releasing a member

A released member becomes an individual card just after the stack, which keeps
its place, its order and the selection; a released front card gives way to its
nearest neighbour. A release that does not complete leaves the stack unchanged.
A pane's card leaves its layout the same way.

### Reordering a member

A member reordered within its stack stays with it and never reaches the row or
an edge. It takes the place it is let go at, counted from the front, and the
stack then shows whichever member is in front; let go where it began, the stack
is unchanged.

## 10. Edge actions

An edge action is what a carried window's release at a screen edge means
(`INPUT.md` § Active card and § Bento).

### Top edge

Make the carried window an Active card, or another display's lone card, never paired. Connected, panes it left wait in Spread; the dock or Spread calls it alone, and `Meta+B`, a Stack or edge lays it out.

### Left or right edge

§3 covers a side snap (§11, A side snap), and §5 one into a live
Bento layout.
Spread's side edges on the display that owns cards are not edge actions.

### Bottom edge

Release the carried window to the Plasma desktop.

On any display showing cards (§11), §2's Desktop is then shown with it; a last
card ends the display's session, as §12's last closure does.

### Cancel

A cancelled carry returns the window as it was.

An edge action commits only when released in its zone.

## 11. Multiple displays

Each display, per desktop, has its own card stage: Spread, Active card,
Stacks and at most one Bento layout. A card belongs to its window's
display. An edge action, a key or a gesture reads the display it
happens on; a key reads an open Spread, else the focused window's display,
else the primary (`DECISIONS.md` § A Spread on every display).

The primary display is your choice while attached, else the touchscreen,
else the largest (`kaduncerc` `[Spread] Display`, kept by identity through
renaming). Unplugged, a display's cards wait in the Spread of the display
KWin moves them to, else the primary's, whose own cards stay.

Search floats on any display; an open Spread takes it in.

### Connected

A display is Connected when its logical size holds more than two panes (§5),
unless chosen for it (`kaduncerc` `[Sizing]`, likewise).

| Behavior | Solo | Connected |
| --- | --- | --- |
| A picked card or new window | Becomes Active (§6, §8) | Joins the layout: paired with the card in use when none shows, else under §8, filling open room, else joining the shortest column or starting one, whichever keeps panes larger |
| A side snap | Takes its half's share (§3) | Names a place: an edge's middle half beside, full height; its bottom quarter under, at that column's foot, its top quarter over; alone, it keeps that place, the wheel its width, its open room offering other cards |
| A dock launch | Opens Active | Names a place, as a side snap does |
| A Stack | Steps in place (§9) | Fills one pane, the rest behind; stepping or closing its front plays the next, and a card called comes forward. A card or pane rested on a pane or the tablet's card joins its Stack in front |
| A full layout | An arrival takes the smallest slot it fits (§8) | Auto-fill lays all out past minimum sizes, keeping a snapped card's side; else the fitting pane used longest ago yields, never that card |
| A pane leaving | §5 | §5, its room opening |

### Every display

Sized Solo, a display shows one card: one carried or sent there is that card,
Stack and all; closing or carrying it off brings back the one before. It stays
though its application stops short (§5). Sized Connected, it plays cards into
its layout. A window opens as a card where KWin places it. A card moved to
another display goes there with its Stack in one transfer, joining a live
layout, else Active. Spreads open
together each show their own cards. Let go (§13), windows
stay windows until one opens.

`kaduncerc` `[Spread] OtherDisplays=windows` keeps one Spread, on the primary
display, moving whole with it, and other displays as ordinary windows or one
Bento layout, an edge snap organizing every window there
(`DECISIONS.md` § A display without cards organizes everything it shows).

On a display switched to zones (Meta+Shift+B or the tray), the zones drawn
with Meta+T are the layout, in place of KWin's default. Each window keeps a
zone it fits, and KWin owns the edges until release.

A transfer (`ARCHITECTURE.md` § Transfer transaction) that fails changes
neither display.

## 12. Window closure

Closing a window removes its card identity.

### Closing an individual card

The card disappears from Spread.

### Closing a Bento pane

Bento reflows, ending at one pane (§5).

### Closing the Active card

Spread returns while owned cards remain.

If nothing remains, the display's session ends; the next window opening there
starts ownership again (§3).

## 13. Release and disable

Explicit bottom-edge release affects only the carried window.

Full Kadunce release or disable:

- Restores every owned window, on every desktop, exactly once
- Restores original geometry, clear of a panel's room taken since
- Restores output and virtual desktop
- Restores quick-tile, maximize, fullscreen, and minimized state; a window
  minimized while held stays minimized
- Clears individual cards, stacks, Bento, and presentation state
- Hands full control back to Plasma

KWin moving the Active card releases everything only once it travels. KWin
placing it on another display, or its application filling the screen, lets
only that card go, where KWin put it. With cards on every display, windows go
back on their own.

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
- Other displays and desktops stay untouched, except where Table moves a
  window.
- Only explicit release or disable returns a managed window to the native desktop.

