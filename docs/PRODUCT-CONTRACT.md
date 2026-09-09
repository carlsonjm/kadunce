# Kadunce Product Contract

## North star

Desktop applications remain real desktop applications. Kadunce gives
them a touch-native spatial shell without replacing KWin's authority over
window lifecycle, focus, snapping, displays, and recovery.

- The tablet is the attention stage: choosing and focusing.
- The external display is the desktop stage: seeing and composing.
- Neither stage imitates the other.
- Applications are cards.
- Stacks are visibly ordered piles.
- Card Line is compositor space, not extended-desktop geometry.
- Moving between outputs is a committed handoff, not background ownership.
- Release always returns safe ordinary Plasma windows.

## Tablet stage

The built-in touch display can present Active, Card Line, and undocked Bento.

- **Active** is one fixed, interactive application card.
- **Card Line** is an ordered compositor view with one centered card and two
  partial neighbors. It never moves real windows into off-screen positions.
- **Bento** uses real geometry for simultaneous interaction and parks overflow
  rather than violating application minimum sizes.

## Desktop stage

An external output presents ordinary Plasma windows or per-output Bento. It
does not run a second Card Line. It accepts deliberate tablet handoffs and
delegates ordinary movement, resizing, focus, and snapping to KWin.

## Release

Release is a command, not a persistent layout mode. Per-output release restores
every managed client inside that output's usable area. Global disable releases
all outputs before unloading Kadunce.

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

- Bottom-edge upward: Card Line
- Top-edge downward: Active
- Active left/right edge: previous/next card
- Card Line background swipe: page line
- Stack vertical gesture: previous/next member
- Long hold: opaque lifted card with stable pointer attachment
- `Ctrl+S`: tablet Active/Card Line
- `Ctrl+B`: external Bento while docked, tablet Bento while undocked
- `Ctrl+Arrow`: equivalent card and stack navigation
- `Ctrl+Esc`: release the output under the pointer

Gestures belong to Kadunce only when they begin in a reserved system edge. Plain
application keyboard and touch input remain with the application.

## Versioning

Repairs, polish, and missing regression fixes remain patch revisions. A
genuinely new product capability advances the minor version.
