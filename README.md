<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/readme/banner-dark.png">
    <img src="assets/readme/banner-light.png" alt="Kadunce: your windows as cards you flip through, pair and arrange by touch" width="100%">
  </picture>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/KDE_Plasma-6.7%2B-1d99f3?style=flat" alt="KDE Plasma 6.7+">
  <img src="https://img.shields.io/badge/session-Wayland-555555?style=flat" alt="Wayland session">
  <img src="https://img.shields.io/badge/input-touch%2C_mouse%2C_keys-6a5acd?style=flat" alt="Touch, mouse and keyboard">
  <img src="https://img.shields.io/badge/license-GPL--2.0--or--later-blue?style=flat" alt="GPL-2.0-or-later">
</p>

<p align="center">
  <a href="#install">Install</a> · <a href="#controls">Controls</a> · <a href="docs/README.md">Docs</a> · <a href="https://shuffleplasma.com">Part of Shuffle</a>
</p>

<p align="center">
Kadunce turns your windows into cards you can flip through, pair and arrange by
touch, mouse or keyboard. Switch it off and everything goes back to the Plasma
desktop.
</p>

<p align="center"><img src="assets/readme/showcase.webp" alt="Spread: the cards of every display in one row, the Active card in the middle" width="100%"></p>
<p align="center"><sub>Spread</sub></p>

## What you get

<table>
  <tr>
    <td width="33%" valign="top"><b>Cards</b><br>Every application window is a card, and opening an application adds one.</td>
    <td width="33%" valign="top"><b>Spread</b><br>Your cards in one ordered row. The one you choose becomes the Active card, the card you are using.</td>
    <td width="33%" valign="top"><b>Stacks</b><br>Related cards kept together. You step through them one at a time, round and round from the card you opened.</td>
  </tr>
  <tr>
    <td width="33%" valign="top"><b>Bento</b><br>Several cards side by side in one layout. In Spread each pane is its own card, and choosing one brings the layout back.</td>
    <td width="33%" valign="top"><b>Table</b><br>Your workspaces as tabs along the top, each with its cards hanging below. Go to one, or carry a card to another.</td>
    <td width="33%" valign="top"><b>Auto layout</b><br>On a big display, cards lay themselves out side by side. Snap one to an edge to place it, and the rest fill in around it.</td>
  </tr>
</table>

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

If anything goes wrong, `Meta+Esc` puts every window back on the Plasma
desktop, and the **Cards** switch in the system tray turns Kadunce off.

## Install

```bash
./install.sh
```

The installer checks and builds Kadunce, then asks for permission to install the
KWin plugin. Log out and back in afterwards so KWin loads it. Kadunce runs on
your distribution's own KWin.

<details>
<summary>Requirements</summary>

- KDE Plasma 6.7 or newer
- A Wayland session
- Qt 6.10 and KDE Frameworks 6.26, or newer
- KWin development files
- CMake and a C++20 compiler
- `pkexec` for system installation

</details>

<details>
<summary>After a KWin update</summary>

Run `./install.sh` again after every KWin update, or choose **Repair for
current KWin…** in the Cards menu. If you had Cards switched off, they stay off.

</details>

<details>
<summary>Turn off or uninstall</summary>

```bash
./disable.sh
./uninstall.sh
```

`disable.sh` returns every window to Plasma and turns Kadunce off until you turn
it on again with the **Cards** switch in the system tray. `uninstall.sh` removes
what was installed, including the repair kit and Kadunce's state. It keeps your
settings in `kaduncerc` for a later install and leaves this folder alone. If the
plugin is still loaded afterwards, log out and back in.

</details>

## Displays

Every window is a card, on every display. A small display shows one card at a
time, or a Bento pair. A big display lays its cards side by side, and a card you
pick joins what is already open.

One display holds Spread: the one you choose, else the one your touchscreen
drives, else the largest. Spread lists the cards of every display, and a card
you pick there or in the dock opens on its own display.

### Auto layout on a big display

Drag a window to the upper half of a side edge and it takes a full-height
column on that side. The lower half puts it under whatever is already on that
side. With one card open, the next window you open pairs with it. New windows
fill the open room, or join the shortest column, or start a new one, whichever
keeps every window larger. Once the screen is full, Kadunce lays everything out
evenly and keeps your snapped card on its side. If even that cannot fit, the
window you used longest ago steps back into Spread. Drag a divider to fine-tune.

### Prefer your own layout?

If auto layout isn't your thing, keep a display as ordinary windows (below),
draw zones for it with `Meta+T` and switch to them with `Meta+Shift+B`.

<details>
<summary>More about displays</summary>

- A window you carry to another display belongs to that display from then on.
- Unplug the display holding Spread and its cards move to the next one; plug it
  back in and they return. Unplug another display and its cards stay in Spread.
- Search floats on any display, and joins Spread once Spread is open.
- The swipe up to Spread and the pull down to Table work on any display a
  touchscreen drives.
- You can keep other displays as ordinary windows instead, with
  `kaduncerc` `[Spread] OtherDisplays=windows`. There an edge snap arranges
  every window into one Bento layout, or into your zones.

</details>

<details>
<summary>More about cards, Bento and Table</summary>

- A Bento pane's card in Spread sits beside its partners in the layout's order
  until you move it. A window that leaves a layout becomes its own card. Each
  display has one Bento layout at a time.
- Each workspace in Table keeps its own cards and Bento layouts. An empty one
  goes away unless you named it.

</details>

## The Cards menu

Click the **Cards** icon in the system tray to turn Kadunce on or off.

<details>
<summary>What the menu holds</summary>

- **Cards on**, the same switch.
- Which KWin Kadunce was built for, and **Repair for current KWin…**, which
  rebuilds it after a KWin update.
- **Monitors use my zones (Meta+T)**, which switches a display kept as ordinary
  windows between filling itself and your zones, like `Meta+Shift+B`.
- **Settings…**, where you set the gutter around the Active card, 4 to 48 px,
  10 px by default. Once you set it, Bento's panes stand that far apart too.

Every word the menu shows can be translated.

</details>

## For developers

<details>
<summary>Folders and checks</summary>

Read `AGENTS.md` before working in this repository. The documentation index is in `docs/README.md`.

```text
native/                 KWin effect, window controllers, and native tests
control/                system tray control
tests/                  source, package, control, and session checks
docs/                   product contracts and architecture
install.sh              verified installation
disable.sh              safe release and temporary disable
uninstall.sh            installed component removal
```

Run the repository checks before proposing a change:

```bash
./verify.sh
```

It runs the documentation, source, package and control checks. `install.sh`
runs the source, package and control checks itself before it builds. Where the
Plasma development stack is missing, `bash tests/verify-headless.sh` is a fast
pre-check: it runs the native domain tests that don't link KWin and needs only
a C++20 compiler and Qt6Core.

Changes to the compositor also need the private route matrix, a fresh login
session and physical testing. `docs/TESTING.md` describes each check and what it
proves.

</details>

## Part of Shuffle

Kadunce is one of the open-source pieces of Shuffle, a touch-first desktop for
KDE Plasma, alongside [Tettegouche](https://github.com/carlsonjm/tettegouche),
[Temperance](https://github.com/carlsonjm/temperance),
[Gooseberry](https://github.com/carlsonjm/gooseberry) and
[Split Rock](https://github.com/carlsonjm/split-rock). In Shuffle, Kadunce is
called Cards. Get the whole thing at [shuffleplasma.com](https://shuffleplasma.com).

## License

Kadunce is licensed under GPL-2.0-or-later. See `LICENSE`.

The project names and marks are not covered by that licence. See
[TRADEMARKS.md](TRADEMARKS.md).

<p align="center"><picture><source media="(prefers-color-scheme: dark)" srcset="assets/readme/good-input-white.png"><img src="assets/readme/good-input-ink.png" alt="Good Input" width="140"></picture></p>
