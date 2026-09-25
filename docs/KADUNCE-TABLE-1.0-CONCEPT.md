# Table product contract

**Status:** Required for Shuffle 1.0. Interaction and visual direction approved
by J on 25 September. KDE's virtual-desktop APIs were audited from source the
same day and support every part of it (`ROADMAP-CONTEXT.md` § Block 8); the
runtime proofs are next.

Table extends Kadunce's spatial model to existing KDE Plasma virtual desktops:

`Active = this window → Spread/Bento = these windows → Table = these workspaces`

KWin remains authoritative for virtual-desktop membership, switching, lifecycle,
and persistence. Table is the interaction and presentation layer. A workspace is
one KDE virtual desktop.

`table-prototype.html` is the approved interactive reference. It is a browser
mock and does not define timing or geometry beyond what this document states.

## The gestures

The same three gestures work in Active, Bento, Spread and Table:

- Top-edge downward opens Table.
- Bottom-edge upward opens Spread of the current workspace.
- A tap returns to work: a card becomes Active in its own workspace, and a tap
  anywhere else returns to where the person was.

## Scrubbing the tabs

Table is one stroke that does not need the finger to lift. Pull distance chooses
the level; sideways movement chooses the item on that level.

1. A short pull from the top edge brings the workspaces down as a row of tabs
   under the edge, in KWin's desktop order.
2. Sliding sideways over the tabs previews each workspace. The whole column under
   a tab belongs to it, so the finger never has to land on the tab itself.
3. Pulling past a depth line locks the tab under the finger, and that
   workspace's cards hang below it as a second row. Sliding sideways previews
   each card.
4. Pushing back above the line returns to the tabs. Pushing back to the edge and
   lifting cancels, and nothing changes.
5. Lifting on a tab enters that workspace as it was left. Lifting on a card makes
   that card Active in its workspace.

A quick flick that releases before any scrub leaves the tabs open as a menu bar.
Then a tap or a mouse hover previews, a second tap enters, and a tap outside
closes. With a keyboard, left and right choose, down and up change level, Enter
commits and Escape cancels. The shortcut that opens Table is not yet chosen.

## Moving a card

Resting on a card at the card level lifts it. Carrying it back up to the tabs
and releasing on a tab moves the window to that workspace; releasing on the `+`
tab at the end of the row creates a workspace and moves it there. The tab under a
carried card shows the destination before release. Releasing a lifted card on
its own place opens it. Releasing at the edge cancels.

## Workspaces and names

- A new workspace is named after the application of the card that created it.
  A double tap on a tab renames it. The name is the whole label system: no
  numbers, colours or tags.
- A workspace that holds no cards dissolves on its own unless it is pinned. The
  workspace the person is in stays until they leave it.
- A workspace spans every display. Its tab row holds its windows from every
  display, and a Bento layout is one grouped card, as in Spread. KDE's
  per-display desktop switching stays off: with it on, each display has its own
  current desktop and a window moved between displays changes desktop. J
  turned it off on 25 September and ruled that the setting should not exist
  for Shuffle.

## Presentation

- The preview is the workspace itself at full size, not a thumbnail. While a tab
  or card is previewed, every display shows that workspace, under a light shade
  on the display holding the tabs.
- Table appears on the display that holds the dock: the touchscreen, or a monitor
  set as the dock's primary display. There is one Table per session, not one per
  display.
- Tabs are pills carrying the name, the first few application colours, and a
  pin. The current workspace carries a dot. Cards in the second row show the
  application icon, name and window title, joined to their tab by a thin stem.
- Table is not a desktop grid, a set of surfaces or miniature Spreads, and it is
  not a restyled Plasma Overview.
- Visual and motion grammar follow `ITASCA-VISUAL-LANGUAGE.md`. Accent marks only
  the drop destination.

## Invariants

- Use KWin's virtual-desktop APIs; do not create parallel desktop membership.
- Reuse Kadunce's window identity and prepared-transfer rules.
- Keep virtual desktops and physical outputs as independent dimensions.
- Preserve Active, Spread, Bento, monitor transfer, tablet ownership, dock
  behavior, restoration, and the disable control.
- Normal KDE desktop switching must continue to work.
- A rejected or interrupted transfer preserves the source desktop and card state.

## Feasibility gate

Before product implementation:

- ~~audit KWin/Plasma virtual-desktop APIs, including creating and removing a
  desktop and painting a non-current desktop at full size on every display~~
  (from source, 25 September; the painting still needs its runtime proof);
- define state ownership across desktop switch, output change, client close,
  effect unload, and session restore, and where a pin is kept;
- prove one value-first transfer on a private prototype;
- physically test the scrub, the depth line, a card transfer, and cancel;
- verify the underlying desktop membership through KDE independently of Table.

## Open

- Where the depth line sits; the prototype uses 30% of the display height.
- How the tab row holds more workspaces than fit across.
- How strongly the preview is shaded.
- The shortcut that opens Table.
- How Shuffle keeps per-display desktop switching off.
- KDE's own three-finger touchscreen swipe and desktop-name pop-up, beside
  Table.

## 1.0 acceptance

On a supported touch device, the user can pull down Table, scrub existing KDE
virtual desktops and their cards, move a real Kadunce-managed window between
them, create a workspace by dropping a card on `+`, lift into the destination,
and observe correct underlying membership. Existing Kadunce and KDE behavior
remains intact.

## Non-goals

- Replacing KDE virtual desktops, KWin Overview, or Activities.
- A second window manager or persistence layer.
- Reopening stable Kadunce ownership architecture.
