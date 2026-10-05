# KWin native touch-lifetime correction

A version-bound correction to KWin 6.7.5, and the provenance and rollback
contract of the local package that carries it. Kadunce's installer never
patches, replaces or pins KWin, and this repository distributes no package
binary. Do not apply the patch to another KWin version without reviewing the
upstream source and running the same focused regressions.

## What it changes

`0001-bound-native-touch-to-move-lifetime.patch` changes KWin's internal
`MoveResizeFilter` and adds six integration cases; Kadunce's own source is
unchanged. The saved native touch ID belongs to one window and one native
move/resize lifetime, not to the input filter's lifetime. Before motion or
release, KWin compares the weak window identity and
`interactiveMoveResizeCount()`; if either differs, it discards the saved
selection and lets the native path proceed. The weak handle covers destruction
and replacement; the counter covers a new move on the same window. The change
adds no timer, synthetic event, input priority, geometry writer or
Kadunce-specific code.

## Source identity

- Upstream archive `kwin-v6.7.5.tar.gz`, SHA256
  `29ebcd04aaf1bd05d5a3d23478ff2d5fdacd8ac142950b539686445490cc4442`
- Official release archive SHA256
  `6baa910b732d93c48c90f9c1cc685cc93d0b8de0cdf138c24192c045bc3a48e2`
- Base recipe: official Arch `kwin` 6.7.5-1 PKGBUILD, SHA256
  `8707a0efff46abdb7849d906dabaf45247c5c238ecc951ed70e17d2d89ca3ee4`
- The installed CachyOS recipe with only `pkgrel=1.1`, SHA256
  `82177fc6944085a9b947351e48144da62ec43ec771cb59d4ed43a2d2368245ac`

Local release `1.2` (`package/PKGBUILD`, `package/makepkg-local.conf`) adds the
checksum-pinned patch, permits the installed x86_64_v4 architecture, and keeps
normal dependencies, package ownership, capabilities and build functions. The
upstream detached signature validates to the recipe-authorized KDE key; source
verification and patch application stay enabled. The build uses the host's
CachyOS hardening, LTO and architecture flags, so it is a compatible local
rebuild, not a byte-identical one; archive the makepkg configuration with each
build.

## Installed package

- `kwin-6.7.5-1.2-x86_64_v4.pkg.tar.zst`, SHA256
  `5cce9c33852909bde63a03c0f95658771ea9f1a1b4bb23d5f0cb65c6a2537e01`
- Packaged and installed `libkwin.so.6`, SHA256
  `04e3dcb7252fcede1b01e64a435d1446eef26200707c6ba4655243d4d252872c`
- Packaged `kwin_wayland`, SHA256
  `ae61ebe6c65d1a98a20b240d9610541adfd00053d46de9a96be9bbf5dc38e55f`

Its file, dependency and group metadata and the compositor's ownership, mode and
capabilities match the signed rollback package. The six native touch cases,
Wayland and Xwayland Kadunce exit routes, control tests and read-only live
switch checks passed against it.

## Required validation

- The six added cases distinguish baseline from patched KWin.
- Native move/resize, application move, cancellation, destruction, and Wayland
  and Xwayland Kadunce exit routes pass against the resolved patched library.
- Kadunce's integrated carry, Bento restoration and unload, package and control
  checks pass without weakened assertions.
- A physical pass covers pointer and touch exit, a new drag and edge entry on
  both outputs, repeated attempts, restoration and the disable control.

Private tests resolve and record both `kwin_wayland` and `libkwin.so.6`, since
the change lives in the library, and use an isolated runtime, display and D-Bus.
`tests/verify-unload-isolated.sh` takes an explicit private `KADUNCE_TEST_KWIN`,
never pointed at the live session.

## Next Plasma

The `Patched KWin` workflow builds the KWin that Arch's kde-unstable packages,
applies the patch without fuzz, runs the move and resize class against it and
shows the added cases fail against the stock source. It proves the patch on the
coming release; the package to install is still built on the device, against its
own libraries, as above.

## Rollback and upgrade

The verified signed rollback package is
`/var/cache/pacman/pkg/kwin-6.7.5-1.1-x86_64_v4.pkg.tar.zst`, SHA256
`6fb71a11532630630eec43a23562c3ab879a805ec75bbdeee6e1e3f7922963a4`. Install or
roll back only through the package manager, and keep the rollback until an
official package containing the fix is physically accepted. Official 6.7.5-2 or
later supersedes local 1.2; never use an epoch or `IgnorePkg` to hold back
normal upgrades. After any KWin upgrade, check whether the correction is
present, rebuild Kadunce for ABI compatibility, and rerun the control and
physical gates.

## Safety rules

- Build as an ordinary user; never use `makepkg --install` during validation.
- Do not weaken signature policy or copy a test library into `/usr`.
- Compare both executable and resolved library hashes before installation.
- Installing, and restarting or logging into a new compositor session, need
  explicit authorization.
- Preserve private and live bus separation and the persistent disable control.
