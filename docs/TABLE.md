# Table

Table extends Kadunce's spatial model to KDE Plasma's virtual desktops:

`Active = this window → Spread/Bento = these windows → Table = these workspaces`

A workspace is one KDE virtual desktop. KWin handles desktop membership,
switching, lifecycle and persistence; Table draws the desktops and handles
your input on them. Table's input is in `INPUT.md` § Table, and who owns each
window is in `CARD-LIFECYCLE.md`.

## Workspaces

- Each workspace keeps its own cards, Stacks and Bento layouts. Coming back to
  one shows its Active card or layout, even if you left Spread open there.
- A workspace spans every display, and its tabs hold its windows from every
  display, a Bento layout's panes each a card. Table leaves out a window kept
  out of the taskbar, the pager or the task switcher, card or not. While
  Kadunce runs it holds off KDE's per-display desktop switching, without
  changing your settings, and gives it back when you switch Kadunce off.
- Moving a card to another workspace moves its window there. A Stack moves
  whole and arrives as a Stack, face in front. A pane carried away leaves its
  layout.
- A new workspace is named for the application of the card that made it; one
  made empty with `+` is New, in Table and on the context line, until its
  first application names it, and Table stays open on its tab. The name and
  KDE's desktop number are the whole label. A fresh install's first workspace
  is Home.
- With saved Layouts, `+` hangs Empty workspace, which makes New, and each
  Layout as a card, its desk picture rising when chosen. A Layout opens
  through Split Rock in a new workspace of its name; refused, that goes again.
- A name you give keeps the workspace; an application's does not. Which names
  are yours survives a restart. Clearing a name offers removal, and the
  windows go where KDE sends them: the next workspace, or the one before the
  last.
- A workspace with no cards dissolves once you leave it unless you named it,
  and the context line says so while you are in it.
  This holds for desktops made in KDE's settings too, and a window on every
  desktop does not keep one. A desktop made elsewhere while Kadunce runs stays
  until you have entered and left it. Signing out never dissolves one, nor
  does leaving one that a remembered session may yet bring applications back
  to (`REQUESTS.md` § Bringing apps back).
- KDE's three-finger sideways swipe still flips to the neighbouring
  workspace. Kadunce holds off KDE's desktop-name pop-up while it runs. The dock
  lists the current workspace's applications.

## Presentation

- The preview is the workspace itself, at full size, on every display, while
  you stay put; entering it switches desktop.
- Over Spread, the tabs leave Spread in view: a workspace previews once a pull
  reaches its cards or a menu-bar tap chooses its tab.
- Table opens where you call it: on the touchscreen for a finger, under the
  pointer for the mouse and keys. A session has one Table.
- On the display holding the tabs, the dock's band hangs from the top edge
  behind the rows, darkening with a pull, full by the tabs, else fading down
  into place.
- Under the band, that display is frosted with the glass behind Shuffle
  Settings (`ITASCA-VISUAL-LANGUAGE.md` § Kadunce geometry), its panels
  standing over it, whichever workspace shows. The glass clears while a card
  is previewed, and Table over Spread frosts nothing.
- Tabs are pills carrying the desktop's number, name and first few
  application colours; the workspace you are in is the brightest. A crowded row
  gives up the colours, then name length, leaving circles holding numbers. `+`
  is a circle. Cards show the application icon, name and window title, less
  any repeat of the name; a Stack is one card with Spread's closed-stack edges
  behind it; rested on, its cards hang under it.
- The rows float apart, unlined. What the finger is on lifts, lighter, larger
  and softly shadowed, as does the tab whose cards hang. Cards rise to meet a
  nearing finger, the chosen one toward the lift line; the card in hand is the
  card itself; held on another, Spread's hold ring fills.
- The depth and lift lines sit 25 px under the row above them, 5 mm on a
  tablet at 1.75 scale. Accent marks only the drop destination; the rest
  follows `ITASCA-VISUAL-LANGUAGE.md`.
- A screen reader names each piece: a tab as its workspace's number and name,
  `+` as New workspace, a card by its application and title and whether it is
  a Stack, a hung card with its place, and the card in hand as moving.

## Invariants

- Membership comes only from KWin's virtual-desktop APIs. Kadunce keeps no
  parallel desktop model or persistence layer.
- Virtual desktops and physical displays are independent dimensions.
- Active, Spread, Bento, monitor transfer, tablet ownership, the dock,
  restoration and the tray switch behave as they do without Table, and KDE's
  own desktop switching keeps working.
- A rejected or interrupted move leaves the source workspace and its cards as
  they were.

Table is not a desktop grid, a set of miniature Spreads or a restyled Plasma
Overview, and it replaces neither KDE's virtual desktops nor Activities.
