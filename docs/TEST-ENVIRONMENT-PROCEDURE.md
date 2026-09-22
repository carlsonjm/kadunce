# Test environment procedure

Use this procedure before diagnosing any sandbox, D-Bus, display-socket, private
compositor, or startup failure. Transport failure is not evidence of a Kadunce
behavior defect or a missing live safety control.

## Classify the target

| Target | Permitted effect |
| --- | --- |
| Source/build/unit test | Reads source and writes build artifacts only |
| Private compositor | Uses isolated runtime, display, D-Bus, config, data, and state |
| Live read-only safety | Reads registration, installed provenance, and startup wiring |
| Live mutation | Installs, toggles, injects input, restarts, logs out, or changes the real session |

Live mutation requires explicit authorization, and the three that cost the user
their session — installation, session restart and logout — are never an agent's to
perform however the task is worded; see § Handing over an installation. A request
to test source or private runtime behavior does not authorize effect toggling or
input injection either.

## Preflight

Before changing code:

1. Record the exact command, source revision/diff, build directory, and intended
   target class.
2. Confirm required executables, libraries, scripts, and plugin paths exist.
3. For a private compositor, confirm every runtime and socket path is private and
   owned by the test. Never reuse the real session bus or display.
4. For live read-only checks, use the existing graphical-session environment rather
   than inventing D-Bus or display addresses.
5. Capture the first failing line and exit code. Separate setup from assertions.

Do not weaken sandboxing, change ownership or modes on `/run/user`, expose D-Bus over
TCP, or use administrator privileges to make an ordinary test pass.

## Live D-Bus diagnostics

For a read-only live check, distinguish:

- no live bus address available to the process;
- permission denied opening the live bus or socket;
- bus available but Kadunce service absent;
- service present but method/property failure;
- service healthy while the installed effect/plugin differs from source.

Only the last three contain Kadunce runtime evidence. `EPERM`, `EACCES`, connection
denial, or an unavailable display is an environment result until proven otherwise.
Run `tests/verify-live-control.sh` only in the actual graphical-session environment.

## Private compositor rules

Use the repository harnesses and their whitelisted sessions. Keep private:

- `XDG_RUNTIME_DIR`, configuration, cache, data, and state;
- Wayland/X11 display sockets;
- D-Bus session;
- KWin executable and plugin under test;
- logs and evidence directories.

Never kill broad process names such as all `kwin_wayland`, `dbus-daemon`, Xwayland,
or Plasma processes. Cleanup must target the exact owned test process tree. If
ownership is uncertain, stop.

`tests/verify-unload-isolated.sh` accepts verified command-scoped overrides for a
private KWin executable, build directory, and whitelisted probe session. Do not
export those values globally or point them at the live session. Exit 124 means the
harness timed out; it does not diagnose the cause.

## Reading a probe run

The runner compiles the production controllers out of the working tree for every
run, so a tree edited while a probe is running, or a runtime build written to
while one is running, yields a result that describes neither the old code nor the
new. Both failure modes produce an empty or truncated log, which reads like a
quiet pass rather than a failure. Let a run finish before editing or building.

Read verdicts from `session.log` in the evidence directory the runner names on
its first line, never from the runner's own output: it aborts before printing its
summary as soon as a probe fails, so a failing run prints no verdict at all.

A probe session the whitelist does not name is refused by name before anything
starts. That is the intended way to park a probe that cannot run yet; it can
never half-run and report a pass.

Every virtual output the harness creates is 1280x800, which is below the compact
threshold, so each caps at two Bento panes and the larger pane of a two-pane
landscape shape is roughly 780 pixels wide. A probe asserting three panes, or a
share wider than that, is asserting a grammar no display in the harness has.
Check that arithmetic before reading such a failure as a regression.

## Evidence classification

| First failure | Classification and next action |
| --- | --- |
| Cannot open/bind bus or display | Resolve permission/transport; do not patch production |
| Build, link, or missing dependency | Fix prerequisites within scope |
| Child never starts or log is empty | Verify build, script, launcher, and permissions |
| Unknown probe session | Use the documented whitelist; do not bypass it |
| KWin/plugin fails to load | Verify ABI, version, path, and candidate hash |
| Timeout after progress | Inspect the last completed assertion/process stage |
| Assertion fails after healthy setup | Investigate candidate behavior or test contract |
| Portal/FUSE/PipeWire warning | Decide whether the required capability actually failed |

Keep build pass, private runtime pass/fail, live safety state, installed provenance,
and physical acceptance as separate results. An infrastructure skip is not a pass.

## Manual evidence

When a user has already run a requested check, consume its exact command, candidate,
exit code, and evidence path before requesting another run. If only completion is
known, record “executed; result pending” and ask for the result or log path. A
user-reported pass remains user-reported; it does not establish unrelated routes.

If another manual run is necessary, provide one exact reviewed command, identify
whether it targets private or live state, and state what output should be returned.
Do not hide installation, restart, effect toggling, or live input inside a test.

## Handing over an installation

Installing a candidate, restarting the graphical session and logging the user out
are never an agent's to perform, and no task packet makes them so. An agent builds
and verifies the candidate, then hands the installation over as one exact command.
This is not a permissions workaround: the user is the only party who can judge when
losing their session is acceptable, and a physical result is worthless if the
gestures ran against a build the compositor was not using.

The handover is written so the user can act on it without coming back to ask. It
carries all six:

1. The exact command, as one copy-pasteable line carrying its absolute path, with
   the branch and commit it installs. Naming the directory separately, or linking
   to it, is not the path: the user runs these from whatever terminal is open, and
   working out the `cd` is time added to a pass they did not agree to spend. Every
   other command in the same handover carries its absolute path too.
2. That the installer asks once for a password, and that it restores the previous
   effect configuration by itself if it stops early.
3. Which line to read at the end. `install.sh` asks the running compositor which
   plugin image it is actually using and reports it; "still running the PREVIOUS
   build" is the expected answer after an in-session install, not a fault.
4. That the restart logs the user out, and that no gesture before it tests the
   candidate that was just installed.
5. The safety check to run first after logging back in, and that a failure there
   ends the pass before any gesture is attempted.
6. The checks themselves, written as gestures rather than contract language, and
   the ones already expected to fail, so a known gap is not reported as a defect.

Give the user a place to record the results that survives the logout; a terminal
session does not. For a pass carrying more than two or three checks, that place is
a published test sheet: the numbered setup, the gestures, the failures already
expected, and per check a verdict, a free note, or both, saved on the sheet itself
so a later session reads them back rather than asking. The note is not only for a
failure: the answer to a gesture is often neither pass nor fail, and a sheet that
offers only the two loses exactly the observation worth having. It costs little beyond deciding the checks,
which the pass needs anyway, and it is what stops a result depending on the user
remembering a dozen outcomes across a restart. A single check needs no sheet.

The tray control appears twice in the list above, and only one of the two belongs
on every pass. `verify-live-control.sh` is the check that is never skipped: it
proves the control registered and exposes its switch, it costs one command, and a
failure there ends the pass before any gesture. The by-hand toggle --- switching
Kadunce off and watching every window, a sleeping one included, arrive on the
plain desktop --- is a different check, and putting it on a sheet whose candidate
touched neither release, nor admission, nor minimizing spends the user's
attention on an answer already known. J asked for it to be dialed back on 21
September, after it had passed by hand on two candidates the same day. Put it on
the sheet when the change reaches §7, §13 or the control itself, and leave it off
otherwise. That narrows how often the user is asked and nothing else: the control
stays release-critical, and a safety-control failure still blocks promotion and
is never waived.

## Retry and reporting

- Capture one minimal failure, correct the environment or obtain the required access,
  then retry once. Do not repeat an unchanged denial.
- Preserve build artifacts while diagnosing access. Rebuild only after relevant
  source or dependency changes.
- Report the first error, target class, evidence path, and missing capability.
- Do not make speculative production changes for infrastructure failures.
