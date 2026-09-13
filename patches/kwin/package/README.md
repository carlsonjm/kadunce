# Local KWin package candidate

Built and focused package gates passed September 12, 2026. Installed with J's
explicit approval at19:54; verified installed payload and capability match.
Logout/login and physical acceptance remain J's next step.

## Provenance

The official Arch `6.7.5-1` PKGBUILD is available at:
https://gitlab.archlinux.org/archlinux/packaging/packages/kwin/-/raw/6.7.5-1/PKGBUILD

Its SHA256 is
8707a0efff46abdb7849d906dabaf45247c5c238ecc951ed70e17d2d89ca3ee4.
Changing only `pkgrel=1` to `pkgrel=1.1` yields SHA256
82177fc6944085a9b947351e48144da62ec43ec771cb59d4ed43a2d2368245ac,
exactly the PKGBUILD hash in the installed CachyOS package's `.BUILDINFO`.
There are no additional source patches in that recipe.

Candidate changes: release `1.2`, permit the installed x86_64_v4 architecture,
add the checksum-pinned touch-lifetime patch and apply it with zero fuzz.
The usual full build/package functions, runtime dependencies and capability
assignment are retained. Tests are not shipped in the production package.

The upstream archive's detached signature validates to the recipe-authorized
primary key `0AAC775BB6437A8D9AF7A3ACFE0784117FBCE11D`, signing subkey
`B3CB366552540BE06EE9AD9711968C44928CAEFC`. Verification used a private keyring;
the user's trust settings were not changed.

## Build boundary

`makepkg-local.conf` retains this host's CachyOS O3/hardening/LTO flags, makes
znver4 targeting explicit, enables split debug packaging as in the installed
BUILDINFO, and limits build/compression concurrency to four. Compression level
is reduced for local turnaround. This is a compatible local rebuild, NOT a
byte-for-byte recreation of the distro build server or its full environment.
Archive the host makepkg configuration alongside each build; it is an input.

Persistent work bundle, relative to the repository:
`../work/kwin-touch-repair-20260912`.
Its `build/` contains this recipe/config, the parent patch, signed release source,
and normal makepkg outputs. `provenance/` contains the original Arch recipe,
host configuration and release signing public key. `rollback/` contains the
exact signed installed package, not a newly reconstructed substitute.

Run makepkg as the ordinary user. Never use `--install`, skip source verification,
copy a test library into /usr, change pacman signature policy, or pin KWin.
The private signing-key directory used for this build is
`/tmp/kadunce-kwin-keys.ve6sFA`; import the archived vetted public key into a new
private keyring if that temporary directory is gone.

Official `6.7.5-2` and later versions supersede local `6.7.5-1.2`; no epoch or
IgnorePkg is introduced. Future upgrades must be checked for this fix, not
silently prevented. The candidate is a local unsigned package, not CachyOS-signed.

## Release gates

Before install: compare package files/dependencies/capabilities with rollback;
extract candidate under /tmp and record the executable AND resolved libkwin
hashes; run the focused native touch regression against that library, the
Wayland/Xwayland exit checks, and persistent-control checks. J owns the broader
physical regression pass; do not repeat the full 16-route gate for this package.
Never substitute the earlier private test library for packaged-binary evidence.

Installation requires J's separate approval. It must use pacman's package
ownership, retain the signed rollback, and not restart the live compositor.
J controls logout/login and physical acceptance. No install script is supplied
until the package gates pass.

## Built candidate evidence

`kwin-6.7.5-1.2-x86_64_v4.pkg.tar.zst` SHA256:
5cce9c33852909bde63a03c0f95658771ea9f1a1b4bb23d5f0cb65c6a2537e01.
Packaged libkwin SHA256:
04e3dcb7252fcede1b01e64a435d1446eef26200707c6ba4655243d4d252872c.
Packaged kwin_wayland SHA256:
ae61ebe6c65d1a98a20b240d9610541adfd00053d46de9a96be9bbf5dc38e55f.

File paths and dependency/optional-dependency/group metadata match the signed
rollback exactly. The compositor's root ownership, mode and capability metadata
match too. Private extraction used --no-xattrs, deliberately not applying system
capabilities to a temporary test executable. Library-resolution evidence records
the packaged payload, not the earlier reduced test build.

The six new native touch cases passed against this library (8 QTest results with
init/cleanup). The two narrowly scoped Kadunce touch detach→native movement→edge
checks passed on Wayland and Xwayland. Independent control tests2/2 and read-only
live switch checks pass. J owns the broader regression pass; the prior16-route
gate was not repeated. Nothing was installed/restarted except the authorized
missing build dependencies.

Private extraction: /tmp/kadunce-kwin-package.I5Yg03. Persistent bundle evidence/
contains native-touch.log, both exit summaries, both complete isolated evidence
directories and native-library-resolution.txt. provenance/ contains input/package
hash manifests and signature verification. Debug symbols are retained separately
for diagnostics; installing the debug package is unnecessary for J's testing.
# Current checkpoint status — September 12 evening

J separately approved and installed the6.7.5-1.2 package documented below.
Installed libkwin SHA256 matches04e3dcb7252fcede1b01e64a435d1446eef26200707c6ba4655243d4d252872c.
Later Kadunce edge-stabilization was physically accepted for main. Earlier
not-installed statements below describe the packaging gates at that time.
No package binary is distributed by this repository or installed by install.sh.
