# Rollback provenance — 2026-09-11

Initial read-only audit followed by user-authorized recovery. No plugin
installation, effect toggle or compositor restart performed.

## Installed binary: confirmed pre-model-switch build

Current `/usr/lib/qt6/plugins/kwin/effects/plugins/kwin4_effect_kadunce.so`
SHA-256:

`b43695f137874f3dd17694ea097c6ebd382c300136c27c5087a649f712129494`

Independent historical evidence from **Z13 - CashyOS**, rather than trusting
the later rollback script's label:

- Turn `01a08d71-13c3-7e53-b6b5-b3bafa485acc`: the pre-switch handoff build
  command reported this exact hash and supplied the display-handoff installer.
- Turn `01a08d86-23c2-7c92-bd27-edb4caa197fc`: user reported successful drag
  and drop. Further background-sizing work was subsequently held uninstalled.
- Turn `01a08d92-80c0-7cc2-b346-034296db723d`: a read of the installed file
  again reported this hash while investigating Active drag releasing the stage.
- Turn `01a08e30-21d5-7022-bc39-f06ee8bc06f6`: final pre-capacity gesture
  audit explicitly made no code or installation changes.
- Turn `01a08e36-47f2-74c1-9338-c6974615e150`: user announced the switch to
  Sol. The preceding records establish the installed baseline before that point.

Thus the file restored is byte-identical to the last installed pre-switch build,
not merely an approximation rebuilt after the rejected touch experiment. It
includes the initial handoff work that passed a user drag/drop check, but still
has the known Active-drag/full-stage-release bug. It is not a claim that every
monitor scenario passed, nor that it equals clean Git HEAD.

## Source and repair: recovered and aligned

Initially, the working native tree and `~/.local/share/kadunce/repair/source.tar`
both contained later changes. Recovery reconstructed the trusted handoff source
by replaying the untruncated file edits from turn
`01a08d71-13c3-7e53-b6b5-b3bafa485acc` onto clean Git HEAD. Later turns were not
replayed. The rebuilt binary's .text section matches the installed baseline.
After stripping debug information and removing .note.gnu.build-id, the complete
binaries are identical, SHA-256:
`d422c0ee8770d2cc0385d708aa17c6ca50ef9a8c3f1713c7ebce12448a929ca4`.

All seven native tests passed, including the isolated D-Bus window-handling test.
The source and control checks passed on the recovered tree. The compiler emits
the original ignored-result and missing-initializer warnings; no cleanup edits
were mixed into this recovery.

The working native tree was restored to that recovered source. The original
prepare-repair helper then regenerated the repair archive from the tested tree.
New archive SHA-256:
`27f775ecad1e2d132f985950660c8d039eaf015b7e499723cf348cd51c4fa1d9`.
The checksum passes. The repair helper itself matches the frozen Git version;
the erroneous input snapshot, rather than a changed helper, was the problem.

Clean Git HEAD is `a74991fccaa5e4070f62a7ec87642406d9eecf56`; it predates the
initial handoff candidate. Resetting to HEAD would discard more than the rejected
touch changes. The recovery directory adjacent to the repo is
`recovery/kadunce-pre-switch-2026-09-10/`: source and build contain the recovered
baseline; preserved contains the prior native tree, repair archive/checksum/helper,
and a copy of the trusted installed binary. Later changes remain recoverable.

## Remaining verification limits

- The private pre-touch backup could not be read in this environment. The
  independent pre-switch hash records above establish installed-file identity.
- After access was granted, KWin reports the effect loaded. Its systemd service
  started after the rollback file was installed. Direct process mappings remain
  unavailable, so an in-memory byte comparison was not performed.
- Live tray verification passes: the switch is registered, Active, and exposes
  the enable/disable action. Its service is active and enabled, with the required
  graphical-session.target startup link. No switch was toggled as a test.
- No fresh gesture or monitor acceptance test was performed.

Recovery does not fix pre-existing Active-drag release or improve touch physics.
Keep those future changes separate from this restored baseline. Never refresh
the accepted repair snapshot merely because candidate source builds successfully.
