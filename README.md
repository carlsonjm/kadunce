<p align="center">
  <img src="assets/studio.warbler.kadunce-logo.png" width="180" alt="Kadunce package icon">
</p>

# Kadunce

Kadunce is a touch-first spatial window workspace for KDE Plasma. It presents real
applications as cards in Active, Card Line, stacks, and output-local Bento layouts
while KWin remains authoritative for window lifecycle, focus, outputs, and recovery.

## Features

- Active and Card Line presentations on the built-in touch display.
- Ordered stacks with a compact fan and unbounded logical membership.
- Output-local Bento layouts with reversible real-window geometry.
- Transactional transfer between Card Line, Bento, and ordinary desktop space.
- A persistent tray switch that safely enables or disables the workspace.
- Native compositor edge input, with optional posture-aware hardware integration.
- A versioned read-only context contract and guest-card protocol for companions.

## Requirements

- KDE Plasma 6.7 or newer on Wayland.
- Qt 6, KDE Frameworks 6, KWin development files, CMake, and a C++20 compiler.
- `pkexec` for installing the native KWin plugin into the system plugin path.

The built-in display is detected by the conventional `eDP`, `DSI`, or `LVDS`
output prefix. Optional device helpers can provide richer posture and edge behavior.

## Install

```bash
./install.sh
```

The installer validates and builds the package before requesting permission to copy
the native plugin. Log out and back in after installation so KWin loads the binary.
The installer does not replace KWin. See `patches/kwin/package/README.md` when the
documented KWin touch-lifetime correction is required for the exact system version.

## Controls

- `Ctrl+S`: toggle Active and Card Line.
- `Ctrl+Left/Right`: page Card Line.
- `Ctrl+Up/Down`: browse the selected stack.
- `Ctrl+B`: toggle Bento under the pointer.
- `Ctrl+Esc`: release managed windows on the output under the pointer.
- Bento divider: hold a shared divider for 90 ms, drag the preview, and release.

The tray item contains one checked **Kadunce enabled** switch. Disabling releases
managed windows before unloading the effect; enabling reloads the installed effect.

## Disable or uninstall

```bash
./disable.sh
./uninstall.sh
```

Uninstalling leaves the source checkout intact. A session restart may be required
after removing the native plugin.

## Development

Read `AGENTS.md` before working in the repository. The documentation index is
`docs/README.md`; current state and future work live only in the canonical files
named there.

```text
native/                 KWin effect, controllers, models, and native tests
control/                persistent tray switch
tests/                  source, package, control, and isolated-session checks
docs/                   current contracts, architecture, roadmap, and archive
install.sh              verified installation path
disable.sh              safe release and temporary disable
uninstall.sh            installed-payload removal
```

Run the focused source, package, and control checks before proposing a candidate:

```bash
./tests/verify-source.sh
./tests/verify-package.sh
./tests/verify-control.sh
```

Native compositor updates also require an authorized fresh-session and physical
pass. See `docs/KNOWN-ISSUES.md` and `docs/TEST-ENVIRONMENT-PROCEDURE.md`.

## License

Kadunce is licensed under GPL-2.0-or-later. See `LICENSE`.
