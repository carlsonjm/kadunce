# September 16 accepted checkpoint

This checkpoint freezes the work J physically accepted on September 16, 2026 and records the suite state that future Codex tasks should use.

## Accepted scope

- **Kadunce A2:** constrained launches enter Kadunce ownership, and Bento state is retained as a stack when a touch display enters Card Line. The accepted production replay is `d2a0357`, based on the previously published PM head `bfe761b`.
- **Temperance T1:** ticker width now refreshes when Plasma moves panel ancestors. The functional fix is `8022fc6`; physical acceptance is recorded by `91616a2`.
- **Tettegouche:** remains frozen at accepted `7f54caa`.

J passed the A2 ownership checks and accepted Zen in the small Bento pane when its minimum size permits it. J also passed the restored Temperance ticker on the installed system with expanding spacers: “We're back to normal.”

## Validation at freeze

- Kadunce production build and eight focused ownership CTests passed, plus A2 and A1 private two-output runtime scenarios. Source/control checks passed, including control build/2 tests and read-only live kill-switch verification. Evidence: `/tmp/kadunce-unload-test.N02GHF/session-retry.log` and `a1-regression.log`.
- Temperance final-head native build and all 5 existing CTests passed. Separate real-panel QtTests cover the ancestor-movement regression, tablet/monitor/DPR1.5, adaptive on/off, arrows, task growth/shrink and scrolling real text. The installed plugin is byte-identical to the accepted candidate build.
- Repository diffs passed whitespace checks. Packages staged only under `/tmp`; staged plugin comparisons and dependency checks passed. Kadunce production/test/control sources match accepted `5e91671` exactly; source parity does not claim byte-identical rebuilds from different absolute checkouts.

## Deliberately excluded

- Fixed or equal spacer center experiments
- Shared center-slot and fixed-cap candidates
- Tettegouche B1 commit `fd977bf`
- Kadunce A2.1 investigation work with no accepted production commit
- Temperance instrumentation-only history

## Remaining work

- Fix Bento-derived Card Line live presentation, including undersized content and black margins.
- Finish Tettegouche centered-dock pressure behavior and transfer filename/completion confirmation.
- Complete notification grouping, label casing, popup spacing, power icon sizing, and Control/Tray design-kit alignment.
- Improve Card Line extraction, reordering, labels, and stack-position presentation.
- Continue the roadmap through missing features, refactor/audit and consumer packaging, website/server setup, and website release.

The rolling planning estimate is **5.5 engineering days remaining**. Update that number in `NEXT-ROADMAP.md` as measured delivery speed changes.

## Rollback and publication

Kadunce rollback: accepted A1 `0d970c9` (production-identical PM head `bfe761b`).
Temperance rollback: `10fb70f`. Build/install separately using the normal session
procedure; no history reset or spacer changes. See `OWNERSHIP-A2-20260916.md`
and Temperance `docs/T1-TICKER-GEOMETRY.md` for the runbooks.

Publication uses normal fast-forward pushes. This freeze performs no live
installation, restart, logout, configuration mutation, or branch deletion.
