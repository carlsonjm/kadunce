# Documentation index

Canonical documentation states what is true now, why it is true, and what must be
preserved. Reference documents define live interfaces, tests, product contracts, or
operational procedures. Archived evidence explains how the current state was reached
and is not part of normal startup.

## Normal startup

Read only:

1. `../AGENTS.md`
2. `CURRENT_STATE.md`
3. `ROADMAP-CC.md`
4. `../SWARM.md`

Then open only the references required by the task. Consult `archive/` for a
regression, provenance question, or failed-candidate diagnosis.

Run `../verify.sh` for the repository's source, package, safety-control, and
documentation checks. The documentation guard enforces archive isolation, compact
runtime handoffs, current-state hygiene, and index coverage.

## Canonical

| Document | Authority |
| --- | --- |
| `../AGENTS.md` | repository workflow and safety requirements |
| `../CLAUDE.md` | Claude Code entry point; routes into the `AGENTS.md` startup set |
| `../README.md` | public package and developer entry point |
| `../SWARM.md` | live cross-worker dependencies only |
| `README.md` | document classification and routing |
| `CURRENT_STATE.md` | current behavior, limitations, provenance boundary, validation |
| `ROADMAP-CC.md` | the only execution plan, as a checklist: what is next, block order and status, one line per task, open product decisions |
| `ROADMAP-CONTEXT.md` | the reasoning, measurements and history behind each block, under the same headings; product boundary, execution policy and planning controls |
| `ARCHITECTURE.md` | subsystem ownership and invariants |
| `DECISIONS.md` | durable subsystem-grouped decision ledger |
| `PRODUCT-CONTRACT.md` | accepted Kadunce product behavior |
| `CARD-LIFECYCLE.md` | canonical card ownership, presentation, transition, and navigation contract |

## One owner per invariant

An invariant is stated once. Other documents cite the owner rather than repeat
its wording, so a behavior change cannot leave two documents disagreeing. When
wording conflicts anyway, the owner governs.

| Invariant class | Owner |
| --- | --- |
| Card ownership, presentation, transition, navigation | `CARD-LIFECYCLE.md` |
| Subsystem authority, transfer order, input ownership, teardown | `ARCHITECTURE.md` |
| What a user is promised, including gestures and release | `PRODUCT-CONTRACT.md` |
| Why a decision holds, and what it rejected | `DECISIONS.md` |
| What is true now, what is missing, what has been validated | `CURRENT_STATE.md` |
| Block order, dependencies, open product decisions | `ROADMAP-CC.md` |
| Why a block's tasks are shaped as they are, and what was measured | `ROADMAP-CONTEXT.md` |

`DECISIONS.md` records rationale, not restated rules. `CURRENT_STATE.md` reports
status against the contracts and never redefines them.

## Reference

| Document | Live contract |
| --- | --- |
| `CARRY-SESSION-CONTRACT.md` | prepared transfer lifecycle and invariants |
| `INPUT-OWNERSHIP.md` | input routing, proof, cancellation, and teardown |
| `INTEGRATION-RELEASE-GATE.md` | candidate promotion evidence |
| `REFACTOR-REGRESSION-GATE.md` | behavior preserved across structural change |
| `KNOWN-ISSUES.md` | current limitations and compatibility constraints |
| `TEST-ENVIRONMENT-PROCEDURE.md` | private/live test separation and failure classification |
| `TETTEGOUCHE-CONTEXT.md` | versioned context and guest D-Bus API |
| `TERMINOLOGY.md` | suite-wide approved and retired language, and the rules for applying it |
| `ITASCA-VISUAL-LANGUAGE.md` | shared visual and motion grammar |
| `KADUNCE-TABLE-1.1-CONCEPT.md` | required Table product contract and feasibility gate |
| `SHUFFLE-KEYBOARD-1.0-CONCEPT.md` | required keyboard product contract and feasibility gate |
| `../patches/kwin/README.md` | version-bound native touch correction |
| `../patches/kwin/package/README.md` | installed package and rollback provenance |
| `../tests/unload-probe/README.md` | private unload/takeover harness contract |

These references are retained outside the archive because changing Kadunce safely
still depends on them.

## Historical evidence

`archive/README.md` indexes every archived freeze, candidate audit, physical result,
investigation, and pre-normalization snapshot. An old `docs/NAME.md` reference maps to
`docs/archive/NAME.md` when that filename exists. Pre-normalization snapshots use a
date or `THROUGH-YYYYMMDD` suffix and retain the old content verbatim.

## Obsolete or redundant

The following completed coordination or superseded planning documents were deleted.
Their last pre-normalization versions remain recoverable from Git revision `125806b`:

- `ENGINEERING-BLOCKS.md`
- `HEAVY-REMAINING.md`
- `KDE_RESEARCH.md`
- `MORNING-CHECKLIST.md`
- `MVP-RELEASE-SCOPE.md`
- `REFACTOR-PLAN.md`
- `ROADMAP-HISTORY-20260913.md`
- `UNIFIED-CARD-BLOCKS.md`
- `UNIFIED-CARD-OWNERSHIP.md`

No current contract links to these files. Their active scope, where still relevant,
is represented in `ROADMAP-CC.md`, `ARCHITECTURE.md`, or `DECISIONS.md`.
