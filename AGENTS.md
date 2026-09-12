# Repository context and handoff

Start substantial work with `docs/CURRENT_STATE.md` and `docs/ARCHITECTURE.md`.
Use their links to read only the relevant decisions, contract, research and source.
Check the actual branch/diff before editing. Historical chats/checklists are
design evidence, not proof of current implementation or blanket authorization.

End substantial work by updating CURRENT_STATE: changed behavior, evidence,
installed versus source state, remaining uncertainty, and the next bounded task.
Record architectural changes in `docs/DECISIONS.md`. Keep CURRENT_STATE under
1,000 words; replace stale state instead of appending a transcript. Documentation
is a navigation cache, not a substitute for checking source or live state.

Heavy work owns architecture, input/rendering/state boundaries and recovery.
Lighter work takes a named task with explicit files, acceptance checks and stop
conditions; use `docs/REFACTOR-PLAN.md`. Work on disjoint files when tasks overlap.
Do not infer permission to install or publish from a handoff document.

# Mandatory safety control

Kadunce's persistent tray enable/disable switch is a release-blocking requirement,
including when the workspace effect is disabled or incompatible. Never remove it
or make it optional. Do not mark an install/update ready based on effect tests alone.

For Kadunce updates, run `bash tests/verify-control.sh` and, in the user's graphical
session after installation, `bash tests/verify-live-control.sh`. Check session
startup wiring too: the control must be wanted by `graphical-session.target`, not
only `default.target`, so logout/login under a surviving user manager restarts it.
Do not log out the user, stop the graphical session, or toggle the effect as a test
without permission. Ask for visual confirmation when needed; report any safety
control failure as blocking further feature testing.
