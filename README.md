# Kadunce

Kadunce is a touch-first card workspace for KDE Plasma. It presents real
desktop applications as cards while leaving window lifecycle, focus, outputs,
and recovery under KWin's authority.

> The tablet optimizes choosing and focusing. The desktop stage optimizes
> seeing and composing. Neither display should imitate the other.

## Features

- Active and Card Line presentations on a built-in touch display.
- Logical stacks with a compact fan and unlimited membership.
- Output-local Bento layouts with reversible window geometry.
- Transactional handoff between Card Line and Bento.
- A persistent tray switch that safely enables or disables the workspace.
- Direct four-edge input when a supported posture helper is available.
- Native top and bottom touchscreen edges on other hardware.
- A versioned context contract and opt-in guest-card handoff for companion
  launchers.

## Requirements

- KDE Plasma 6.7 or newer on Wayland.
- Qt 6, KDE Frameworks 6, KWin development files, CMake, and a C++20 compiler.
- `pkexec` for installing the native KWin plugin into the system plugin path.

Kadunce detects a built-in display by the conventional `eDP`, `DSI`, or `LVDS`
output prefix. Optional device helpers can provide richer posture and edge
behavior, but they are not required to build or install Kadunce.

## Install

```bash
./install.sh
```

The installer validates and builds everything before requesting permission to
copy one native plugin. Log out and back in after a successful installation so
KWin loads the new binary.

## Controls

- `Ctrl+S`: toggle Active and Card Line.
- `Ctrl+Left/Right`: move between Card Line groups.
- `Ctrl+Up/Down`: move through a selected stack.
- `Ctrl+B`: toggle Bento under the pointer.
- `Ctrl+Esc`: release managed windows.

The Kadunce tray icon contains one checked **Kadunce enabled** switch. Turning
it off releases managed windows before unloading the effect. Turning it back on
reloads the installed effect.

## Disable or uninstall

Temporarily release and disable Kadunce:

```bash
./disable.sh
```

Remove the installed plugin and per-user controller:

```bash
./uninstall.sh
```

Uninstalling does not remove this source checkout. Log out and back in after
removing the native plugin.

## Companion context

Kadunce publishes normalized workspace state through the versioned
[`workspaceContext`](docs/TETTEGOUCHE-CONTEXT.md) contract. Compatible companion
launchers may also negotiate a separate guest-card session; unsupported clients
and older Kadunce builds retain standalone behavior.

## Project layout

```text
native/                 KWin effect, controllers, models, and native tests
control/                persistent tray switch
tests/                  source, package, control, and nested-session checks
docs/                   architecture, product, context, and compatibility notes
install.sh              verified installation path
disable.sh              safe release and temporary disable
uninstall.sh            complete installed-payload removal
```

## Verification

```bash
./tests/verify-source.sh
./tests/verify-package.sh
./tests/verify-control.sh
```

Native compositor updates require a fresh Plasma session and a physical pass
covering gestures, stacks, Bento, display changes, suspend, release, and the
tray kill switch. Current limitations are recorded in
[`docs/KNOWN-ISSUES.md`](docs/KNOWN-ISSUES.md).

## License

Kadunce is licensed under GPL-2.0-or-later. See [`LICENSE`](LICENSE).
