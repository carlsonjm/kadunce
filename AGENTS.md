# Repository context and startup

For every task, read only this startup set, in order:

1. `AGENTS.md`
2. `docs/CURRENT_STATE.md`
3. `docs/NEXT-ROADMAP.md`
4. `SWARM.md`

Then inspect the branch, working tree, and only the source or reference documents
needed for the assigned work. `docs/README.md` classifies the remaining documents.
Do not read `docs/archive/` during normal startup. Use archived evidence only to
diagnose a regression, answer a provenance question, or revisit a failed candidate.

`CURRENT_STATE.md` describes current behavior, limitations, source/installed state,
and validation only. Replace stale text instead of appending progress notes.
`NEXT-ROADMAP.md` is the sole execution plan. Record durable architecture decisions
in `DECISIONS.md`; Git records implementation history.

Keep `SWARM.md` empty unless another live agent must act. A live handoff must be at
most 50 words; remove it when the dependency is resolved. Do not put backlogs,
status reports, implementation history, or completed handoffs there.

# Safety control

Kadunce's persistent tray enable/disable switch is release-critical, including
when the workspace effect is disabled or incompatible. Never remove it, make it
optional, or treat effect tests alone as release evidence.

For sandbox, D-Bus, private compositor, or startup failures, follow
`docs/TEST-ENVIRONMENT-PROCEDURE.md`. A transport denial is not evidence that the
kill switch is missing. Preserve private/live bus separation.

For Kadunce changes, run `bash tests/verify-control.sh`. After an authorized live
installation, run `bash tests/verify-live-control.sh` in the graphical session and
confirm the controller is wanted by `graphical-session.target`. Do not log out the
user, stop the graphical session, toggle the live effect, install, or publish unless
the task explicitly authorizes it. A safety-control failure blocks promotion.
