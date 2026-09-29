# Repository context and startup

For every task, read only this startup set, in order:

1. `AGENTS.md`
2. The suite record, kept privately in the Shuffle repository and read from its
   checkout beside this one: `../shuffle/docs/suite/CURRENT_STATE.md`,
   `../shuffle/docs/suite/ROADMAP-CC.md` and `../shuffle/docs/suite/SWARM.md`.
   Its `README.md` says how the record is written. When it is not checked out,
   say that the suite plan was unavailable rather than inventing an order.

Then run `git fetch --all` and check the `origin/claude/*` branches. Cloud sessions
land work there, and one branch can be ahead of `main` in several Shuffle
repositories at once. Read a newer one before planning, and reconcile it against
what `main` holds, not against the state it forked from, before merging.

Inspect the branch, working tree, and only the source or reference documents the
assigned work needs; `docs/README.md` routes each subject to its document. Archived
evidence is no longer kept in this repository; maintainers hold it privately
for diagnosing a regression, a provenance question, or a failed candidate. For any
card, Bento, admission, release, or Shuffle navigation task, read
`docs/CARD-LIFECYCLE.md` before inspecting implementation, and for any input,
`docs/INPUT.md`.

Claude Code loads `CLAUDE.md` automatically. It routes into this same startup set
and adds no separate protocol.

## Holds

These hold in every Shuffle repository.

- The maintainer approves product behavior and visual direction before
  implementation begins.
  Engineering may present evidence, constraints and alternatives; an unapproved
  proposal does not become a candidate.
- One implementation owner per repository. A second worker is read-only review or
  a disjoint file set.
- Reproduce a defect and measure the property controlling it before changing it.
  A fix whose symptom cannot be reproduced is not yet a fix.
- Components never depend on private product features.

## Work packets

Keep assignments compact and ordered: repository and roadmap item; required
outcome; task-relevant contracts; acceptance checks; stop conditions; permissions
already granted. Omit history and unrelated reading. If another worker must act,
reduce the dependency to one handoff in the suite record's `SWARM.md` and delete
it when resolved.

Batch two or three related physical checks into one candidate; freeze what
passed, and later candidates touch only failures. Keep feasibility, a minimal
prototype and product implementation separate. Unfinished architecture work
lives on a named WIP branch; only physically accepted behavior reaches `main`.

## Where writing goes

This section is the one home of the writing rules. Each document owns one
subject, and `docs/README.md` routes every subject to its owner.

- State a rule once, in its owner, and cite it elsewhere. Replace stale text;
  never append a correction beside it.
- Every input Kadunce handles is defined in `docs/INPUT.md`, and other documents
  cite it. It opens with the controls map: each destination by its product
  name, with the touch and key that reach it, and every other action under that
  name below. A README quick start repeats rows of the map; `tests/verify-public.py`
  checks both.
- This repository is public. Its documents, code comments and commit messages
  describe the product: Kadunce does things, you act, shipped behavior is in the
  present tense, and planned behavior lives only in `docs/ROADMAP.md`. They carry
  no names, approvals or approval dates, no chat or handover narration, and
  nothing of Table beyond its name, its 1.0 release and how it opens and closes.
  `tests/verify-public.py` checks what a check can, in files and in commits not
  yet pushed; who decided what belongs in the private suite record.
- The plan, what each block learned, its status and cross-agent handoffs live in
  the suite record, never in this repository; its `README.md` states their rules.
- A durable decision goes in `docs/DECISIONS.md` as its rule in a sentence, why
  it holds, and what was rejected, citing the document that states the rule.
- Removed text lives in Git history. How something was found, measured and
  accepted goes in the commit message; live documents carry no progress
  narration, worker summaries, candidate hashes or test logs.
- Code comments explain code behavior and reasoning only, never handoffs,
  authorship, product instructions or agent conversation.
- Terminology follows `docs/TERMINOLOGY.md`, whose § Enforcement names the two
  layer-3 identities that keep the retired workspace term until Block 10b.
- A new tracked document is added to `docs/README.md` in the same change, or
  `tests/verify-docs.py` fails. `docs/README.md` § Keeping documentation small
  states the word budgets it enforces.

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

## Versioning

Repairs, polish and missing regression fixes are patch revisions. A genuinely
new product capability advances the minor version.

## Installation handover

Installing, restarting the graphical session and logging out are performed by
the session owner, never by an agent, and no task packet changes that. An agent
builds and verifies the candidate and hands the installation over as one
copy-pasteable command; the suite record's `HANDOVER.md` states what the
handover carries. After installation, `bash tests/verify-live-control.sh` runs
in the graphical session and confirms the controller is wanted by
`graphical-session.target`. Do not toggle the live effect or publish unless the
task says so in words.
