# Kadunce Product Contract

## North star

Desktop applications remain real desktop applications. Kadunce gives
them a touch-native spatial shell without replacing KWin's authority over
window lifecycle, focus, snapping, displays, and recovery.

- The display a touchscreen drives is the attention stage: choosing and
  focusing.
- Any other display is the desktop stage: seeing and composing.
- Neither stage imitates the other.
- Applications are cards.
- Stacks are visibly ordered piles.
- Spread is compositor space, not extended-desktop geometry.
- Moving between outputs is a committed handoff, not background ownership.
- Release always returns safe ordinary Plasma windows.

## Attention stage

The display a touchscreen drives presents Active, Spread, and Bento. Cards go
there because of that capability, never because of a display's name or hardware
identity. A machine with no touchscreen has no attention stage and gets Bento
only.

- **Active** is one fixed, interactive application card.
- **Spread** is an ordered compositor view with one centered card and two
  partial neighbors. It never moves real windows into off-screen positions.
- **Bento** uses real geometry for simultaneous interaction.

`CARD-LIFECYCLE.md` is authoritative for card ownership, admission, selection,
minimization, Bento membership, and what a release restores.

## Desktop stage

Any other display presents ordinary Plasma windows or one Bento layout, never
both, and never cards or Spread. One window snapped to a side takes half the
display, and snapped to the top takes the Active card's size. With two or more
windows, a snap to an edge organizes every window there into one layout, as many
as their minimum sizes allow. A window without room goes to the dock and never
to another display on its own. Carrying a card here from the attention stage is
a deliberate handoff; ordinary movement, resizing and focus stay with KWin.
`DECISIONS.md` § A display without cards organizes everything it shows.

## Release

Release is a command, not a persistent layout mode. Per-output release restores
every managed client inside that output's usable area. Global disable releases
all outputs before unloading Kadunce. `CARD-LIFECYCLE.md` §13 lists what a
release restores.

## System control

The persistent Plasma tray item is an out-of-process control surface, never a
second workspace authority. Its single checked switch loads the installed
native effect when enabled. Disabling first requires KWin to confirm a safe
effect unload and persists the disabled state. If KWin cannot confirm the
release, the control restores the enabled configuration instead of risking a
half-disabled workspace. A future
Settings window may extend this helper without adding preference state to the
compositor plugin.

## Input

- Bottom-edge upward: launch Card Spread
- Top-edge downward: Table (`KADUNCE-TABLE-1.0-CONCEPT.md`). Until Table ships
  in Block 8 it returns to the current card state: the Active card, a Stack, or
  the resumed Bento layout
- Tap in Spread or Table: back to work, the tapped card Active in its own
  workspace
- Active left/right edge: previous/next card
- Spread background swipe: move through the Spread
- Stack vertical gesture: previous/next member
- Long hold: opaque lifted card with stable pointer attachment
- `Ctrl+S`: Active/Spread on the attention stage
- `Ctrl+B`: Bento on the largest desktop-stage display while one is attached,
  otherwise on the attention stage
- `Ctrl+Arrow`: equivalent card and stack navigation
- `Ctrl+Esc`: release the output under the pointer

Gestures belong to Kadunce only when they begin in a reserved system edge. Plain
application keyboard and touch input remain with the application.

## Versioning

Repairs, polish, and missing regression fixes remain patch revisions. A
genuinely new product capability advances the minor version.
