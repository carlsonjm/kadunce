<p align="center">
  <img src="assets/studio.warbler.kadunce-logo.png" width="180" alt="Kadunce">
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

Kadunce manages each display on its own. The display your touchscreen drives
holds your cards; Kadunce finds it from the touchscreen, never from the display's
name. An attached monitor stays an ordinary Plasma desktop and can have its own
Bento layout. On a computer without a touchscreen, every display uses Bento. A
window you move to another display belongs to that display from then on.

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
KWin plugin. Log out and back in afterwards so KWin loads it. Some system
versions need the KWin touch correction in `patches/kwin/README.md`.

## Turn off or uninstall

```bash
./disable.sh
./uninstall.sh
```

`disable.sh` returns every window to Plasma and turns Kadunce off until you turn
it on again. The **Cards** switch stays in the system tray, so you can always
turn Kadunce off completely. `uninstall.sh` removes what was installed and leaves
this folder as it is; log out and back in if the plugin is still loaded in the
current session.

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
