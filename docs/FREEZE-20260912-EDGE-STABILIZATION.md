# Accepted main checkpoint — September 12 evening

J reported the installed edge-stabilization update passed, and explicitly
authorized freezing it as the new main version and pushing to the repository.
This succeeds faee199/14c6f89 without rolling back accepted cross-display work.

## Included

- Dock-safe ordinary bottom release with10px clearance, including floating docks.
- Physical bottom-edge Bento departure on tablet and monitor; pullback cancels.
- New eligible apps join existing Bento visibly through the existing planner.
- Automatic KDE side/corner tiling and top-edge maximize are suppressed for the
  effect lifetime, including ordinary windows. Preferences survive config reload
  and restore on unload; persistent settings are not rewritten.
- Direct idle bottom-swipe reach includes dock/work-area depth plus36 logical
  pixels above it. Taps, intent threshold and multitouch rejection are preserved.
- Focused regression probes, test-environment procedures and scope-reset roadmap.
- Separate KWin touch-lifetime patch and distribution package provenance, not
  a bundled or automatically installed replacement compositor.

## Provenance and evidence

Accepted installed/candidate SHA256:
`38a2fbb57e8bc93990ca46c6a6619b66e779c8f7e964f8cc36cb9f55d6beebac`.
Native source compared equal to the captured candidate archive during freeze.
The publication changes documentation only beyond that accepted runtime source.

- Focused model/router checks: panel-input, window-handling, carry-paint3/3.
- Private Xwayland local Bento entry and held disable, pointer/touch: rchf2D.
  Includes native option suppression, configuration reload and unload restoration.
- Private tablet-only Wayland entry, bottom release and cancellation,
  pointer/touch: om9o6A.
- Earlier same-batch dock/native release: monitor bQxK55, tablet UFJE8u;
  tablet Bento departure/fresh move/re-entry: mWOTaX.
- Earlier new-app1→2 visible Bento plus restoration: monitor e5LPK0,
  tablet C4zmbZ. These are historical batch results, not re-run freeze checks.
- Freeze-time independent safety2/2, live kill-switch registration, graphical
  session startup wiring, source guards and diff checks pass.
- J's latest report provides physical acceptance. No broad new runtime matrix
  or claim of universal hardware coverage is implied.

Evidence lives in the private machine-local work bundles; raw session data,
binaries, system package archives and Codex recovery backups are excluded.

## Boundaries

Explicit Shift-drag custom tiling and keyboard/manual window operations remain
unchanged. Konsole had no independently proven app-specific cause; do not claim
one. Occupied-tablet and slow/helper-launch concerns require current reproduction
before further structural work. Stack/motion polish remains a separate packet.

The installed KWin6.7.5-1.2 patch is documented under patches/kwin/package.
The normal Kadunce installer does not deploy KWin. Trusted repair remains the
previous archive; this freeze/push does not authorize its automatic replacement.
