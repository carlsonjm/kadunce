# Repository context and startup

For every task, read only this startup set, in order:

1. `AGENTS.md`
2. `docs/CURRENT_STATE.md`
3. `docs/ROADMAP-CC.md`
4. `SWARM.md`

Then run `git fetch --all` and check the `origin/claude/*` branches. Cloud sessions
land work there, and one branch can be ahead of `main` in several Shuffle
repositories at once. Read a newer one before planning, and reconcile it against
what `main` holds, not against the state it forked from, before merging.

Inspect the branch, working tree, and only the source or reference documents the
assigned work needs. `docs/README.md` classifies the remaining documents. Do not
read `docs/archive/` during normal startup; use archived evidence only to diagnose
a regression, answer a provenance question, or revisit a failed candidate. For any
card, Bento, admission, release, or Shuffle navigation task, read
`docs/CARD-LIFECYCLE.md` before inspecting implementation.

Claude Code loads `CLAUDE.md` automatically. It routes into this same startup set
and adds no separate protocol.

## Holds

These hold in every Shuffle repository.

- J approves product behavior and visual direction before implementation begins.
  Engineering may present evidence, constraints and alternatives; an unapproved
  proposal does not become a candidate.
- One implementation owner per repository. A second worker is read-only review or
  a disjoint file set.
- Reproduce a defect and measure the property controlling it before changing it.
- Components never depend on private product features.

## Work packets

Keep assignments compact and ordered: repository and roadmap item; required
outcome; task-relevant contracts; acceptance checks; stop conditions; permissions
already granted. Omit history and unrelated reading. If another worker must act,
reduce the dependency to one `SWARM.md` handoff and delete it when resolved.

## Where writing goes

- `docs/ROADMAP-CC.md` is the only execution plan and owns block order,
  dependencies and open product decisions. It is a checklist, one line per task,
  and J follows it directly; tick a finished task there and keep it one.
- What was measured or learned goes in `docs/ROADMAP-CONTEXT.md` under the
  block's heading; read that section for the block being worked on.
- Durable decisions go in `docs/DECISIONS.md`.
- `docs/CURRENT_STATE.md` describes current behavior, limitations,
  source/installed state and validation only. Replace stale text; never append
  progress notes.
- A live cross-agent dependency goes in `SWARM.md` and nowhere else: at most three
  handoffs of at most 50 words each, removed when resolved. No backlogs, status
  reports, history or completed handoffs.
- Keep progress narration, worker summaries, candidate hashes and test logs out of
  live documents. Git carries history.
- Code comments explain code behavior and reasoning only, never handoffs,
  authorship, product instructions or agent conversation.
- Terminology follows `docs/TERMINOLOGY.md`. Three layer-3 identities keep the
  retired workspace term until Block 10b; `tests/verify-source.sh` names them and
  rejects every other occurrence.
- A new tracked document is added to `docs/README.md` in the same change, or
  `tests/verify-docs.py` fails.
- When a task is ticked, reduce its context to the durable finding; the story of
  how it was found goes in the commit message. `docs/README.md` § Keeping
  documentation small states the word budgets `tests/verify-docs.py` enforces.

# Safety control

Kadunce's persistent tray enable/disable switch is release-critical, including
when the workspace effect is disabled or incompatible. Never remove it, make it
optional, or treat effect tests alone as release evidence. A safety-control
failure blocks promotion and is never waived by passing effect tests.

A sandbox, D-Bus or transport denial is a test-environment result, not evidence
that the switch is missing; classify it with `docs/TESTING.md` § Failure
classification. Preserve private/live bus separation.

Never probe the live session: do not start `plasmashell`, script panels, or kill
a process by `$PPID` outside a private compositor. A nested Plasma-shell probe
once froze the machine. Private compositor rules are in `docs/TESTING.md`.

## Verify

Run `./verify.sh` before calling any change complete, and check its exit status
rather than a pipe's. `docs/TESTING.md` says what each further check proves and
when a candidate is promotable.

## Installation handover

Installing, restarting the graphical session and logging out are performed by J,
never by an agent, and no task packet changes that. An agent builds and verifies
the candidate and hands the installation over as one copy-pasteable command;
`docs/TESTING.md` § Handing over an installation states what the handover
carries. After J installs, `bash tests/verify-live-control.sh` runs in the
graphical session and confirms the controller is wanted by
`graphical-session.target`. Do not toggle the live effect or publish unless the
task says so in words.
