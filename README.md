<p align="center">
  <img src="assets/co.goodinput.kadunce-logo.png" width="180" alt="Kadunce">
</p>

# Kadunce

Kadunce turns your windows into cards you can flip through, pair and arrange by
touch. Your apps stay ordinary Plasma windows, and switching Kadunce off puts
everything back. On the touchscreen you choose windows as cards; on an attached
monitor you arrange them side by side.

## Controls

| Task | Touch | Keyboard |
| --- | --- | --- |
| Spread | Swipe up from the bottom edge | `Meta+S` |
| Active card | Tap a card in Spread | `Enter` in Spread |
| Bento | Drag a window to the left or right edge | `Meta+B` |
| Table | Pull down from the top edge | `Meta+W` |
| Plasma desktop | Switch Cards off in the tray | `Meta+Esc` |

Kadunce's keys work only while it runs. Every other gesture, tap and key is in
[docs/INPUT.md](docs/INPUT.md).

If anything goes wrong, `Meta+Esc` puts every window back on the ordinary Plasma
desktop, and the **Cards** switch in the system tray turns Kadunce off
entirely.

## Cards, Spread, Bento and Table

- **Card:** an application window Kadunce holds. Opening an application adds a
  card; the cards you already have stay as they are.
- **Active card:** the card you are using.
- **Spread:** your cards in an ordered row. Choose one to make it the Active
  card.
- **Stack:** related cards kept together, which you move through one at a time,
  round and round from the card you opened.
- **Bento:** several cards side by side in one layout. In Spread the whole layout
  is one group card, and choosing it brings back the same panes and proportions.
  A window that leaves a layout becomes its own card. Each display has one Bento
  layout at a time.
- **Table:** your workspaces as a row of tabs along the top, each with its cards
  hanging below. Choose one to go there, or carry a card to another. Each
  workspace keeps its own cards and Bento layouts, and one left empty goes away
  unless you named it.

## Displays

Kadunce manages each display on its own. One display holds your cards and
Spread: the one you choose, else the one your touchscreen drives, which Kadunce
finds from the touchscreen and never from the display's name, else the largest.
Every other display stays an ordinary Plasma desktop and can have its own Bento
layout. Unplug the display holding your cards and they all move to the next
one; plug it back in and they return. A window you move to another display
belongs to that display from then on. When
KWin itself puts a card on another display, as a screenshot tool returning to
its last place does, only that window leaves the cards.

Wherever a touchscreen drives a display, Kadunce answers its top and bottom
edges itself, so the swipe up to Spread and the pull down to Table work on any
touchscreen.

## The Cards menu

The **Cards** icon in the system tray is Kadunce's switch; clicking it turns
Kadunce on or off. Its menu holds:

- **Cards on**, the same switch.
- Which KWin Kadunce was built for, and **Repair for current KWin…**, which
  rebuilds it after a KWin update.
- **Monitors use my zones (Meta+T)**, which switches monitors between filling
  themselves and the zones you drew, as `Meta+Shift+B` does.
- **Settings…**, where you set the gutter around the Active card, 6 to 48 px,
  10 px by default.

Every word the menu shows can be translated.

## Requirements

- KDE Plasma 6.7 or newer
- A Wayland session
- Qt 6.10 and KDE Frameworks 6.26, or newer
- KWin development files
- CMake and a C++20 compiler
- `pkexec` for system installation

## Install

```bash
./install.sh
```

The installer checks and builds Kadunce, then asks for permission to install the
KWin plugin. Log out and back in afterwards so KWin loads it, and run it again
after every KWin update; Cards you switched off stay off through it. Kadunce
runs on your distribution's own KWin.
When Kadunce takes a touch drag from KWin, it leaves KWin nothing that loses
the next one.

## Turn off or uninstall

```bash
./disable.sh
./uninstall.sh
```

`disable.sh` returns every window to Plasma and turns Kadunce off until you turn
it on again. The **Cards** switch stays in the system tray, so you can always
turn Kadunce off completely. `uninstall.sh` removes what was installed, the repair
kit and Kadunce's state among it, keeps your settings in `kaduncerc` for a later
install, and leaves this folder as it is; log out and back in if the plugin is
still loaded in the current session.

## What's next

[docs/ROADMAP.md](docs/ROADMAP.md) lists what is planned and what Kadunce does not
do yet.

## Development

Read `AGENTS.md` before working in this repository. The documentation index is in `docs/README.md`.

```text
native/                 KWin effect, window controllers, and native tests
control/                system tray control
tests/                  source, package, control, and session checks
docs/                   product contracts, architecture, and roadmap
install.sh              verified installation
disable.sh              safe release and temporary disable
uninstall.sh            installed component removal
```

Run the repository checks before proposing a change:

```bash
./verify.sh
```

It runs the documentation, source, package and control checks; `install.sh` runs
the source, package and control checks itself before it builds.
`bash tests/verify-headless.sh` runs the native domain tests that do not link
KWin, needing only a C++20 compiler and Qt6Core, as a fast pre-check where the
Plasma development stack is unavailable.

Changes to the compositor also need the private route matrix, a fresh login
session and physical testing. `docs/TESTING.md` describes every check and what it
proves.

## License

Kadunce is licensed under GPL-2.0-or-later. See `LICENSE`.

The project names and marks are not covered by that licence. See
[TRADEMARKS.md](TRADEMARKS.md).
