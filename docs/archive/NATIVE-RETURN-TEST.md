# Cross-output native return — September 12

Installed baseline: `31fae518395f42f43d9b982a6623fcd3451665fd2df743c40f13ad3ca1a9da13`.
Candidate: `f30825c0514f7f72129ededa48f2b5e54a2d31cb764ae971724419d5c01aa57b`.
No installation, effect toggle, restart, repair promotion or push by the agent.

## Reproduction and scope

User reports an Active-sized tablet window returning home when dropped in free
monitor space; Card Line transfer still passes. Live bounded trace is saved in
NATIVE-RETURN-TRACE-20260912.jsonl: Xwayland ordinary-source staging, accepted
pickup, lost destination, uncommitted drop. It is not lost input proof.

The receiver rejected NativeDesktop intent whenever output differed from the
window's source. Permit that landing only for an unmanaged source on a free
external display. Managed departures, existing destination sessions and tablet
admission cannot bypass their normal paths. Release revalidates preparation.
Only DesktopStageController.cpp changes production behavior.

This does not explain why that particular window retained Active-like geometry
without a managed source. Do not infer ownership from sizing or claim that
separate lifecycle issue resolved.

## Evidence

- New cross-open regression fails on baseline at destination assertion:
  /tmp/kadunce-unload-test.uHpVWH.
- Candidate Wayland and Xwayland cross-open pointer/touch pass:
  /tmp/kadunce-unload-test.SPTS6m and /tmp/kadunce-unload-test.uLf9RF.
- Genuine Xwayland Active pickup/return passes on baseline and candidate:
  /tmp/kadunce-unload-test.epqTRo and /tmp/kadunce-unload-test.fyQr7A.
- Fresh integrated build/gate: /tmp/kadunce-integrated-carry.NizPos.
  Completed exit 0: 14 CTests, 15 runtime routes, Bento restoration/unload,
  source guards and independent control/live safety gates passed. Exact frozen
  source and logs preserved in ../work/kadunce-native-return-test-20260912.

## Physical check after install and login

User reports all three checks below passed. Additional deferred failure: a normal
window returned through bottom exit could not subsequently re-enter Bento;
KDE edge snapping occurred over the open Bento layout. This is not evidence of
the cause. Next investigation should compare fresh ordinary entry with
bottom-exit→new drag→edge entry beside existing Bento. No runtime changes made
in response to this later-fix report.

1. Drag the tablet's large Active window to empty monitor space: it stays there
   as an ordinary window, without returning home.
2. Drag an ordinary monitor window across the tablet and back without releasing:
   it remains ordinary on final monitor release.
3. Side-edge Bento entry and bottom-edge return remain unchanged.

Installer: ../install-kadunce-native-return-test.sh. Its --rollback restores
the baseline above; --check verifies hashes and the independent live safety
control without installing. Trusted repair remains unchanged.
