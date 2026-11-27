# Card lifecycle

Who owns each window, how it is presented, and what every change does to it,
in the order a card lives them. Each change is stated once, with everything that
causes it; `INPUT.md` lists the touch and key behind each. Every way a window
leaves its place names its way back (§7).

## 1. Owners

Each eligible application window has exactly one owner:

1. **Native:** Plasma, outside Kadunce.
2. **Card:** Kadunce, as an individual card: Active, in Spread, sleeping, or
   held out of sight behind what its display presents.
3. **Pane:** Kadunce, as part of its display's visible layout (Bento); in Spread
   a card of its own beside the layout's other panes (§4).

A window is never both a card and a pane. Active, Spread and the others in §4
are presentations, not owners.

## 2. Displays and workspaces

Every display holds its own cards: its own Spread, Active card, Stacks and at
most one layout. A card belongs to the display its window stands on.

`kaduncerc` `[Spread] OtherDisplays=windows` keeps one Spread instead, on the
primary display, moving whole with it, and leaves the other displays as
ordinary windows or one layout, an edge snap organizing every window there
(`DECISIONS.md` § A display without cards organizes everything it shows). Each
section says where that setting differs.

The primary display is your choice while attached, else the touchscreen, else
the largest (`kaduncerc` `[Spread] Display`, kept by identity through
renaming).

### Solo and Connected

Each display is sized Solo or Connected. Solo shows one card at a time over the
work area, and a pair at most. Connected plays a picked card into the display's
layout beside what it already shows. A display is Connected when it has room
for more than two panes, unless chosen (`kaduncerc` `[Sizing]`). Each section
gives both.

Switched to zones (`Meta+Shift+B` or the tray), a display, Connected too, is
laid out in the zones drawn with `Meta+T`, each window in one it fits, a snapped
one at its edge; KWin owns their edges.

### Workspaces

Every virtual desktop has its own cards on every display, begun the first time
it is shown. Nothing done on the workspace shown touches another, except a
window you send there (§9).

### Which display acts

A gesture or an edge action acts on the display it happens on. A key acts on an
open Spread, the display in use first; else on the focused window's display;
else on the display KWin treats as current; else on the primary
(`DECISIONS.md` § A Spread on every display).

Search floats on the display in use, or a desktop clicked; Spread takes it in.

## 3. Becoming a card

### Switching on

Switching Cards on, in the tray or with `Meta+Esc`, makes every eligible window
on every display a card, for the workspace shown. The window in use becomes its
display's Active card; every other eligible window becomes a nonselected card,
each awake card held in the Active card's place. With the windows setting, only
the primary display does this.

Adoption is atomic: one that cannot be prepared safely leaves every window
Native. It happens once; a display holds its cards until its session there ends
(§10).

When a display holds no card, the next application window to open there starts
its cards the same way and becomes Active. A window arriving from another
display does too, carried there or leaving a layout that could not show it; it
arrives as one card and adopts nothing else.

### First edge entry

On a display holding no cards, a top, left or right snap gives the same result:
the carried window becomes the Active card, and every other eligible window
there a nonselected card. On a display kept as windows, the first deliberate
layout action starts ownership instead, and the edge entries still answer a
window carried there.

### Eligible windows

Kadunce adopts normal top-level application windows on the current display and
workspace. It does not make cards of:

- Panels or docks
- Desktop surfaces
- Tooltips or menus
- Temporary popups
- Child dialogs that must follow a parent
- Windows explicitly excluded from normal task switching
- Windows set to appear on every workspace
- Screenshot captures and desktop portal dialogs
- Windows that keep their own size, floating centred; Shuffle Settings at the
  tablet's card size

A dependent dialog follows its application's card. Over that card, or its pane,
it floats at its own size. Behind, it waits hidden with that card and the
application wants attention; brought forward, it shows on top. In Spread a
waiting dialog is drawn on its application's card. A carried card takes its
dialogs, each landing in the same place on it, or its middle when wider, wholly
on the display. Switching Cards off gives every waiting dialog back. A dialog
that names its application late, as Electron's do, follows it from then on.

## 4. What you see

### Active

One card is shown over the work area; the display's other cards stay owned and
hidden. A new window, once drawn, becomes Active over the previous card, now a
neighbour. Applications reopened at sign-in wait unseen; the one last used
shows.

The Active card follows the work area. A panel taking room makes way at once;
room given back is grown into after a wait, except room the card stood in a
moment before, which a panel brought from its edge only borrowed: the card
grows back as that panel leaves.

### Layout

The display's panes are shown; its cards stay owned and hidden. Nothing is
shown on top of live panes (§5, Arrivals).

### Spread

Spread shows every card, every Stack, and each pane of the display's layout as a
card of its own. While Spread is open a new window becomes its selected entry.

Spread has one order. Its entries are cards, Stacks and the cards of the
layout's panes, which stand side by side in the layout's order, left to right,
until you move one. Moving along the row stops at its two ends. Which entry is
selected, and which side a two-entry Spread draws its neighbour on, are
presentation only; they decide neither a partner nor a pane's side.

- **Selecting a card** makes it Active (§5, On Connected). Nothing else
  changes owner, and no window returns to the native desktop.
- **Selecting a pane's card** resumes its layout with that pane in use. A Stack
  holding a pane's card fills that pane with its front card, the rest behind.
  Cards stay owned, hidden and out of it.
- **Returning to Spread** shows the same entries: cards stay cards, Stacks keep
  their members, the layout returns as its panes' cards side by side, and the
  selection stays on the entry presented last.

### Sleeping

A minimized card stays owned but is not shown. Spread draws it dimmed in place,
or at full strength while a context line says it sleeps
(`TETTEGOUCHE-CONTEXT.md` § Context-line claims); `SleepingDimmed` overrides
both. Selecting it, or stepping onto it beside the Active card, wakes it as
Active; on a display showing its layout it joins the layout (§7).

### Desktop

A display that holds cards shows the ordinary desktop, with the window you gave
back to it (§7) or maximized. Every card and pane stay owned, held aside.
Spread brings them back, and going back returns here; choosing a card or
opening an application presents one again, and asking for a given-back window
shows the desktop again.

### Apps that size themselves

Games and apps that Steam, Lutris, Heroic, Prism Launcher, Bolt or Waydroid
start, and RuneLite, keep the size they choose where they open; the launchers'
own windows are ordinary cards. Maximized, in a Stack or as a pane, such a card
fills the work area as any card does; restored, it returns to its own size.
Spread marks them. `kaduncerc` `[SizesItself]` chooses per app or per launcher,
and Search can set it for a window (`TETTEGOUCHE-CONTEXT.md`).

## 5. Laying cards out

### Pairing at a side edge

On a display with cards and no live layout, a left or right snap asks for a
pair of exactly two named windows: the carried window and a partner.

- When the carried window is not the Active card, a window still on the native
  desktop, the partner is the Active card. A card carried in Spread never pairs.
- When it is the Active card, the partner is the card Active just before it,
  from either side. Failing that, it is the nearest eligible card on the
  contacted side in Spread order, walking round from the carried window's
  entry.

The carried window takes the side it was released into, with the share its
half asked for; the partner takes the other. Both become panes and no other
window joins. The partner named at the edge stays the partner through the
carry. With no eligible partner, nothing pairs: the carried window becomes, or
stays, the Active card. A pair commits whole or not at all; one that cannot be
prepared leaves both windows as they were.

A side snap also carries a size intent, larger or smaller, taken from where the
edge was touched; contact near the edge's midpoint keeps the previous choice.

### Who can be a partner

The partner is on the current display and workspace, a card, and awake. The
carried window, a pane's card, a live pane and a native window never are. The
Active card is a partner whether it stands alone or fronts a Stack; pairing
takes a named Stack member out and leaves the rest. A partner search passes
over every entry that fails this test, and every Stack: a card leaves a Stack
for a layout only when you named it by using it.

### Standing alone

The top edge, or `Meta+Shift+Up` on a pane, makes the window the card that fills
its display, never paired: the Active card, or another display's lone card. A
pane takes its Stack with it. On Connected its layout waits in Spread; the dock
or Spread calls it alone, and `Meta+B`, a Stack or an edge lays it out (§7).

### Meta+B

`Meta+B` lays out without a carried window. It belongs to the display in use,
or, with the windows setting, to the largest display without cards, else the
primary. On a Solo display it pairs as a side snap from the right does, the
Active card keeping the left; with no card to pair, nothing happens. On
Connected every card takes a pane, Spread closing first. On a display without
cards it composes across the display. On a display with a live layout it ends
the layout; its windows stay cards.

### Layout rules

Each display has at most one live layout, and it owns only the windows visible
in its panes: never hidden overflow, nor minimized or displaced panes. One pane
limit per display is read by every path: two on Solo; on Connected, what
minimum sizes allow, or its zones. Orientation follows the work area's
proportions.

A layout's shape fixes how panes are arranged, not their proportions. A
preferred proportion is a starting point, clamped by each window's minimum
size, and you then move any shared boundary with its rail. A shape the minimums
cannot satisfy is skipped. Four panes make quarters.

A pane whose application stops short of its place, as a terminal keeping whole
rows does, or one slow to wake, stays: it covers nothing beside it. Only one
that will not fit leaves, and it waits behind the layout, never over it.

### Arrivals at a live layout

On a display showing its layout, every arrival goes into the layout: a new
window, a card you call forward, a card sent from another display or workspace.
An arrival is never parked, and a new window shows once it has drawn its pane,
as the panes draw back.

- If the layout can grow to show it, the arrival becomes a pane.
- If the layout is full, it claims one slot and only that slot's occupant
  leaves, becoming a card; every other pane keeps its window, size and place.
  The slot is the smallest whose size the arrival's minimum permits. The layout
  never reshapes to make room.
- If no slot fits, the arrival becomes the Active card and the layout a
  separate Spread entry; it does not stay on screen behind the card.

A stated intent comes first: a card released into a side edge of a live layout
takes that side, and the pane there yields, shown while dragging; no history
decides it. A layout never begins a second pair. A card called forward names no
side, so the rule above decides.

### Making room while dragging

Held still for a moment over a place in a layout, a carried window opens that
room: the panes move to where the layout would put them, drawn only. Over a
pane, its middle means its Stack and the outer half of a side a new place.
Nothing is placed until the drop; another place or a cancel closes the room.

### On Connected

| Change | Solo | Connected |
| --- | --- | --- |
| A picked card or new window | Becomes Active | One fills the display, two stand as halves, a third goes under the half holding fewest, never a third strip; full, the pane used longest ago yields |
| A side snap | Takes its half's share; unpaired, the rest offers sleeping cards | Names a place: an edge's middle half beside, full height; its bottom quarter under, at that column's foot, its top quarter over; alone, it keeps that place as a layout of one, the wheel its width, its open room offering other cards |
| A dock launch | Opens Active | Names a place, as a side snap does |
| A full layout | The arrival takes the smallest slot it fits | Auto-fill lays all out past minimum sizes, keeping a snapped card's side; else the fitting pane used longest ago yields, never that card |
| A pane leaving | §7 | §7; only a snap's room reopens |

`Meta+Shift+Left` and `Right` move a pane: it swaps with the pane beside, else
takes open room toward that side, else goes to the display beside (§8). On the
Active card alone on Connected, a full card takes the half toward that side, a
half moves into the open half, and at the edge it goes on.

## 6. Stacks

You organize cards into Stacks. A Stack has one front member, stays separate
from a layout, keeps its fan's room wherever it stands (landing on it moves
nothing), shows its position in the label row, and becomes a card when one
member remains. Its members come forward in turn round a ring, which side steps
never leave.

### Making a Stack

A Stack takes no arrival from the native desktop: a carried window becomes a
card (§5), and joining is a separate act:

- A held card dropped on another card, once it has risen; on a pane's card, it
  fills that pane.
- `Meta+Shift+S` on the Active or centred card: it goes in front of the Stack of
  the card used just before it on its display, or with that card makes one.
  Pressed again, it goes one place deeper.
- `Meta+Shift+S` on a pane: with no Stack, it joins the pane used before it;
  with one, its title bars show and it goes in where `Meta` is let go.
- Table, from any workspace, a pane's Stack included (`TABLE.md`).
- An application opened from a card (`REQUESTS.md` § Opened from a card).

### Ordering

A member reordered within its Stack by a drag stays with it, never reaching the
row or an edge, and takes the place it is let go at. `Meta+Shift+PgDown` and
`PgUp` move the front card back or forward, and keep moving that card while it
stays in the Stack. The Stack shows whichever member is then in front.

### In a pane

On Connected a Stack fills one pane, the rest behind; stepping it or closing its
front plays the next card in that slot, and a card called comes forward. A card
or pane rested on a pane, or on the display's lone card, joins its Stack in
front.

### Leaving a Stack

A member pulled down in Spread, or the Active card's `Meta+Shift+Down`, becomes
a card just after the Stack, which keeps its place, order and selection; a
front card gives way to its nearest neighbour. On Solo an Active card let go
stays shown, the Stack waiting in Spread; on Connected the Active card and
the Stack it left are laid out side by side, then the layout fills as `Meta+B`
fills it. A pane's `Meta+Shift+Down` puts its next card in the slot, and the
card let go takes a pane of its own, else waits in Spread. A release that does
not complete leaves the Stack unchanged.

A member given to the desktop (§7) leaves its Stack, and the rest stay one; a
pane's Stack waits together in Spread. A Stack sent to another display or
workspace goes whole (§8, §9).

## 7. Leaving and coming back

| Leaves by | Becomes | Comes back by |
| --- | --- | --- |
| Minimizing | A sleeping card | Waking it: into its place while its layout stands |
| An arrival taking its slot | A card in Spread | Itself, when the arrival leaves; called meanwhile, it takes its slot back |
| The top edge or `Meta+Shift+Up` | The card filling the display | `Meta+Shift+Down`, after the key; else `Meta+B`, a Stack or an edge |
| A pull down in Spread or `Meta+Shift+Down` | A card, or a pane of its own | Calling it forward, or a drop |
| `Meta+Shift+Left` or `Right` past the edge | A card on the display beside | The same keys back: the side it left |
| `Meta+Ctrl+Shift+Left` or `Right`, or Table | A card on that workspace | The same keys, or Table |
| The bottom edge or `Meta+Shift+Backspace` | A window on the Plasma desktop | A snap at the top or a side edge |
| Closing | Nothing | None (§10) |

### A pane leaving its layout

A pane becomes a card when it is taken out, minimized, displaced by an arrival,
no longer fits, or excluded from the visible combination. It keeps no layout
group, and waits in the Active card's place on the display that can hold it as
a card, keeping its restore record so release still returns it where it began.
Where no display can hold a card, nothing leaves: the layout keeps its
combination.

### One remaining pane

When a layout on a Solo display falls to one visible pane, the layout ends and
that pane becomes the Active card. On Connected a card placed alone keeps its
place as a layout of one, its room open; a display kept as windows keeps its
layout too.

### Waking

A minimized card stays a card, sleeping and nonselected, its minimized intent
kept. A minimized pane leaves its layout at once as a sleeping card, and the
layout reflows its other panes. It never returns by itself.

Woken any way but a pick in Spread or a drag, as from the dock, the task
switcher or its application asking, it goes back into its place while that
layout stands. With the same panes awake as when it slept, the layout returns
as it was; else it goes beside the pane it slept next to, on the same side. It
never carries an awake pane across the layout; then, as when its neighbour is
gone, the layout's own rule places it (§5). From the dock it grows out of its
icon. A Solo pair ends when one pane sleeps, so its pane wakes as the Active
card.

### Pushed out

A pane displaced by a window carried or opened in comes back when that window
leaves, if it still waits there unused. Called meanwhile, other than by a pick
in Spread, it takes its slot back and the arrival yields it.

### Standing whole

A pane made whole by `Meta+Shift+Up` returns with `Meta+Shift+Down` to the side
it left, the layout laid out again from Spread around it. One made whole at the
top edge has no key back: Spread, `Meta+B`, a Stack or an edge lays it out.

### To the desktop

The bottom edge, for a carried window, or `Meta+Shift+Backspace`, for the card
or pane in use outside Spread, gives the window back to the Plasma desktop, out
of its Stack or layout. Its display then shows §4's Desktop with it; a last
card ends the display's session, as a last closure does (§10). A snap at the top
or a side edge makes it a card again.

### Cancel

An edge action commits only when released in its zone. A cancelled carry
returns the window as it was, and a cancelled room closes.

## 8. Between displays

A card moved to another display goes there with its Stack in one transfer
(`ARCHITECTURE.md` § Transfer transaction); one that fails changes neither
display. A Stack member the other display cannot take now keeps the whole Stack
here. A held card dropped on another display becomes its card or joins its
layout.

Sent there, it is shown there: in a layout there, else as its Active card. Sent
by `Meta+Shift+Left` or `Right`, it also pairs with a lone card there: the
Active card, or a single half standing beside the desktop. When that card fills
the display, the arrival takes the side facing the display it came from. A pane
the keys carried off remembers its side, and carried home takes it back, with
whichever card is then in front of its Stack.

Sized Solo, a display shows one card: one carried or sent there is that card,
Stack and all; carrying it off brings back the one before. It stays though its
application stops short (§5). Sized Connected, it plays cards into its layout.

Unplugged, a display's cards wait in the Spread of the display KWin moves them
to, else the primary's. Plugged in, a display takes the dock display's cards
and panes, showing the one in use; back within ten seconds, only those KWin
returns.

## 9. Between workspaces

`Meta+Ctrl+Shift+Left` and `Right` send the card in use, or Spread's centred
card, to the workspace beside with its Stack, which is stacked again there; a
pane goes alone, leaving its layout. Table moves cards and Stacks the same way
(`TABLE.md`). You stay where you are: the keys go to what now stands in front,
or the desktop, never to the card sent, so KDE's own keys cannot carry it and
you after it. Only a window on the workspace shown is sent.

The workspace it goes to takes it as a card at once, at the size it will show.
Entering there, a layout takes it in as a window opening there does; else it is
the Active card; a display holding nothing yet starts its cards with it.

## 10. Closing

Closing a window removes its card. A closed card leaves Spread. A closed pane's
slot goes to the next card of its Stack; with none, the layout reflows, ending
at one pane as §7 says. `Delete` in Spread closes the centred card's
application, as a flick up does.

Closing the Active card returns Spread while cards remain. With none left, the
display's session ends, and the next window opening there starts its cards
again (§3).

## 11. Giving everything back

A release to the desktop (§7) affects only that window. Switching Cards off, in
the tray or with `Meta+Esc`:

- Restores every owned window, on every workspace, exactly once
- Restores original geometry, clear of a panel's room taken since; a window
  given back maximized keeps a floating place to restore to
- Restores output and workspace
- Restores quick-tile, maximize, fullscreen and minimized state; a window
  minimized while held stays minimized
- Clears cards, Stacks, layouts and presentation state
- Hands full control back to Plasma

KWin moving the Active card releases everything only once it travels. KWin
placing one card on another display, or its application filling the
screen, lets only that card go, where KWin put it; between displays holding
cards, the other display takes it instead (§8).

Kadunce does not keep cards, Stacks or layouts across unload or restart.

## 12. Rules that always hold

- One window has one owner.
- One display has at most one layout, and it owns visible panes only.
- A window outside the visible layout is a card.
- A side snap pairs exactly two named windows and commits with the partner it
  was prepared for, or not at all.
- A gesture or key decides which side a pane takes; Spread order never does.
- Active is a presentation, not an owner.
- Selecting one entry never releases its neighbours.
- Presentation changes never consume or overwrite restoration records.
- Cross-owner transfers follow `ARCHITECTURE.md` § Transfer transaction.
- Failed or cancelled changes preserve the exact prior state.
- A window reaches another display or workspace only when you, Table or KWin
  move it.
- A window returns to the native desktop only when you give it back, KWin lets
  it go (§11), or Cards switches off.
- Every way out of a place has a way back (§7).
