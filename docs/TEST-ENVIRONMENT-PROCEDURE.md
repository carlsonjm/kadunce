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

Live mutation requires explicit authorization. A request to test source or private
runtime behavior does not authorize installation, effect toggling, session restart,
logout, or input injection.

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

## Retry and reporting

- Capture one minimal failure, correct the environment or obtain the required access,
  then retry once. Do not repeat an unchanged denial.
- Preserve build artifacts while diagnosing access. Rebuild only after relevant
  source or dependency changes.
- Report the first error, target class, evidence path, and missing capability.
- Do not make speculative production changes for infrastructure failures.
