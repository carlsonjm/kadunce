# Native touch lifetime correction — KWin 6.7.5

Historical development evidence below; the distribution-matched6.7.5-1.2
package was subsequently approved and installed. See package/README.md and
../../docs/CURRENT_STATE.md for current provenance. This directory contains
source/recipe documentation, not a replacement KWin binary or automatic installer.
Do not apply it to an arbitrary KWin version or copy a test library into /usr.

`0001-bound-native-touch-to-move-lifetime.patch` changes KWin's internal
MoveResizeFilter and adds six integration-test cases. Kadunce production source
is unchanged. See `docs/K1-TAKEOVER-AUDIT.md` for the original failing sequence.

## Ownership rule

The saved native touch ID belongs to one window AND one native move/resize
lifetime, not the lifetime of the input filter. Before processing motion or up,
compare the weak window identity and its existing `interactiveMoveResizeCount()`.
If either differs, discard the previous touch selection. The existing native
movement/release implementation then runs normally.

KWin already increments this counter when a native move finishes or is canceled.
The weak handle covers destruction/replacement. Checking on up also handles a
new native move released without any intervening motion. No new timer, signal
subscription, synthetic event, input priority, geometry writer or Kadunce-specific
code is introduced into KWin. Existing in-move multi-contact behavior is unchanged.

## Source and private build

Upstream source archive:
https://invent.kde.org/plasma/kwin/-/archive/v6.7.5/kwin-v6.7.5.tar.gz

Archive SHA256:
29ebcd04aaf1bd05d5a3d23478ff2d5fdacd8ac142950b539686445490cc4442.

Private work root: `/tmp/kadunce-kwin-lifecycle`.
Source: `kwin-v6.7.5`; build: `build`; original binaries: `baseline-bin`.
`configure.log`, `build-baseline.log`, `build-patched.log` retain build evidence.
Tests use a private runtime directory, configuration/data/state and D-Bus session.
No graphical-session restart or installed effect toggle occurred.

Build used Ninja, RelWithDebInfo with `-O1 -g1 -DNDEBUG`, BUILD_TESTING=ON,
KWIN_BUILD_KCMS=OFF and KWIN_BUILD_RUNNERS=OFF. Xwayland and screen locking remain
compiled in. This is a test build, not the installed distribution build recipe.

Missing protocol XML/development metadata was extracted privately from these
packages, not installed: wayland-protocols 1.49-1 and plasma-wayland-protocols
1.22.0-1. Their private prefix is `deps/usr`; the local wayland-protocols.pc prefix
was relocated accordingly. J subsequently approved necessary missing dependency
installation for future work; that does not authorize replacing KWin itself.

The baseline library SHA256 is
02f8b926a2c8165aae5d8a68137b7983dc05c7a7dcad9afd7f0687796b123687.
Patched library SHA256 is
3470ac9ccf5fe69cf83b067d91118d0bb7c7745491f6f756feff7e82b5116e98.
The executable alone is insufficient provenance: the changed implementation
is in libkwin.so.6. The isolated Kadunce harness now records both resolved hashes.

## Discriminating results

- `/tmp/kadunce-kwin-before.ONgKRZ`: all six added cases fail against the original
  library, each in its own process. Both same/replacement-window motion and
  release-without-motion distinguish the defect.
- `/tmp/kadunce-kwin-test.9fJOHz`: patched library, all six added cases plus the
  selected existing move/resize, pointer release, application move, cancellation,
  and destruction tests: **35 passed, zero failed/skipped**.
- `/tmp/kadunce-unload-test.LmLK9A`: freshly compiled unpatched KWin reproduces
  accepted Kadunce's second-touch-drag failure, matching the installed engine.
- `/tmp/kadunce-unload-test.PxbRHz` and `/tmp/kadunce-unload-test.SH7ztE`: patched
  engine, accepted f308 Kadunce, full Wayland/Xwayland exit matrices both pass.
  Each covers pointer/touch and detach, multi, withdrawn, dock and release cases;
  native movement, completion, preview and exact membership remain asserted.

KWin's native test process reported an unavailable private X11 socket; those
selected cases use Wayland, run to completion and do not establish Xwayland
coverage. The separate Xwayland Kadunce matrix uses the established private
abstract-socket launcher and provides that coverage.

Fresh Kadunce capture `/tmp/kadunce-integrated-carry.5EuMDY` rebuilt the identical
accepted f308 binary. All 14 CTests and all 16 runtime routes passed. The initial
aggregate was terminated with SIGTERM mid-desktop-runtime, without an assertion
failure; it is not a successful aggregate invocation. Remaining routes passed
in two continuation batches using the same captured source/build; their files
are named `*-resumed.log`. No failed assertion was skipped or weakened.
The separate patched-engine Bento restoration/unload check passed at
`/tmp/kadunce-bento-candidate.dzuDfI`. Source guards, control package tests and
read-only live kill-switch verification also pass.

## Running Kadunce tests against the private engine

`tests/verify-unload-isolated.sh` accepts an opt-in `KADUNCE_TEST_KWIN` absolute
executable path under /tmp. Without it, the existing installed-engine behavior
is unchanged. All private bus/configuration separation remains mandatory.

```bash
KADUNCE_TEST_KWIN=/tmp/kadunce-kwin-lifecycle/build/bin/kwin_wayland \
KADUNCE_RUNTIME_BUILD=/tmp/kadunce-integrated-carry.NizPos/production \
KADUNCE_PROBE_SESSION=exit-runtime-session.sh bash tests/verify-unload-isolated.sh
```

Use `x11-exit-runtime-session.sh` for the corresponding Xwayland matrix.
Never point this private test at the real user-session bus or its display socket.

## Packaging and acceptance boundary

Next prepare a distribution-matched KWin package, preserving CachyOS/Arch patches,
build options, dependencies and package ownership. Keep a verified rollback
package for the currently installed 6.7.5-1.1. Do not distribute the private
test binary as a system upgrade, silently pin KWin, or promote Kadunce repair.
Review the patch against the exact package source; run the same regressions on
the packaged binary. Installation and logout/login need explicit approval.
No upstream issue/MR, Git publication or system package change has been made.

Verified cached rollback package (CachyOS detached signature is good):
`/var/cache/pacman/pkg/kwin-6.7.5-1.1-x86_64_v4.pkg.tar.zst`, SHA256
6fb71a11532630630eec43a23562c3ab879a805ec75bbdeee6e1e3f7922963a4.
The inspected Arch6.7.5-1 recipe with only pkgrel changed to1.1 matches the
installed CachyOS PKGBUILD hash exactly. See package/README.md for the full-featured
local package build, signed-source verification and acceptance gates. Official release
archive SHA256 is 6baa910b732d93c48c90f9c1cc685cc93d0b8de0cdf138c24192c045bc3a48e2;
download and zero-fuzz patch applicability were verified separately.

Physical K1 acceptance still requires J's original pointer/touch exit→new drag→
edge loop, both outputs and repeated attempts. The native fix does not establish
that every earlier pointer report has this cause, or complete K2–K6.
