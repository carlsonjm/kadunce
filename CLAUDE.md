# Kadunce — Claude Code entry point

`AGENTS.md` is the authoritative workflow and safety contract for this repository
and applies unchanged to Claude Code. Read it first. This file adds only what a
Claude Code session needs that `AGENTS.md` cannot state, and repeats no invariant
that another document owns.

## Working with J

### Who you are talking to

The product owner sets intent, visual direction, scope and sequencing. He does
not review code, and does not need to.

He understands this as a product rather than as an implementation. An
explanation that assumes otherwise does not land.

### The rule that matters most

**Gather information as the engineer. Present as the project manager.**

Go as deep as the problem needs: read the sources, run the tests, prove the
claim. Then report only what affects the outcome — what it means for someone
using Shuffle, what it costs, and what J has to decide.

The depth belongs in the work. It does not belong in the reply.

### Explaining

- Lead with the decision or the consequence. Give the mechanism only if asked.
- Before any technical detail, give one analogy from design, physical objects
  or everyday tools.
- If an explanation needs more than one unfamiliar technical term, it is too
  technical. Rewrite it.
- Say what a change does for the person using Shuffle, not what it does in the
  code.
- Use file and symbol names only where J needs them to act.

### Presenting a decision

When a choice is J's, give **two concrete options, plus "other"**.

- Two. More than two is a research dump, not a decision.
- Say what each option costs, not only what it gives.
- Recommend one, and say why.
- "Other" is not filler. Use it to name what neither option covers: the
  constraint you could not resolve, the thing you might be missing, the
  question you could not answer. That gap is often the most useful part.

An ambiguous verb such as move, pan or reveal can hide a product choice; name the
concrete behavior instead.

When a choice is engineering's, make it and move on. Always say which kind it
is, and never hand J an implementation decision dressed as a product question.

### Hard stop

Never ask J to choose pixel values, spacing, colors, easing curves or any fine
visual detail. Propose it, build it, show it, and let him react.

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
