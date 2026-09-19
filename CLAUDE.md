# Kadunce — Claude Code entry point

`AGENTS.md` is the authoritative workflow and safety contract for this repository
and applies unchanged to Claude Code. Read it first. This file adds only what a
Claude Code session needs that `AGENTS.md` cannot state, and repeats no invariant
that another document owns.

## Startup

Follow the `AGENTS.md` startup set in order, every task. Do not substitute a
summary, a previous session's recollection, or a memory entry for reading those
four files.

## Which checkout

The working tree is `Projects/Shuffle/kadunce`. A second checkout at
`Projects/Itasca/kadunce` predates the current execution plan and carries its own
worktrees and candidate builds. Confirm the path before reading or editing.

## Suite position

Kadunce is one of three open-source component repositories. `docs/ROADMAP-CC.md`
is the execution plan for the whole suite, so Tettegouche and Temperance blocks
are ordered from here while their task detail stays in their own repositories.
`docs/NEXT-ROADMAP.md` supplies Kadunce's per-component task detail and is read
when a block is picked up, not at startup.

## Session reach

`docs/ROADMAP-CC.md` § Working model defines what a local session can do that a
cloud session cannot. Check which side of that boundary a step falls on before
proposing it. Anything touching `Effect`, `WorkspaceInputRouter`, packaging or the
installed system requires a local session. `tests/verify-headless.sh` is a
pre-check and never promotion evidence.

## Verification

- Every change: `./verify.sh` — documentation, source, package and control checks.
- Domain-layer pre-check: `bash tests/verify-headless.sh`.
- After an authorized installation, in the graphical session:
  `bash tests/verify-live-control.sh`, then confirm the controller is wanted by
  `graphical-session.target`.

`tests/verify-docs.py` guards index coverage, archive isolation, `SWARM.md` limits
and `CURRENT_STATE.md` hygiene. A new tracked document must be added to
`docs/README.md` in the same change or the guard fails.

## Stop conditions

Unless the assigned task authorizes it in words, do not install, publish, log the
user out, stop the graphical session, or toggle the live effect.

The tray enable/disable control is release-critical. A safety-control failure
blocks promotion and is never waived by passing effect tests. A sandbox or D-Bus
transport denial is a test-environment result, not evidence about the control;
classify it with `docs/TEST-ENVIRONMENT-PROCEDURE.md`.

## Writing into this repository

- Ordering goes in `docs/ROADMAP-CC.md`; durable decisions in `docs/DECISIONS.md`;
  current behavior in `docs/CURRENT_STATE.md`; a live cross-agent dependency in
  `SWARM.md` and nowhere else.
- Keep progress narration, worker summaries, candidate hashes and test logs out of
  live documents. Git carries history.
- Code comments explain code behavior and reasoning only — never handoffs,
  authorship, product instructions or agent conversation.
- Terminology follows `docs/TERMINOLOGY.md`. The retired workspace term was
  replaced by Spread in one mechanical commit under Block 1b. Three layer-3
  identities are deliberately frozen until Block 10b; `tests/verify-source.sh`
  names them and rejects every other occurrence.
