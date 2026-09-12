# Native appearance and landing refinements — September 12

This candidate changes presentation and release placement, not the mid-contact
input owner. An ordinary window carried across tablet and back to free monitor
space has no placement outline, releases as ordinary, and can be picked up natively
again. Real edge/Bento departure feedback stays intact.

Ordinary native monitor-bottom release receives one position-only translation
inside the work area with 10px clearance. Escape/cancel does not apply this guard.
Oversized windows retain a reachable title bar, never forced smaller. Tablet
edge-withdrawal semantics and ordinary dock input are unchanged.

Installed baseline: 9b84cd5a66165d516666e2a7251ddeb55f3479ce49595b323adc89f844fe9d88.
Candidate: 31fae518395f42f43d9b982a6623fcd3451665fd2df743c40f13ad3ca1a9da13.
Opt-in installer: ../install-kadunce-native-refinements-test.sh.
--rollback restores that baseline; no restart/configuration/repair promotion.

## Evidence

Final full gate: /tmp/kadunce-integrated-carry.9GmA03, exit 0, one complete invocation.
It builds production and disposable tablet-predicate variants from one frozen
source and runs 14 CTests, 15 runtime routes, Bento restoration/unload and control
checks. Native-entry routes include pointer/touch bottom release and cancellation
on Wayland and Xwayland. Tablet route includes held out-and-back appearance,
ordinary release, native repick and subsequent successful tablet admission.
Exact native source matches the frozen production snapshot. Gate logs and private
session logs are preserved in ../work/kadunce-native-refinements-test-20260912/evidence.
Installed/rollback/repair/safety checks pass; no installation performed.

Earlier p2JHZL candidate was superseded: broader testing found that local tablet
edge withdrawal must retain its original NativeDesktop intent. The monitor-only
landing guard now preserves that branch. A new test initially expected a Bento
outline during tablet Card Line arrival; it now checks the correct destination
and actual released state. Those failed runs are not final acceptance evidence.

## Physical checks

1. Hold an ordinary GPT window, cross tablet and return to free monitor space.
   The placement outline should disappear; release and pick up again normally.
2. Drop an ordinary window at the monitor's bottom edge. Its frame should land
   above the dock, without resizing or activating Bento. Canceling should restore.
3. Recheck side-edge Bento entry and bottom-edge Bento exit.

No live installation, session reset or push by the agent. Physical acceptance
and trusted-repair promotion remain separate decisions.
