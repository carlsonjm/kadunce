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

## Session reach

`docs/ROADMAP-CC.md` § Working model defines what a local session can do that a
cloud session cannot. Check which side of that boundary a step falls on before
proposing it. Anything touching `Effect`, `WorkspaceInputRouter`, packaging or the
installed system requires a local session.

## Rules owned elsewhere

- Verification, the safety control, installation handover and where writing goes:
  `AGENTS.md`.
- What each check proves, reading a probe run, failure classification and the
  test sheet: `docs/TESTING.md`.
