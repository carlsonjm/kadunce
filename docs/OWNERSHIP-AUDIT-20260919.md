# Ownership audit — 19 September 2026

## Scope

Read-only review of Card ownership against `CARD-LIFECYCLE.md`. Covers
`CardWorkspaceState`, the prepared-transaction surface, the two stage
controllers, and the preserved `wip/card-lifecycle-ownership-20260917`
candidate. No production behavior changed. Findings 2 and 3 are reproduced
against real sources, not inferred.

## Finding 1 — ownership has no single representation

`CARD-LIFECYCLE.md` §1 defines exactly three owners: Native, individual card,
Bento pane. No type holds that value.

| Contract owner | Actual location |
| --- | --- |
| Individual card | `CardWorkspaceState::m_windows` and `m_model` |
| Bento pane | `DesktopStageController::m_sessions`, keyed by output |
| Native | represented only by absence from both |

`CardStageController::m_bentoProjectionSession` is presentation provenance and
disclaims membership authority in its own declaration.

`CardWorkspaceState::invariantHolds()` verifies only
`m_model.cardCount() == m_windows.size()`. It cannot observe Bento. §14's first
invariant, one window has one owner, is therefore unenforced: no object can see
all three states at once, so no object can check it.

Consequence: every transition keeps two independent containers consistent by
hand. Roughly twenty `prepare*` entry points and five prepared-transaction types
exist for what the contract defines as at most six directed transitions. Work
that adds a transition grows the matrix rather than reducing it, which is the
shape of the preserved WIP candidate: it adds a sixth prepared type and leaves
`prepareStackAdmission` as a wrapper.

## Finding 2 — presentation voids prepared transactions

A prepared admission or removal is rejected after any of `selectIndex`,
`page`, `pageStack`, or `setPairNeighborSide`. Reproduced on unmodified
`main`: three of four presentation operations void a still-valid reservation,
while a genuine `append` correctly voids it.

This is not a defect in isolation. See Finding 3.

## Finding 3 — prepared tickets snapshot presentation, not a membership delta

`PreparedStackInsertion`, `PreparedRemoval`, and `PreparedAdmission` each copy
the entire `SpreadModel` at preparation time, and each `commit*` writes it
back wholesale with `m_model = prepared.model`. The model carries selection,
page offset, and pair neighbor side, so a ticket prepared before a presentation
change and committed after it reverts that change.

Reproduced: with a ticket held across `selectIndex(1)` and
`setPairNeighborSide(-1)`, a commit that bypasses the revision guard restores
`selectedIndex=0` and `pairNeighborSide=1`, discarding the user's live view.

The single revision counter is therefore load-bearing, and
`WorkspaceStateTest.cpp` asserts it deliberately, including a net-zero
`page(1); page(-1);` case documented as ABA protection.

**Splitting the counter into ownership and presentation revisions is not a
viable isolated fix.** It was implemented, reproduced green against the narrow
symptom, then rejected because it converts a conservative refusal into silent
view corruption. The change was reverted.

The correctable root is the ticket payload: a prepared transaction must carry
the membership, order, and grouping delta it intends, not a whole-model
snapshot. Once a ticket no longer transports presentation state, ownership and
presentation revisions can be separated safely and a held reservation can
survive ordinary paging.

## Consequence for the ownership block

Ticket payload narrowing is a prerequisite of the block, not a separate
cleanup. Atomic group admission and removal cannot be expressed while every
transaction also rewrites presentation.

Suggested order, subject to product approval:

1. Narrow prepared tickets to a membership delta; preserve current
   invalidation semantics unchanged while doing so.
2. Separate ownership and presentation revisions, which becomes safe after 1.
3. Introduce the ownership value as an assertion-only observer across both
   stage controllers; every §14 violation it reports is a pre-existing defect.
4. Make it authoritative and collapse the `prepare*` surface onto it.

## Obstacle — source-shape assertions

`tests/verify-source.sh` contains six ordering assertions over string positions
in `Effect.cpp` and the stage controllers, naming `m_workspace.reset(`,
`m_workspace.commitAdmission(`, `m_bentoProjectionSession`, and
`m_sessions.insert(`. They constrain implementation shape rather than behavior
and will reject correct code produced by step 1 or 4. Convert them to
behavioral coverage before the block, not during it.

## Not found

Dead code is negligible: one unreferenced header, `CarryWindowPaint.h`, at 29
lines. The carry and native-move cluster separates passive observation,
physical input-stream ownership, native-call boundaries, and headless
destination transactions along a real axis and should not be consolidated. The
documentation index is accurate; every live document it lists exists and every
tracked document is listed.

## Verification boundary

Findings 2 and 3 were reproduced by compiling unmodified `CardWorkspaceState.h`
and `SpreadModel.cpp` against a minimal container shim in a sandbox without
Qt or KWin. `WorkspaceStateTest.cpp` compiles and passes unmodified under the
same shim. No Qt, KWin, package, control, or live-session verification was
performed, and no installed or physical behavior is claimed.
