# Current state

Updated September 12, 2026: J confirmed the edge-stabilization installation
passed and explicitly requested freezing it as the new main checkpoint and
pushing. This supersedes the earlier candidate/failure status. See
FREEZE-20260912-EDGE-STABILIZATION.md for the accepted scope and evidence.

## Accepted installation and source

Installed Kadunce SHA256:
`38a2fbb57e8bc93990ca46c6a6619b66e779c8f7e964f8cc36cb9f55d6beebac`.
The persistent candidate has identical bytes. Repository native source matches
the candidate's captured source archive; no new runtime edits during the freeze.
Previous main was faee199 (implementation checkpoint14c6f89).

Included: dock-safe ordinary release and tablet/monitor Bento bottom departure;
visible new-app admission into existing Bento; automatic native edge-tiling and
edge-maximize suppression while Kadunce is enabled; expanded direct idle swipe
reach across dock depth plus36 logical pixels above it. Preserve the accepted
cross-display ownership fixes. No timing rewrite or input-priority change.

NativeEdgePolicy changes only in-memory automatic edge options, honors config
reload, and restores preferences on unload. Explicit Shift-drag custom tiling,
keyboard/manual maximize and ordinary window magnetism remain separate paths.
Held-card departure still targets the physical bottom24px. Idle swipe still
passes taps through and requires deliberate single-finger upward motion.

J reported the update installed and passed, then requested this freeze. Earlier
Konsole misalignment and restricted swipe reach prompted the candidate; no
Konsole-specific cause or patch was established. Acceptance is J's physical
report, not a claim of exhaustive hardware/application coverage.

## Evidence and safety

Focused panel-input/window-handling/carry-paint tests3/3 pass. Private KWin
Xwayland local entry/held disable passes (rchf2D), including automatic edge
suppression through config reload and restoration on unload. Tablet-only
Wayland local entry/bottom release/cancel pointer+touch passes (om9o6A).
Earlier combined-batch tests cover visible new-app1→2 admission and restoration,
tablet/monitor bottom recovery and Bento departure/re-entry; details in the freeze.

Freeze-time control tests2/2, source guards, diff check and read-only live safety
pass. Control startup is wanted by graphical-session.target. No restart, input
injection, installation or repair promotion is part of this publication step.

## Recovery / separate engine prerequisite

Persistent bundle: ../work/kadunce-edge-stabilization-20260912.
Opt-in installer: ../install-kadunce-edge-stabilization.sh.
It preserves previous6475 and pre-K0 accepted f308 binaries. Machine-local
binaries, logs and recovery files are not published in the repository.

KWin6.7.5-1.2 was separately approved and installed. Installed lib SHA256:
`04e3dcb7252fcede1b01e64a435d1446eef26200707c6ba4655243d4d252872c`.
The source patch and package recipe/provenance are under patches/kwin; the
Kadunce installer does not install this engine patch. The signed original1.1
package remains in ../work/kwin-touch-repair-20260912/rollback.

Trusted repair source.tar remains unchanged:
`27f775ecad1e2d132f985950660c8d039eaf015b7e499723cf348cd51c4fa1d9`.
Publishing main does not silently promote repair. Never restore the rejected
rough-swipe build ba47bf822343fcf06140a1dbd24f209052fc29d01e7d6fadef2ab6e8be0e6859.

## Next work, not automatic continuation

MVP-RELEASE-SCOPE.md is assignment policy; old K0–K6 and historical roadmap
lists are not mandatory refactors. Reproduce occupied-tablet contention or
slow/helper launch failures before changing their architecture. Stack insertion
and motion polish are separate experience packets. Any lost window, stuck input
or broken disable is a blocker when observed. B remains assigned Temperance.
Companion-app work and Codex profile recovery are outside this Kadunce freeze.
