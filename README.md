<p align="center">
  <img src="assets/studio.warbler.kadunce-logo.png" width="180" alt="Kadunce">
</p>

# Kadunce

Kadunce organizes KDE Plasma application windows as Cards that are easier to use
and arrange on a touchscreen. Applications remain ordinary Plasma windows and
continue to work normally.

Kadunce is designed for a computer with a built-in touchscreen. It can also arrange windows independently on an attached monitor.

## How cards work

A Card is a normal application window managed by Kadunce. Each Card belongs to a
Workspace, and each display manages its own Cards.

Turning Kadunce on does not immediately take over your windows. Management begins when you drag a window to one of Kadunce's screen edges:

- **Top edge:** make the window the active card.
- **Left or right edge:** place the window in a Bento layout.
- **Bottom edge:** return the window to the regular Plasma desktop.

The first time you send a window to the top, left, or right edge, Kadunce also gathers the other eligible application windows on that display. This keeps every open window available when you enter Card Spread.

## The workspace

### Active

Active is the Card you are currently using. Other Cards remain in the Workspace
and are ready when you return to Spread.

### Spread

Spread shows the Cards immediately around you as an ordered row. Choose a Card to
make it Active, or move through the row to find another application.

Opening an application creates a Card. It does not replace or release the Cards
already in the Workspace.

### Stacks

Cards can be collected into a stack. A stack keeps related applications together
and lets you move through them one at a time.

### Bento

Bento shows several Cards working together in one layout.

In Spread, the complete Bento arrangement appears as one grouped Card. Choosing it
restores the same panes and proportions.

A window leaves Bento and becomes its own card when you:

- Drag it to the top edge
- Minimize it
- Replace it with another Bento pane
- Remove it from the visible layout

To put an individual Card back into Bento, drag it to the left or right edge.

Each display can have one Bento layout at a time.

## Moving around

On the built-in touchscreen:

- Swipe upward from the bottom edge to open Spread.
- Swipe downward from the top edge to return to Active, a stack, or Bento.
- Swipe from the left or right edge while using a Card to move between Cards.
- Swipe across the Spread background to move through the row.
- Swipe vertically on a stack to change its selected Card.
- Hold a Card to pick it up and move it.

Keyboard controls are also available:

- `Ctrl+S`: switch between Active and Spread.
- `Ctrl+Left` / `Ctrl+Right`: move through Spread.
- `Ctrl+Up` / `Ctrl+Down`: move through the selected stack.
- `Ctrl+B`: open or close Bento on the display under the pointer.
- `Ctrl+Esc`: return Kadunce-managed windows on that display to the regular Plasma desktop.

While adjusting Bento, hold a divider briefly, drag it to the desired position, and release.

## Multiple displays

Kadunce manages each display separately.

The built-in touchscreen can use Active, Spread, stacks, and Bento. An attached
monitor continues to work as a normal Plasma desktop and can have its own Bento
layout.

Moving a window between displays transfers it to the destination display. It is no longer managed by the display it left.

## Enabling and disabling Kadunce

The system tray contains a **Kadunce enabled** switch.

Disabling Kadunce safely returns its managed windows to the regular Plasma desktop before unloading the workspace. Enabling it loads Kadunce again but does not capture any windows until you perform a Card or Bento action.

## Requirements

- KDE Plasma 6.7 or newer
- A Wayland session
- Qt 6 and KDE Frameworks 6
- KWin development files
- CMake and a C++20 compiler
- `pkexec` for system installation

Kadunce recognizes built-in displays that use the conventional `eDP`, `DSI`, or `LVDS` output names.

## Install

```bash
./install.sh
```

The installer checks and builds Kadunce before asking for permission to install the KWin plugin.

Log out and back in after installation so KWin can load it.

Some system versions may require the documented KWin touch correction in `patches/kwin/package/README.md`.

## Disable or uninstall

```bash
./disable.sh
./uninstall.sh
```

Disabling returns managed windows to Plasma and temporarily turns Kadunce off.

The **Kadunce** button remains available in the System Tray so Kadunce can always
be disabled completely.

Uninstalling removes the installed components but leaves this source folder intact. Log out and back in if the plugin remains loaded for the current session.

## Development

Read `AGENTS.md` before working in this repository. The documentation index is in `docs/README.md`.

```text
native/                 KWin effect, window controllers, and native tests
control/                system tray control
tests/                  source, package, control, and session checks
docs/                   product contracts, architecture, roadmap, and archive
install.sh              verified installation
disable.sh              safe release and temporary disable
uninstall.sh            installed component removal
```

Run the repository checks before proposing a change:

```bash
./verify.sh
```

It runs the documentation, source, package and control checks; `install.sh` runs
the source, package and control checks itself before it builds. `bash tests/verify-headless.sh` runs the native
domain tests that do not link KWin, needing only a C++20 compiler and Qt6Core, as
a fast pre-check where the Plasma development stack is unavailable.

Changes to the compositor also need the private route matrix, a fresh login
session and physical testing. `docs/TESTING.md` describes every check and what it
proves; `docs/KNOWN-ISSUES.md` lists current limitations.

## License

Kadunce is licensed under GPL-2.0-or-later. See `LICENSE`.

The project names and marks are not covered by that licence. See
[TRADEMARKS.md](TRADEMARKS.md).
