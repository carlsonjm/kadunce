# Current state

Updated September 13, 2026. J accepted the interaction-only candidate and requested
a micro freeze on main, then the placement-continuity pass. Read ARCHITECTURE.md
for owners, DECISIONS.md for behavior, MVP-RELEASE-SCOPE.md for scope discipline.

## Accepted main freeze

Interaction candidate built from5ba19e6 is now the accepted source baseline.
Installed plugin SHA verified after J's physical pass:
`71e5bf843ce357150ce31865597bb74f901be983a87e00e0b2fff1623762e24f`.
Bundle: ../work/kadunce-interaction-20260913. Installer:
../install-kadunce-interaction.sh; its --rollback returns to previous38a2 baseline.
No compositor restart or installation was performed by the freeze operation.
Publication and trusted-repair promotion remain separate from this local freeze.

J passed separation of stack insertion from held row paging, improved placement,
and held Card Line paging. Geometry, selection continuity and animation remain
experience work, not reasons to roll back the accepted gesture behavior.

The router requires fresh horizontal contact movement for one insertion-slot
request after dwell. Entry alone does not page. An end slot does not start row
paging. Physical screen-edge contact permits row paging; each repeat revalidates
contact and geometry. Approach side initializes insertion end. Timing, resting
size and ownership are unchanged. No44% scale/pose experiment is included.

Focused panel-input/workspace-state/card-line-model3/3, source guards and control
package2/2 passed. Router tests use real membership state with simulated compositor
adapter. The stationary-entry regression failed on frozen source before the fix.
J supplies physical acceptance; tests do not prove all geometry sequences.

Accepted earlier behavior remains: cross-display ownership, dock-safe release,
tablet/monitor Bento bottom departure, visible new-app Bento admission, automatic
native edge-tiling/maximize suppression, idle bottom swipe reach. Do not rework
these for stack polish. Shift-custom-tiling and explicit keyboard states are distinct.

## Next bounded pass: placement continuity

J observes correct-looking cyclic order but newcomer jumps forward visually:
intended A,D,B,C appears D,B,C,A. Existing insertion explicitly selects the
new member; distinguish stored order from active member and paint elevation.

Approved next pass:
- Keep destination selection on insertion unless user explicitly selects a card.
- Align stack preview and released geometry; no changes to accepted paging.
- Make insertion readable among similarly dark windows through a slot cue/gap.
- Then timing and animation polish, not a new motion engine or sizing experiment.

Verify value-model insertion selection, controller elevation cleanup and preview
rendering together. J tests placed card staying in its slot, browsing order and
readable preview. No automatic install/push. Occupied-tablet arrival is considered
solved; Affinity splash is deferred. B owns assigned Temperance work.

## Preserved rejected work / recovery

Failed44%/pose experiment is removed from main source, preserved recoverably in
../work/kadunce-pre-interaction-freeze-20260913/local-source-docs.tar.gz
SHA b63982c8fc21bff444d8c3114e78d0bda08912fac83299c1c55dfb7e7593153a.
Original candidate bundles ../work/kadunce-stack-scale-20260912 and
../work/kadunce-stack-approach-20260912 remain. Failed scale installer blocks install.
No rejected source was pushed or promoted to trusted repair.

KWin6.7.5-1.2 engine patch is separate; do not rebuild it. Provenance under
patches/kwin. Original engine rollback package remains under
../work/kwin-touch-repair-20260912/rollback. Trusted repair archive remains
unpromoted SHA27f775ecad1e2d132f985950660c8d039eaf015b7e499723cf348cd51c4fa1d9.
Never restore rejected rough-swipeba47bf.
