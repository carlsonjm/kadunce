# Current state

Updated 2026-09-12: user approved a source checkpoint freeze/push after all three
native-return tests passed. **Development checkpoint, not finished production.**
FREEZE-20260912.md records acceptance and open issues. NEXT-ROADMAP.md covers remaining production
blockers and queued Tette/Temperance/package work.

## Provenance and recovery

Local main HEAD a74991fccaa5e4070f62a7ec87642406d9eecf56, with extensive intentional
overhaul now approved for a checkpoint commit/push. Preserve all in-scope work.
Installed binary verified this turn:
f30825c0514f7f72129ededa48f2b5e54a2d31cb764ae971724419d5c01aa57b.
User passed ordinary monitor→tablet→monitor appearance, bottom safety, and edge
entry. Earlier Active-sized tablet→free monitor failure now physically passes.
Earlier Xwayland build 7248d3fb physically passed.
Frozen bundle ../work/kadunce-xwayland-test-20260912; installer
../install-kadunce-xwayland-test.sh --rollback restores
8d033f4dbacbf1f7342cf648f09ed2c88789d1bea8ab3af9084592f51c4eecb3.
Older edge bundle/installer preserves b2b65253e02d3e33ba73334d27b2f7c7776800642bfd13db6d0a106986dacd73.
Initial positive touch dd0ff473499d50b9f326333ee03ac90333c901afbc0822115c9c175ff46e3bcc
and stability 6b752de4f4d9cedbf5ae533716c93135d6cf7c1b003ba2751df27b067a961448 remain preserved.
Trusted repair source.tar SHA:
27f775ecad1e2d132f985950660c8d039eaf015b7e499723cf348cd51c4fa1d9.
Never restore rejected rough swipe ba47bf822343fcf06140a1dbd24f209052fc29d01e7d6fadef2ab6e8be0e6859.
No installed binary/configuration, repair archive or user session changes this turn.
Previous 9b84cd5a is preserved in ../work/kadunce-native-entry-test-20260912;
its installer --rollback returns 7248d3fb. Previous refinement
31fae518395f42f43d9b982a6623fcd3451665fd2df743c40f13ad3ca1a9da13
is in ../work/kadunce-native-refinements-test-20260912; installer
../install-kadunce-native-refinements-test.sh rolls back to previous 9b84cd5a.

## Cross-output return correction

Live NATIVE-RETURN-TRACE-20260912.jsonl shows X11 staged-ordinary → adopted →
destination-none → drop-not-committed. Visual Active sizing was not a managed
source in this trace. NativeDesktop receiver rejected every cross-output landing.
It now permits an unmanaged source to land on a free external output, while
rejecting managed-source, occupied-destination and tablet-destination bypasses.
No source-identity inference, input change, or repeated geometry writes.
New cross-open pointer/touch regression fails on installed31fa at destination
validation (/tmp/kadunce-unload-test.uHpVWH). Genuine X11 Active pickup passes
even on31fa (/tmp/kadunce-unload-test.epqTRo). Fresh candidate gate:
/tmp/kadunce-integrated-carry.NizPos; candidate SHA
f30825c0514f7f72129ededa48f2b5e54a2d31cb764ae971724419d5c01aa57b.
Full gate passed in one invocation: 14 CTests, 15 runtime routes, Bento restoration,
source and control/live safety checks. Extra X11 tablet fixture passed (fyQr7A).
Bundle ../work/kadunce-native-return-test-20260912; opt-in installer
../install-kadunce-native-return-test.sh restores31fa via --rollback.
User physically passed all three return tests. Separate deferred failure:
bottom-exited ordinary window could not re-enter Bento; KDE edge snapping acted
over the existing Bento layout. Cause unproven; do not change this build yet.
See NATIVE-RETURN-TEST.md.

## Latest refinement

Temporary carry capture no longer implies a card-placement outline: ordinary
NativeDesktop return stays captured until release but renders without the outline.
Explicit Bento exit/edge previews remain. Ordinary native bottom release schedules
one position-only correction after native finish, 10px inside work area; oversized
frames keep title bars reachable. Escape/cancel does not schedule correction.
Same safe geometry is used for ordinary owned carry bottom landing. No dock input
change, native resize, retry loop or mid-contact input handback.

Files: Effect.cpp, NativeCarryRuntime.h, DesktopStageController.h, NativeLanding.h.
Final frozen source/build: /tmp/kadunce-integrated-carry.9GmA03. Full gate passed
in one invocation: 14 CTests, 15 runtime routes, Bento restore/unload, source and
control/live safety checks. Opt-in installer --check passes; installed baseline
and trusted repair remain unchanged. See NATIVE-REFINEMENTS-TEST.md. Earlier p2JHZL found local tablet
edge-withdrawal intent must be preserved; this is corrected in the final source.
Tablet-arrival test also now checks its actual Card Line destination, not a
Bento outline. No partial run is being represented as final acceptance.

## Current bounded change

Previously a proven ordinary drag was immediately canceled/adopted as a card.
New source holds contact proof and original restore reservation while KWin moves
the window. No carry input ownership or preview until a top/left/right edge, or
crossing into a tablet/existing monitor card workspace. Same-output free movement
stays native even beside Bento members. Panel input is unchanged; managed-card
pickup remains immediate.

NativeCarryRuntime retires pending entry on release, extra contact, keyboard,
stream loss or cancellation. NativeCarryHandoff cancels its one-turn fallback
after proof without consuming native movement. DesktopStage validates source
generation/topology across native output drift. The acquiring event's pending
motion delta is projected once into the carried pose, since the filter precedes
KWin's native movement. This fixes a real single-jump external-edge rejection
found by the broader gate. On takeover KWin cancels once;
original restore state and pre-cancel visual pose remain distinct. Effect's native
visibility exception preserves the tablet cutoff without hiding dragged windows.
No repeated geometry writes added.

Code: NativeCarryRuntime.h, NativeCarryHandoff.h, NativeMoveTakeover.h,
DesktopStageController.cpp, Effect.cpp. New native-entry/x11-native-entry runtime
tests. Old exit/desktop/X11/trace assertions updated where they required early
ordinary adoption; managed-card and accidental-action checks remain.

## Evidence so far

Build /tmp/kadunce-native-entry-20260912: 14 CTests pass.
Private real-plugin pointer/touch checks:
- Wayland native free movement/release, local/crossed-output edge entry and disable
  before entry: /tmp/kadunce-unload-test.AE9gq1.
- Same Xwayland cases: /tmp/kadunce-unload-test.yaJXru.
- Existing managed carry, held unload/settle unload: /tmp/kadunce-unload-test.8Blvuf.
- Bottom exit/pullback, native departed movement beside Bento and clearance:
  /tmp/kadunce-unload-test.lsonEE. Initial kXiUdz failure expected the old owned
  destination after departure; replacement checks no ownership and native movement.
- Native/Kadunce Xwayland pointer/touch cancellation over real button produces no
  action; fresh clicks work: /tmp/kadunce-unload-test.wYVPIj.
Control package tests and read-only live kill-switch registration pass.
Previous 9b84cd5a binary comes from /tmp/kadunce-integrated-carry.IQIMav/production.
Fourteen CTests, fifteen runtime routes,
Bento restoration/unload, source guards and safety checks pass in composite
validation; see NATIVE-ENTRY-TEST.md for exact logs. The aggregate invocation
stopped on an outdated tablet ordinary-pickup assertion (fIkDlh line63); current
tablet tests passed separately against the same source/output-predicate build.
Do not claim one uninterrupted aggregate success. Hrhi9l was the earlier failed
single-jump version, superseded by IQIMav.

## Prior findings and next tasks

Native-only KWin cancellation reproduced Xwayland's paired cleanup release
without Kadunce (/tmp/kadunce-unload-test.DtD7bu). User approved native-equivalent
behavior if no accidental activation occurs. Gate allows at most one paired
cleanup release, never a new press, and checks actual action/fresh-click recovery
separately. No synthetic release, persistent blocker or delayed disable.
See XWAYLAND-TEST.md for installed build evidence.

At-rest blur was diagnosed as video wallpaper BlurMode=1; Kadunce had no
renderer/carry/layout and KWin Overview was inactive. No wallpaper change.
Physical wins: touch pickup, display transfer, Bento resize/reorder, lone survivor
Active, Ctrl+B either display, Ctrl+S tablet-only, actual bottom exit/pullback,
10px dock clearance, Xwayland entry and safety.

Deferred follow-up: bottom-exit→fresh drag→existing Bento edge re-entry. Then occupied-tablet Active +
incoming edge contention, then new-launch admission (Ghostty stayed free beside
monitor single-card Bento). Those remain open. Stack/motion polish and other
packages stay later roadmap work. No production freeze or repair promotion.
For the remaining architecture-heavy versus lighter-work split, use HEAVY-REMAINING.md.
