# Table

Table extends Kadunce's spatial model to KDE Plasma's virtual desktops:

`Active = this window → Spread/Bento = these windows → Table = these workspaces`

A workspace is one KDE virtual desktop. KWin stays authoritative for desktop
membership, switching, lifecycle and persistence; Table is the interaction and
presentation layer over them. Every input Table answers is in `INPUT.md`
§ Table, and who owns each window is in `CARD-LIFECYCLE.md`.

## Workspaces

- Each workspace keeps its own cards, Stacks and Bento layouts. Coming back to
  one shows its Active card or layout, never a Spread left open.
- A workspace spans every display, and its tabs hold its windows from every
  display, a Bento layout's panes each a card. KDE's per-display desktop
  switching is held off while Kadunce runs, without writing your settings, and
  given back when it is switched off.
- Moving a card to another workspace moves its window there. A Stack moves
  whole and is a Stack there, face in front; a pane carried away leaves its
  layout.
- A new workspace is named for the application of the card that made it; one
  made empty with `+` takes its first application's name. The name and KDE's
  desktop number are the whole label.
- A name you give keeps the workspace; an application's does not. Which names
  are yours is kept across restart. Clearing a name offers removal, and the
  windows go where KDE sends them: the next workspace, or the one before the
  last.
- A workspace with no cards dissolves once you leave it unless you named it.
  This holds for desktops made in KDE's settings too, and a window on every
  desktop does not keep one.
- KDE's three-finger sideways swipe stays beside Table, flipping to the
  neighbouring workspace; KDE's desktop-name pop-up is held off while Kadunce
  runs. The dock lists the current workspace's applications.

## Presentation

- The preview is the workspace itself at full size, not a thumbnail. While a tab
  or card is previewed, every display shows that workspace as it presents
  itself, without leaving yours; entering switches desktop.
- Over Spread, the tabs leave Spread in view: a workspace previews once a pull
  reaches its cards, or a tap in the menu bar chooses its tab.
- Table appears where it is called: on the touchscreen for a finger, and on the
  display under the pointer for the mouse and the keys. There is one Table per
  session.
- On the display holding the tabs, the dock's band hangs from the top edge
  behind the rows. It darkens with the finger during a pull, full by the tabs,
  and otherwise fades down into place.
- Tabs are pills carrying the desktop's number, the name and the first few
  application colours; the workspace you are in is the brightest. A crowded row
  gives up the colours, then name length, leaving circles holding numbers. `+`
  is a circle. Cards show the application icon, name and window title; a Stack
  is one card with Spread's closed-stack edges behind it.
- Float: nothing joins the rows and no line is drawn. What the finger is on
  lifts, lighter and a little larger with a soft shadow, and the tab whose cards
  hang stays lifted. Cards come up to meet a finger nearing them, the chosen one
  rises toward the lift line as it is pulled, and the card in hand is the card
  itself.
- The depth and lift lines sit 25 px under the row above them, 5 mm on a
  tablet at 1.75 scale. Accent marks only the drop destination; the rest
  follows `ITASCA-VISUAL-LANGUAGE.md`.
- A screen reader names each piece: a tab as its workspace's number and name,
  `+` as New workspace, a card by its application and title and whether it is
  a Stack, and the card in hand as moving.

## Invariants

- KWin's virtual-desktop APIs are the only membership; there is no parallel
  desktop model or persistence layer.
- Window identity and prepared transfers follow Kadunce's own rules.
- Virtual desktops and physical displays are independent dimensions.
- Active, Spread, Bento, monitor transfer, tablet ownership, the dock,
  restoration and the tray switch behave as they do without Table, and KDE's
  own desktop switching keeps working.
- A rejected or interrupted move leaves the source workspace and its cards as
  they were.

Table is not a desktop grid, a set of miniature Spreads, or a restyled Plasma
Overview, and it replaces neither KDE's virtual desktops nor Activities.
