# Xwayland client-drawn drag candidate — September 12

Opt-in installer: ../install-kadunce-xwayland-test.sh. Save work and log out/in
after installing. `--rollback` restores the previously installed diagnostic build.
No agent installation, restart, repair promotion, commit or push was performed.

## Small physical test

1. With GPT a normal window, drag its title bar to a monitor side edge. It should
   enter Bento without first visiting Card Line. Repeat after Ctrl+Esc releases it.
2. Repeat ordinary-window edge entry on the tablet with no existing Active card.
   The occupied-tablet conflict remains a separate open task.
3. Drag a Bento card to the actual bottom edge. It should return to a normal window
   with 10px clearance above the dock's reserved work-area boundary.
4. Disable Kadunce while holding a GPT drag, release over a harmless control, then
   click deliberately. No accidental activation; the deliberate click should work.

Known open: ordinary drags still adopt early; occupied-tablet admission and newly
launched Ghostty joining single-card Bento are not fixed by this candidate.

## Candidate and evidence

Candidate SHA256: 7248d3fbb764eca7d5fcb9895b45dc34cde912307d8ffaf411d47387fef1dc8a.
Rollback: 8d033f4dbacbf1f7342cf648f09ed2c88789d1bea8ab3af9084592f51c4eecb3.
Native source is identical to /tmp/kadunce-integrated-carry.TPi2Le/source/native.
Validation is composite; aggregate invocations stopped on the earlier fixture/
zero-release assertions. Do not describe those invocations as successful.

- 14 CTests: TPi2Le production build (repeated after final test corrections).
- Existing routes: tEvtwS (runtime), BVNSoO (local), LXtI8d (ordinary desktop),
  slewe1 (Xwayland server decoration), b5yjS5 (tablet), iDBaHE (tablet entry),
  ALzpRs (Card Line), 360XuL (bottom exit/clearance), Oix6WO (tablet desktop),
  NdmT2w (bounded trace). These are /tmp/kadunce-unload-test.* directories.
- New Xwayland ordinary/Bento pointer/touch entry and rejected requests, held
  unload: /tmp/kadunce-unload-test.DcWRwX.
- Native-only baseline: /tmp/kadunce-unload-test.DtD7bu.
- Real underlying button remains inactive through pointer/touch cancellation;
  fresh clicks work, with and without Kadunce: /tmp/kadunce-unload-test.5AtSWO.
- Two-output Bento restoration/unload: /tmp/kadunce-bento-candidate.P3BkQm.
- Source, control package and read-only live kill switch pass; graphical-session
  startup remains required. Installed binary and trusted repair hashes unchanged.

Xwayland synthesizes a matching mouse-up when an X11 window regains pointer focus,
also with native-only cancellation. The user approved native-equivalent cleanup,
not arbitrary releases or accidental actions. The entry fixture checks bounded
paired cleanup and no new press; the action fixture tests actual QPushButton
activation and fresh-click recovery. Never substitute the counter check alone.
