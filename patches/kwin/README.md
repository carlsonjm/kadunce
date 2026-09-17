# KWin native touch-lifetime correction

This directory documents a version-bound KWin correction required by the tested
KWin 6.7.5 environment. Kadunce's installer does not patch, replace, or pin KWin.
Do not apply the patch to another version without reviewing the upstream source and
running the same focused regressions.

`0001-bound-native-touch-to-move-lifetime.patch` changes KWin's internal
`MoveResizeFilter` and adds six integration cases. Kadunce production source is
unchanged.

## Ownership rule

The saved native touch ID belongs to one window and one native move/resize lifetime,
not to the input filter lifetime. Before motion or release, compare weak window
identity and `interactiveMoveResizeCount()`. If either differs, discard the saved
selection and let the existing native path proceed.

The weak handle covers destruction/replacement. The lifetime counter covers a new
move on the same window, including release without intervening motion. The change
adds no timer, synthetic event, input priority, geometry writer, or Kadunce-specific
code to KWin.

## Source identity

- Upstream archive: `kwin-v6.7.5.tar.gz`
- Upstream archive SHA256:
  `29ebcd04aaf1bd05d5a3d23478ff2d5fdacd8ac142950b539686445490cc4442`
- Official release archive SHA256:
  `6baa910b732d93c48c90f9c1cc685cc93d0b8de0cdf138c24192c045bc3a48e2`

Private tests must resolve and record both `kwin_wayland` and `libkwin.so.6`; the
implementation change lives in the library. Use an isolated runtime, display, and
D-Bus. Never copy a private test library into `/usr`.

## Required validation

- Six added native touch lifetime cases distinguish baseline from patched KWin.
- Selected native move/resize, application move, cancellation, destruction, and
  Wayland/Xwayland Kadunce exit routes pass against the resolved patched library.
- Kadunce integrated carry, Bento restoration/unload, package, and control checks
  pass without weakened assertions.
- A physical pass covers pointer and touch exit → new drag → edge entry, both
  outputs, repeated attempts, restoration, and the persistent disable control.

`tests/verify-unload-isolated.sh` accepts an explicit private `KADUNCE_TEST_KWIN`
path. It must never point to the live session bus or display.

## Packaging boundary

Use the distribution's exact package recipe, dependencies, ownership, capabilities,
and signed rollback package. Install through the package manager only. Do not pin
KWin; an official release containing the fix supersedes the local package. See
`package/README.md` for the installed 6.7.5 package provenance.
