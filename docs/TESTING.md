# Testing

How Kadunce is checked, from the fastest pre-check to J's hands on the tablet, and
what each result is allowed to prove. `AGENTS.md` owns the safety-control rule and
who installs; this document owns the procedure.

## What each check proves

| Check | Target class | Proves | Does not prove |
| --- | --- | --- | --- |
| `bash tests/verify-headless.sh` | Source and build only | Domain rules that need no KWin: a C++20 compiler and Qt6Core | Anything KWin-linked; never promotion evidence |
| `./verify.sh` | Source and build only | Documentation hygiene, source guards, package and control checks; run on every change | Runtime behavior |
| `bash tests/verify-integrated-carry.sh` | Private compositors | Every scene of the private route matrix plus native tests | The installed system, appearance, hardware |
| `bash tests/verify-live-control.sh` | Live, read-only | The tray control is registered, exposes its switch, and is wanted at session start | Any gesture |
| Physical review | Live, by hand | What only the real tablet shows: feel, touch, bezel, folio, displays, keys | Anything a saved test already proves |

A fifth class, live mutation, installs, toggles the effect, injects input,
restarts or logs out of the real session. Installation, session restart and
logout are performed by J and handed over as one command (§ Handing over an
installation); no task wording makes them an agent's. Toggling the live effect or
injecting input into the real session needs the task to say so in words; a
request to test source or private runtime behavior does not.

Keep build pass, private runtime pass or fail, live safety state, installed
provenance and physical acceptance as separate results. Source guards are weaker
than runtime tests, private compositor tests are not physical acceptance, and a
skipped live or infrastructure check stays unverified; it is never a pass.

`./verify.sh` piped into `grep` or `tail` reports the pipe's status, not its own.
Redirect it to a log and check its exit status by itself
(`./verify.sh >LOG 2>&1; echo "EXIT=$?"`), and never chain a commit after a piped
run.

## Private route matrix

Run from the repository root:

```bash
bash tests/verify-integrated-carry.sh
```

While iterating on one change, name only the scenes it touches:

```bash
KADUNCE_GATE_SCENES="output-unplug-runtime desktop-bezel-runtime" bash tests/verify-integrated-carry.sh
```

A candidate is handed over only after a run with every scene. The full run takes
about three minutes; `KADUNCE_GATE_JOBS` sets how many private compositors run at
once (default 4). Every scene runs even after one fails, and each failure names
its log. If the full run creeps past about five minutes, measure where the time
goes before adding more scenes.

The harness snapshots the exact source and tests, records the base revision and
hashes, builds the production plugin, a disposable virtual-tablet variant and the
probe once, and runs:

- all native CTests;
- pointer and touch cross-output carry and destination footprints;
- same-output Bento exchange, return-home, dock exclusion, and restoration;
- ordinary monitor entry, edge withdrawal, cancellation, unload, and restoration;
- tablet Active departure and Bento return into Spread;
- a monitor unplugged and plugged back in, leaving every card and its window on
  the tablet;
- Spread transfer, stack insertion and browsing, cancellation, and interrupted
  motion;
- candidate Bento transfer, restoration and unload, and independent safety checks.

Each scene uses a private display, runtime directory and D-Bus session. The live
control check at the end is read-only. The harness never installs, injects input
into the real session, toggles the effect, restarts Plasma or runs repair. Keep
its printed evidence directory until the candidate is accepted or rejected.

A new scene goes into the list in `tests/verify-integrated-carry.sh` in the same
change that adds it; `tests/verify-source.sh` fails for a scene the gate does not
run. Physical appearance, frame pacing, fullscreen release, fractional-scale
transfer, suspend and resume, and hardware input cannot be accepted here.

## Promotion checks

A promotable candidate requires all of:

1. Exact source identity and a clean expected diff.
2. Production build and focused native tests pass.
3. `./verify.sh` passes.
4. The integrated route matrix passes, every scene, without weakened assertions.
5. After J installs it, installed provenance and `tests/verify-live-control.sh`
   pass.
6. Physical review of § Bounded physical checklist passes for the behavior the
   candidate changed.

Any safety-control, source-identity, restoration, teardown or private-route
failure blocks promotion. Infrastructure failure stays infrastructure evidence
until the private environment is proven healthy; never patch production behavior
to compensate for an unclassified sandbox, display, D-Bus or dependency failure.
Open boundaries the private harness cannot close are listed in
`docs/CURRENT_STATE.md`.

## Structural change

A regression blocks a refactor, and a new product idea enters `ROADMAP-CC.md`
only after J approves its scope. Structural extraction keeps one mutable state
owner, typed controller boundaries, value-first admission, and rendering as a
reader of state, never an owner of it. A refactor may rename and restructure
freely while it preserves the authority boundaries in `docs/ARCHITECTURE.md` and
these accepted behaviors:

| Accepted behavior | Automated evidence | Still needs the tablet |
| --- | --- | --- |
| Active gutter is output-relative, bounded 6–48 px, default 10 px | layout, window handling | real minimum-size applications |
| Release restores desktop state, maximize and fullscreen included | window handling, restoration | repaint, focus, original geometry |
| Two groups use 64% center and 54% shoulder; three or more use 54% | layout, model | animated continuity, clipping |
| Stacks keep membership order and selected face | model, insertion, browse, cancellation | identity across lifecycle |
| Passive tablet neighbors never paint on another output | paint route, source guards | fractional-scale clipping |
| Native move and panel input keep their ownership | panel and input routes | KWin and touchscreen ordering |
| Guest outside-tap dismisses; movement is not a tap | guest input | real closure, swipe collapse |
| A new application replaces the guest center before Active | arrival, launch identity | splash, main window, focus |
| The disable control survives independently | control, live control, repair | the hand toggle |

## Reading a probe run

`tests/verify-unload-isolated.sh` runs one scene. Run directly, it compiles the
probe, with the production controllers, out of the working tree, and loads the
runtime build it is pointed at. Editing the tree or writing that build while it
runs yields a result that describes neither the old code nor the new, and shows
as an empty or truncated log that reads like a quiet pass. Let a run finish
before editing or building. The integrated gate snapshots its source first, but
its closing source and control checks read the working tree.

`tests/verify-unload-isolated.sh` stops as soon as its scene fails, before
printing the `PASS:` line, so a failing direct run prints no verdict. Read the
verdict from `session.log` in the evidence directory named on its first line. The
integrated gate is different: it runs every scene even after one fails and
prints each failing scene's log.

A scene the whitelist in `tests/verify-unload-isolated.sh` does not name is
refused by name before anything starts. That is how to park a probe that cannot
run yet; it never half-runs and reports a pass.

Every virtual output is 1280x800, below the compact threshold, so each caps at
two Bento panes and the larger pane of a two-pane landscape shape is roughly 780
pixels wide. A probe asserting three panes, or a share wider than that, asserts a
grammar no display in the harness has.

A failing assertion is often the probe, not the code. Before changing production
code, find which term of the assertion is false (temporary diagnostic lines, one
scene re-run) and run the same scene against `main` to attribute it.

- Qt logs to journald, so a probe reading a Qt program's stderr sees nothing
  unless that program runs with `QT_FORCE_STDERR_LOGGING=1`. The effect's own log
  lines never reach `session.log`; read state through
  `qdbus6 org.kde.KWin /Kadunce workspaceContext` (or `nativeCarryState`).
- Under parallel load, wait for a state with a bounded poll, never a fixed sleep.
- Iterate with `KADUNCE_GATE_SCENES`, then run every scene once before a handover.

## Failure classification

Transport failure is not evidence of a Kadunce defect or of a missing safety
control. A sandbox, D-Bus or display-socket denial is a test-environment result,
never evidence about the tray control.

### Preflight

Before changing code:

1. Record the exact command, source revision or diff, build directory and target
   class.
2. Confirm the required executables, libraries, scripts and plugin paths exist.
3. For a private compositor, confirm every runtime and socket path is private and
   owned by the test. Never reuse the real session bus or display.
4. For a live read-only check, use the existing graphical-session environment;
   never invent D-Bus or display addresses.
5. Capture the first failing line and exit code. Separate setup from assertions.

Never weaken sandboxing, change ownership or modes on `/run/user`, expose D-Bus
over TCP, or use administrator privileges to make an ordinary test pass.

### Live D-Bus diagnostics

For a read-only live check, distinguish:

- no live bus address available to the process;
- permission denied opening the live bus or socket;
- bus available but the Kadunce service absent;
- service present but a method or property failing;
- service healthy while the installed plugin differs from source.

Only the last three are Kadunce evidence. `EPERM`, `EACCES`, connection denial or
an unavailable display is an environment result until proven otherwise. Run
`tests/verify-live-control.sh` only in the real graphical session.

### Private compositor rules

Use the repository harnesses and their whitelisted scenes. Keep private the
`XDG_RUNTIME_DIR`, configuration, cache, data and state; the display sockets; the
D-Bus session; the KWin executable and plugin under test; and logs and evidence.

- Kill only the exact test process tree the harness owns, never broad names such
  as every `kwin_wayland`, `dbus-daemon`, Xwayland or Plasma process. If
  ownership is uncertain, stop.
- Never run a shell probe against the live session: never start `plasmashell`,
  script panels, or kill by `$PPID` outside a private compositor. A nested
  Plasma-shell probe once froze the machine. A measurement that cannot be taken
  privately is handed over like an installation.
- A Wayland socket path must fit in 108 bytes, so a nested compositor under a long
  `TMPDIR` fails to start. Keep `TMPDIR` at `/tmp`.
- `tests/verify-unload-isolated.sh` accepts command-scoped overrides for a
  private KWin executable, build directory and whitelisted scene. Never export
  them globally or point them at the live session. Exit 124 means the harness
  timed out; it does not diagnose the cause.

### Evidence classification

| First failure | Classification and next action |
| --- | --- |
| Cannot open or bind bus or display | Resolve permission or transport; do not patch production |
| Build, link, or missing dependency | Fix prerequisites within scope |
| Child never starts or log is empty | Verify build, script, launcher and permissions |
| Unknown probe session | Use the documented whitelist; do not bypass it |
| KWin or plugin fails to load | Verify ABI, version, path and candidate hash |
| Timeout after progress | Inspect the last completed assertion or process stage |
| Assertion fails after healthy setup | Investigate candidate behavior or the probe (§ Reading a probe run) |
| Portal, FUSE or PipeWire warning | Decide whether the required capability actually failed |

### Retry and reporting

- Capture one minimal failure, correct the environment or obtain the access, then
  retry once. Do not repeat an unchanged denial.
- Preserve build artifacts while diagnosing access; rebuild only after a relevant
  source or dependency change.
- Report the first error, target class, evidence path and missing capability.
- Make no speculative production change for an infrastructure failure.

## Handing over an installation

J performs installation, session restart and logout. An agent builds and verifies
the candidate, then hands the installation over as one command. J is the only
party who can judge when losing the session is acceptable, and a physical result
is worthless if the gestures ran against a build the compositor was not using.

The handover is written so J can act on it without coming back to ask:

1. One copy-pasteable install line carrying absolute paths
   (`cd /absolute/path/to/kadunce && ./install.sh`), never a bare `./script` or a
   repository link, and holding only the installers. Skip branch switches when the
   branch is already checked out, after checking, and say which branch and commit
   it installs. Every other command in the handover carries its absolute path.
2. That the installer asks once for a password and restores the previous effect
   configuration if it stops early.
3. That the logout replaces the running build, and no gesture before it tests the
   candidate.
4. The safety check to run first after logging back in,
   `bash /absolute/path/to/kadunce/tests/verify-live-control.sh`; a failure ends
   the pass before any gesture.
5. The checks, written as gestures in plain English rather than contract jargon.

`install.sh` asks the running compositor which plugin image it uses; "still
running the PREVIOUS build" is the expected answer after an in-session install.
That line needs no place on the sheet.

### The test sheet

A hand sheet holds only what the test compositor cannot reach: install, the
safety check, how a gesture feels, and real hardware (the touchscreen, the bezel,
the folio, plugging in a display, the keys). Leave out any step a saved test
already proves, and say in chat which tests cover it. One check per change, plus
any unverified carry-over; a check already expected to fail says so on the check.
No record-only card for what the installer said and no "not covered" list; gaps
go in chat.

For more than two or three checks, publish the sheet so results survive the
logout: numbered setup, the gestures, and per check a verdict, a free note, or
both, saved on the sheet. The note matters because a gesture's answer is often
neither pass nor fail. A single check needs no sheet.

The automated control check runs on every pass. The hand toggle, switching
Kadunce off and watching every window, a sleeping one included, arrive on the
plain desktop, goes on the sheet only when the change reaches §7 (minimize), §13
(release and disable) or the control itself. That narrows how often J is asked
and nothing else: the control stays release-critical.

### Reading the results back

Verdicts come back in the published sheet's database: collection `checks`, one
document per check holding `verdict` and `note`. Corroborate before acting on
them:

- `qdbus6 org.kde.KWin /Kadunce loadedPluginProvenance` returns the inode the
  running compositor has mapped; compare it with `stat -c %i` on the installed
  plugin. Installed is not running: KWin keeps the old plugin image mapped across
  an unload and reload, and only a compositor restart replaces it.
- The user journal carries the effect's own account:
  `journalctl --user -b --since "YYYY-MM-DD HH:MM" | grep Kadunce`, with an
  absolute `--since`; a relative one errors and, with stderr dropped, reads as an
  empty result.
- Keys that do not appear while the folio keyboard is attached are KWin laptop
  mode, not a regression. Check
  `qdbus6 org.kde.KWin /org/kde/KWin org.kde.KWin.TabletModeManager.tabletMode`
  first; a sheet that exercises the keys says to detach the folio.

When J has already run a requested check, consume its exact command, candidate,
exit code and evidence path before asking for another run; if only completion is
known, record "executed; result pending" and ask for the result or log path. A
user-reported pass stays user-reported and does not establish unrelated routes.
Another manual run is one exact reviewed command that says whether it targets
private or live state and what output to return. Never hide an installation,
restart, effect toggle or live input inside a test command.

## Bounded physical checklist

Choose the smallest subset the candidate touches and that no saved test proves.
Each result is PASS, FAIL or NOT RUN, with the installed identity from § Reading
the results back.

1. Active and Spread entry and release with ordinary, maximized and fullscreen
   windows: focus, interaction and restored geometry.
2. Spread paging, arrival and removal, stack browsing, lift and cancel,
   extraction, and a stable selected Card.
3. Guest outside tap, movement cancellation, swipe collapse, relaunch and
   application replacement.
4. Panel controls and application input stay native; deliberate system-edge entry
   still works.
5. Cross-output carry, Escape return, Bento admission, dock clearance, output
   scale and topology, and passive-neighbor isolation.
6. The safety control: the automated check always; the hand toggle, with complete
   desktop recovery, only as § The test sheet allows.

Do not run live repair to satisfy this checklist; `tests/verify-repair.sh` covers
it in isolation unless repair behavior itself changed.
