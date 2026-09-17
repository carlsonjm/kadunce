# Local KWin package provenance

This is the live provenance and rollback contract for the tested KWin 6.7.5 package.
No package binary is distributed by this repository or installed by Kadunce's
installer.

## Recipe and patch

- Base recipe: official Arch `kwin` 6.7.5-1 PKGBUILD.
- Base PKGBUILD SHA256:
  `8707a0efff46abdb7849d906dabaf45247c5c238ecc951ed70e17d2d89ca3ee4`
- The installed CachyOS recipe with only `pkgrel=1.1` has SHA256:
  `82177fc6944085a9b947351e48144da62ec43ec771cb59d4ed43a2d2368245ac`
- Local release `1.2` adds the checksum-pinned parent touch-lifetime patch, permits
  the installed x86_64_v4 architecture, and retains normal dependencies, package
  ownership, capabilities, and build/package functions.
- The upstream detached signature validates to the recipe-authorized KDE key. Source
  verification and patch application must remain enabled.

The local build uses host CachyOS hardening/LTO and architecture flags. It is a
compatible local rebuild, not a byte-identical reconstruction of the distribution
build server. Archive the actual makepkg configuration with each build.

## Installed package

- Package: `kwin-6.7.5-1.2-x86_64_v4.pkg.tar.zst`
- Package SHA256:
  `5cce9c33852909bde63a03c0f95658771ea9f1a1b4bb23d5f0cb65c6a2537e01`
- Packaged and installed `libkwin.so.6` SHA256:
  `04e3dcb7252fcede1b01e64a435d1446eef26200707c6ba4655243d4d252872c`
- Packaged `kwin_wayland` SHA256:
  `ae61ebe6c65d1a98a20b240d9610541adfd00053d46de9a96be9bbf5dc38e55f`

The package file/dependency/group metadata and compositor ownership, mode, and
capabilities match the signed rollback contract. The focused native touch cases,
Wayland/Xwayland Kadunce exit routes, control tests, and read-only live switch checks
passed against this package.

## Rollback and upgrade

The verified signed rollback package is:

`/var/cache/pacman/pkg/kwin-6.7.5-1.1-x86_64_v4.pkg.tar.zst`

SHA256:
`6fb71a11532630630eec43a23562c3ab879a805ec75bbdeee6e1e3f7922963a4`

Install or rollback only through the package manager. Keep the signed rollback until
an official package containing the fix is physically accepted. Official 6.7.5-2 or a
later release supersedes local 1.2; do not use an epoch or `IgnorePkg` to prevent
normal upgrades. After any KWin upgrade, determine whether the correction is present,
rebuild Kadunce for ABI compatibility, and rerun control and physical gates.

## Safety rules

- Build as an ordinary user; never use `makepkg --install` during validation.
- Do not weaken signature policy or copy a test library into the system.
- Compare both executable and resolved library hashes before installation.
- Installing and restarting/logging into a new compositor session require explicit
  authorization.
- Preserve private/live bus separation and the persistent Kadunce disable control.
