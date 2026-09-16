# September 16 accepted checkpoint

This checkpoint freezes the work J physically accepted on September 16, 2026 and records the suite state that future Codex tasks should use.

## Accepted scope

- **Kadunce A2:** constrained launches enter Kadunce ownership, and Bento state is retained as a stack when a touch display enters Card Line. The accepted production replay is `d2a0357`, based on the previously published PM head `bfe761b`.
- **Temperance T1:** ticker width now refreshes when Plasma moves panel ancestors. The functional fix is `8022fc6`; physical acceptance is recorded by `91616a2`.
- **Tettegouche:** remains frozen at accepted `7f54caa`.

J passed the A2 ownership checks and accepted Zen in the small Bento pane when its minimum size permits it. J also passed the restored Temperance ticker on the installed system with expanding spacers: “We're back to normal.”

## Validation at freeze

- Kadunce production build and focused ownership tests passed, including the private two-output A2 runtime scenario and source/control checks.
- Temperance native build passed; all 5 CTest checks passed, including the ancestor-movement regression. The installed package matched the candidate used for tablet and monitor acceptance.
- Repository diffs passed whitespace checks.

## Deliberately excluded

- Fixed or equal spacer center experiments
- Shared center-slot and fixed-cap candidates
- Tettegouche B1 commit `fd977bf`
- Kadunce A2.1 investigation work with no accepted production commit
- Temperance instrumentation-only history

## Remaining work

- Fix Bento-derived Card Line snapshots/presentation, including undersized content and black margins.
- Finish Tettegouche centered-dock pressure behavior and transfer filename/completion confirmation.
- Complete notification grouping, label casing, popup spacing, power icon sizing, and Control/Tray design-kit alignment.
- Improve Card Line extraction, reordering, labels, and stack-position presentation.
- Continue the roadmap through missing features, refactor/audit and consumer packaging, website/server setup, and website release.

The rolling planning estimate is **5.5 engineering days remaining**. Update that number in `NEXT-ROADMAP.md` as measured delivery speed changes.
