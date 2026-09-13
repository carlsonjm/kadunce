# J / A / B Team — remaining MVP engineering blocks

**Priority reset:** MVP-RELEASE-SCOPE.md overrides the mandatory K0–K6 sequence
below. Dock safety + new-app Bento admission → J acceptance → freeze/push is the
current batch. These older packets are reference material, not an automatic queue.

September 12, 2026. Authoritative execution queue after checkpoint `faee199`.
This replaces execution ordering in older refactor/roadmap documents, not their
technical evidence. CURRENT_STATE.md remains authoritative for installed state.
This queue alone grants no installation, publication or automatic dispatch.
Current exception: J assigned A-Team takeover of K1 on September 12 and then
reassigned B to Temperance. See CURRENT_STATE.md and K1-TAKEOVER-AUDIT.md;
the native repair is privately validated, not yet packaged/physically accepted.

## Command chain

- **J-Team:** think tank, lead designer, project manager and physical test lead.
  Chooses priorities/product behavior, approves scope and physical acceptance.
- **A-Team / Astra:** lead engineer and co-architect. Converts J's intent into
  contracts, reviews boundary changes, resolves engineering escalations, and
  supplies the next bounded packet. Not the default implementer of every hard task.
- **B-Team / Sol:** senior engineer and implementation partner. Owns each assigned
  packet from source inspection through tests, candidate and handoff. Challenges
  inconsistent contracts with evidence; does not silently invent product policy.

Assign one packet at a time. B-Team first for every block below. A reviews the
proposed ownership change for K2/K3/K4/K5 before B implements it. J approves any
new interaction behavior. Escalation is a decision request, not a failed assignment.
No agent spawning or other-task creation is implied by this document.

## MVP boundary and sequence

The effect conversion, core controllers, native carry and baseline transfers
already exist. Do not rebuild them. MVP closes lifecycle holes and completes the
promised ordered-stack/free-carry interaction, then validates touch continuity.
Persistent stacks across unload, an always-on service, a new renderer framework,
new tiling modes and the full Tette file manager are NOT prerequisites.

| Block | Outcome | Dependency | Completion gate |
| --- | --- | --- | --- |
| K0 | Baseline and reproducible handoff | None | Provenance + test coverage report; no fixes |
| K1 | Bottom-exited window can re-enter Bento | K0 | Reproduction fails before/passes after; J tests full loop |
| K2 | Occupied tablet accepts or rejects one whole transfer | K1 | Transaction/cancellation matrix + J display test |
| K3 | New eligible windows join the owning Bento workspace | K2 | Launch lifecycle tests + Ghostty/slow-launch physical check |
| K4 | Native carry reaches ordered stack slots from either side | K3 | N+1 slots, exact cancellation/order + J touch test |
| K5 | Continuous input/presentation without competing motion | K4 | Measured fast/reverse/regrab behavior + J acceptance |
| K6 | Release candidate, recovery and reproducible package | K1–K5 | Fresh automated gate + physical matrix + separate release approval |

K0–K3 form the correctness milestone. Freeze it before stack/motion work.
K4 and K5 are separate experience milestones, not an invitation to retune all
animations while repairing admission. K6 closes the MVP; anything newly discovered
is triaged as a blocker or deferred explicitly, not silently added to every “next.”

## Universal engineering contract

1. KWin owns real windows; Kadunce controllers own card membership, restore
   records and output-local arrangements. Geometry is not proof of membership.
2. Native ordinary movement stays native until intentional workspace/edge entry.
   Existing Bento receives incoming cards; free monitor space without Bento is
   ordinary desktop. Same-display ordinary movement beside Bento stays ordinary
   until explicit entry. Preserve bottom-edge departure and dock input.
3. Prepare and validate destination before committing source removal. Snapshot
   identity/revision/output; revalidate at release. One commit, one native apply
   path. Rejection preserves source; cancellation cannot resurrect stale work.
4. Rendering is read-only. Free carry follows the contact in two axes. No geometry
   correction loop, parallel membership registry, synthetic mouse release, global
   input blocker or blanket snapping toggle to hide an ownership failure.
5. Keep passive tablet cards clipped to their output. Only the carried face crosses
   outputs. Preserve the independent tray kill switch and immediate disable.
6. Preserve accepted Xwayland native-equivalent cleanup, with no accidental action
   and working fresh input. No timing rewrite or resurrection of rejected swipe code.
7. No user-session toggle, restart, repair promotion, install, commit or push unless
   separately authorized. Do not inherit last packet's publication permission.

## Assignment header — prepend to every block prompt

Socket/testing prerequisite: TEST-ENVIRONMENT-PROCEDURE.md. Do not diagnose
sandbox transport errors as production failures or repeatedly ask J to rerun tests.

```text
You are B-Team, Senior Engineer. J-Team owns product direction and physical
acceptance; A-Team owns architecture review. Implement only the assigned packet.
Work in the Kadunce repository containing this document unless the packet names
another repo. Read AGENTS.md, docs/CURRENT_STATE.md, this packet and the
relevant current-implementation section of docs/ARCHITECTURE.md. Inspect actual
git status/HEAD and relevant source; old prose is not proof of runtime behavior.
Do not reread entire chats or every historical document. Frozen starting
checkpoint is faee199; later approved commits may supersede it—report drift,
do not reset. Record source, installed binary and rollback separately.
Before editing, state the failing invariant, existing owner and smallest proposed
change. For an architecture-review gate, stop after that proposal for A-Team.
Preserve unrelated work. Add a regression which distinguishes old failure from
desired behavior. Never weaken a test solely to make the new code pass.
Run focused tests while iterating; run the fresh integrated/safety gate once on
the final candidate when runtime changed. No physical pass claims from fixtures.
If two distinct fix attempts do not resolve the same evidence-backed failure,
or the fix needs new ownership/product policy, stop and send an escalation packet.
Do not start the next block, install, restart, promote repair or publish.
End with the handoff format in ENGINEERING-BLOCKS.md and update CURRENT_STATE.md.
```

## K0 — establish the receiving engineer's baseline

```text
Assigned K0, read-only baseline. Verify HEAD/status, installed plugin hash,
checkpoint FREEZE-20260912.md and NATIVE-RETURN-TEST.md. Confirm production native
source against the accepted snapshot if available; missing /tmp evidence is not
proof of failure. Inventory current integrated tests and independent safety
control, including graphical-session.target wiring. Identify which assertions
actually cover bottom exit followed by a NEW drag into existing Bento, not just
first entry or movement after exit. Deliver a compact evidence/coverage table
and exact K1 reproduction steps. Do not rebuild unchanged code merely for
orientation, fix behavior, install anything or scan other projects.
```

Gate: A can identify the starting revision, known failure, test gap and rollback
without reading logs. K0 should not become a fresh audit of the whole ecosystem.

## K1 — released-window re-entry (first implementation packet)

```text
Assigned K1. Reproduce: Bento with survivors → bottom-edge release to ordinary
window → release contact → fresh grab → side/top edge of the same existing Bento.
Observed failure is KDE snapping over Bento, not successful Kadunce admission.
Compare fresh ordinary window, bottom-departed window and Ctrl+Esc-released window;
include one-member Bento teardown versus surviving multi-card Bento.
Start with NativeCarryRuntime.h, NativeCarryHandoff.h, Effect.cpp's
updateNativeCarryDestination, DesktopStageController.cpp and the existing
exit/native-entry/Xwayland runtime probes. Inspect nativeMoveTrace to distinguish
missing source proof, stale eligibility, invalid destination and rejected commit.
Do not assume the cause. Add the complete-sequence regression, then fix the
responsible lifecycle boundary using the universal contract. Preserve ordinary
free motion beside Bento, round-trip transit, bottom clearance and no dock click.
Test pointer and touch, Wayland and Xwayland where fixtures support them. Prepare
an opt-in candidate only after final gates. Give J three short physical tests.
```

Escalate if successful re-entry requires changing contact ownership, permanent
exclusion after exit, or global KDE snapping policy. Otherwise B owns the fix.

## K2 — occupied-tablet admission, one transaction

K2a is tests + proposed transaction; K2b is implementation after A review.

```text
Assigned K2a only. Reproduce incoming ordinary/card/Bento window against tablet
Active, Card Line and Bento; include inactive tablet as control. Distinguish open
arrival from deliberate edge entry. Review CardStageController's prepared native
source/transfer paths, DesktopStageController prepareCardDrop/transfer paths,
PreparedCarrySource.h, CardWorkspaceState.h and BentoSessionTransfer.h.
Map prepare → source commit → destination publication → native application →
interruption. Show where incoming and resident restore records are captured and
which generation invalidates the plan. Propose reuse of existing transactions,
not a second transfer coordinator. Add a reproducing test if safe; stop for
A review before changing runtime ownership.
```

K2b continuation: implement only the reviewed plan. Matrix: source types × target
presentations; target closes, incoming closes, output disappears, minimum sizes
reject, held disable, repeated release. Successful edge promotes eligible residents
once; rejected admission keeps the source. No oscillation or monitor bounce.
J tests occupied tablet from ordinary monitor window and existing monitor card.
A should take engineering only if preparation cannot isolate current side effects
or reentrant cleanup leaves conflicting restore owners after B's analysis.

## K3 — new-window admission and readiness

K3a contract/report, K3b basic launch, K3c slow/helper launch; separate candidates.

```text
Assigned K3a only. Trace new eligible-window admission on an output with existing
Bento, especially its single-card Active presentation. Ghostty previously opened
free instead of joining. Inspect Effect.cpp window lifecycle connections,
DesktopStageController session admission, CardStageController finishNewArrival,
and LaunchIdentity code found in the repo. Read Tette launch code only if needed
to identify the boundary; do not change Tette in this packet.
Specify readiness, destination ownership, splash/transient exclusion, late main
window, close-before-ready, user movement/focus changes and cancellation. Use
window identity/output/session revisions, not app-name patches or longer sleeps.
Identify policy gaps for J/A: overflow, late launch after workspace release, and
whether a user's intervening move overrides launch destination. Add fixtures
for observed behavior and propose a bounded admission state machine. Stop for review.
```

K3b implements approved ordinary launch admission first. K3c adds only evidenced
late/helper lifecycle cases (Affinity splash, Steam helper as test cases, not full
Steam discovery). No automatic migration of unrelated-display windows, timeout
retry loop, duplicate membership or stale admission after release. Missing real
Affinity/game availability is a physical-test gap, not permission to fake a pass.

## K4 — one ordered-stack destination for both carry paths

K4a model/contract, K4b runtime producer, K4c physical slot behavior.

```text
Assigned K4a only. Existing Card Line insertion is implemented. Inspect
CardWorkspaceState::prepareStackInsertion/commitStackInsertion,
CardStageController preview/slot functions, WorkspaceInputRouter and the native
carry destination callback. Show how native carry can produce the SAME immutable,
identity/revision-bound insertion intent without duplicating stack membership.
Specify N+1 slots from either approach, selected identity, cancellation, last
member detach, target replacement/closure and source restoration. Confirm which
destinations current UI actually exposes. Do not invent offscreen-stack browsing,
a chooser, or Bento stacking policy; flag those choices for J. Provide model
tests and a runtime integration plan, then stop for A review.
```

K4b implements the reviewed adapter and rejects stale slots at commit. K4c tests
left/right approach, reverse approach, reinsert and cancel physically; tune a
specific slot affordance only after J approves the measured issue. No global dwell
retiming in K4. A escalation: native and router carry cannot share destination
identity without overlapping input owners, or product stack targeting is ambiguous.

## K5 — measured motion/input convergence

K5a instrumentation only; K5b one interruption seam; K5c visual tuning.

```text
Assigned K5a only. Read the existing KDE_RESEARCH, KDE-REUSE-AUDIT and unified-card
motion contracts; do not repeat broad research unless a specific version/API gap
requires it. Map ownership for Card Line swipe, held native carry, release settle,
regrab, reversal and cancellation. Measure bounded input timestamps, presented
pose and frame intervals for slow drag, fast flick, reversal and regrab during
settle. Reuse current pose capture and animations. No per-frame persistent logs
or new motion engine. Deliver the single largest measured discontinuity and a
small proposed correction; separate latency from easing/appearance. Stop for
A review and J's interaction choice before timing or ownership edits.
```

K5b corrects one seam, with no jump when interrupted and no retained input after
release. K5c adjusts one approved visual parameter group against the same trace
and physical gestures. Never transplant “iPad milliseconds” as a universal rule.
If routing/geometry must be rewritten, A reviews the boundary before B continues.

## K6 — explicit MVP exit gate

```text
Assigned K6. No feature changes. Produce a fresh-source candidate with its exact
revision/hash and rollback, run integrated tests plus K1–K5 additions, and verify
independent control startup, unload and recovery. Prepare J's short batches:
(1) ordinary/card/Bento entry-exit cycles, (2) occupied tablet and new launches,
(3) left/right stacks + rapid touch reversal/regrab, (4) fullscreen/maximize/
minimize/manual resize + monitor removal/rotation/scaling, (5) Tette dismissal,
dock taps and disable while held. Record hardware-only/unavailable cases as gaps.
Do not mark failed tests passed because another scenario succeeds. Classify any
new report as blocker or deferred with J/A. After physical approval request
separate decisions for commit/push, installation and trusted-repair promotion.
```

Done means no untriaged correctness/input/recovery failures in the tested matrix,
J accepts touch behavior, and remaining limitations are explicit. This is not a
claim of universal hardware/backend support or completion of companion apps.

## Companion apps — separate lane, not Kadunce MVP scope

Historical findings in NEXT-ROADMAP E1–E12 and ../ECOSYSTEM-NEXT-UPDATE.md must be
rechecked in each app's current source. Do not carry old install authorization.

| Packet | Scope / source entry points | Placement and gate |
| --- | --- | --- |
| A1 | Tette semantic Meta open/close: src/TettegoucheApplet.cpp, src/main.cpp, applet/main.qml | Good break after K1; rapid toggles, in-flight launch, guest cleanup, bridge absent. Do not include fullscreen dock changes |
| A2 | Temperance banner layout: qml/main.qml and actual banner delegates found there | Low-risk break after K2; short/long/image/action/critical, scales and touch targets; no notification ownership edits |
| A3 | Tette search/child visuals: current result delegates + docs/SEARCH-CONTRACT.md | Break after K3; preserve ranking/scopes/identity/actions and keyboard/touch paging |
| A4 | Temperance truthful AC display: qml/main.qml and current power source | Read-only state matrix first; AC charging/holding/discharging/unknown; no battery-policy change or bypass/health claim |
| A5 | Notifications disabled → stock presenter: Temperance `src/systemtray.cpp` + `qml/main.qml`; source handoff in `../../temperance/docs/NOTIFICATION-PRESENTER-HANDOFF.md` | Source investigation complete. Keep the shared notification server; switch applet presenters behind a readiness gate. Dedicated reviewed ownership packet; startup/toggle/DND/actions/history, no lost/duplicate presentation or competing servers |
| A6 | Tette Active/Bento guest + fullscreen Meta/dock | After Kadunce correctness milestone; contract first across WorkspaceContext and Kadunce guest endpoint. Correct output, focus restoration, no game retile, bridge fallback. J decides collapse policy |
| A7 | Steam/external game catalogue and launch | After K3; Steam owns game/library/Proton resolution. Cold/warm/helper/game/missing drive; no raw binary launch or boot mount |
| A8 | Tette All Files | Separate feature project after guest contract: F0 feasibility only first; reuse KDE listing/jobs/actions, prove job lifetime. Full F1–F5 stays in NEXT-ROADMAP, not a “break” |
| A9 | README/artwork/package consistency, system-corners maintenance inventory | Safe docs/package break, inspect existing status first; three repos remain independent, no global hooks or repair promotion |

To assign an app packet, A supplies a fresh exact file list after B's short source
inventory. These entries are boundaries, not authorization to implement all nine.
Use the same assignment header with the app's repo and one row's acceptance.
App releases have independent physical acceptance and publication decisions.

## Handoff and escalation format

Every B handoff fits this structure; link logs rather than dumping them:

```text
Packet / status: proposed | implementing | automated pass | awaiting physical | blocked
Baseline: commit, initial dirty files, installed hash; runtime changed yes/no
Cause: evidence → violated invariant → existing owner (or explicitly unknown)
Change: files/functions, why this is the smallest contract-preserving fix
Tests: failed-before evidence, passed-after, preserved paths, untested conditions
Physical: at most 3 short steps for this candidate, expected outcomes, rollback
Decision needed: exact ambiguity + options/tradeoff; no hidden default
Next: stop at this packet; record CURRENT_STATE and do not begin another
```

On escalation include the smallest repro, trace around first divergence, attempted
fixes/diffs and why rejected, and the boundary needing a decision. Preserve the
candidate and do not stack speculative patches. A may resolve the contract and
return implementation to B, or take that one engineering slice with J's approval.

Resource discipline: prefer focused reads/tests and one final broad gate over
repeating audits; no effort/token promises. J's physical pass is its own gate.
Useful app work may fill a wait for hardware testing only when J assigns it.
