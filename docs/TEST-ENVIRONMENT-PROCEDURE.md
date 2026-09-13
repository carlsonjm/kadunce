# Testing through sandbox and D-Bus boundaries

J/A/B operating procedure, September 12. Read before diagnosing socket failures.
This is not permission to bypass a sandbox, use sudo, restart services or operate
the live desktop. Follow the current task's actual permission tools and policy.

## First classify the environment

| Test | Target | Expected access |
| --- | --- | --- |
| Build / pure model tests | Disposable build files | Workspace/temp filesystem; some Qt tests also need local sockets |
| verify-unload-isolated.sh / nested runtime | Private virtual KWin and private D-Bus | Local socket creation/binding; Xwayland for x11 routes |
| verify-live-control.sh / live trace | J's real graphical session | Existing user session bus, read-only queries only |
| Installation / physical input / restart | J's live desktop | Separate explicit authorization; not granted by test permission |

Do not “fix” a private test by pointing it at the real session bus. Do not run
the live safety checker inside dbus-run-session: that creates a different bus,
so it cannot establish whether J's tray control exists.

## Permission preflight

1. Record command, cwd, expected target, candidate build/hash, first failing
   output and exit code. Use explicit repo cwd; never assume the shell starts there.
2. Inspect current permission instructions. In the present managed desktop
   environment, request_permissions is the supported escalation path; shell
   require_escalated/rule approvals may be disabled. Do not keep retrying a
   disallowed approval mechanism or edit Codex configuration to override policy.
3. For socket denial, request the smallest supported capability and explain the
   exact test. This environment's tool exposes network.enabled, not a per-socket
   field. A turn-scoped grant has previously enabled our local D-Bus/private
   compositor tests; that is observed here, not a guarantee for every host.
   Inspect what was granted and retry the same minimal check once.
4. A filesystem read grant alone may not authorize connecting/binding sockets.
   A network grant may still leave managed socket restrictions. Respect denial;
   report an environment blocker rather than experimenting around restrictions.
5. Grants may expire between turns and do not transfer from A's task to B's task.
   A user saying “I ran it” is test evidence, not a permission grant to your shell.

Example request through the available tool (not a shell command):

```json
{
  "permissions": {"network": {"enabled": true}},
  "reason": "Run Kadunce's isolated virtual-compositor tests, which create local D-Bus/Wayland/Xwayland sockets; no installation or live desktop mutation."
}
```

For the live check, say instead that access is for read-only user-session D-Bus
verification. Do not describe a broadly exposed tool capability as technically
restricted to one socket; the task itself must remain scoped.

## Live D-Bus diagnostics (read-only)

Run in the inherited graphical-user environment, not sudo and not a new bus:

```bash
id -u
printenv XDG_RUNTIME_DIR DBUS_SESSION_BUS_ADDRESS WAYLAND_DISPLAY
qdbus6 org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.GetId
qdbus6 org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner org.kde.KWin
qdbus6 org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner org.kde.StatusNotifierWatcher
```

Keep errors visible. GetId failure means bus access/environment is unproven.
NameHasOwner false on a reachable bus means that name is not owned on that bus;
check target environment before claiming the real service is absent. Do not dump
the entire environment, guess UID 1000, copy credentials or forge bus addresses.

Only after bus access is established, run:

```bash
bash tests/verify-live-control.sh
```

Important: the current checker suppresses D-Bus stderr and can print “kill switch
missing” when access was denied. An earlier A-Team attempt failed this way and
passed unchanged after permission approval. Until transport is established,
classify as **live safety unverified**, not a proven missing control and not a pass.
If it still fails on the correct reachable bus, treat it as a real release blocker
and investigate read-only. Do not restart the tray or toggle Kadunce to test a theory.

## Private compositor tests

Use existing verify-unload-isolated.sh / verify-integrated-carry.sh. The private
harness creates a unique /tmp/kadunce-unload-test.* tree, mode-700 runtime dir,
isolated config/data/state, dbus-run-session and kwin_wayland --virtual. Its plugin
path must reference the intended candidate. Preserve all isolation variables.
Tablet routes need the disposable tablet-predicate build, not production output
discovery. x11 routes need the existing private Xwayland launcher.

For a targeted test, B supplies real verified values, never literal placeholders:

```text
KADUNCE_RUNTIME_BUILD=<absolute verified build directory>
KADUNCE_PROBE_SESSION=<existing whitelisted session script>
bash tests/verify-unload-isolated.sh
```

Pass variables as command-scoped environment assignments or tool env. Do not
globally repurpose XDG_RUNTIME_DIR or DBUS_SESSION_BUS_ADDRESS for later live calls.
Read the printed evidence directory's build.log and session.log, starting at the
first failure. The harness currently bounds compositor runtime at 40 seconds;
exit 124 means timeout, not a diagnosed Kadunce bug.

| Evidence | Next action |
| --- | --- |
| EPERM/EACCES, cannot bind/connect bus or display | Permission/transport preflight; no production patch |
| Build/link/missing dependency failure | Fix build prerequisites within scope; don't diagnose input behavior |
| Empty session.log / child never starts | Check build completion, selected script exists, executable bit/shebang, launcher and permissions |
| Unknown probe session | Verify intended script against whitelist; don't bypass it with arbitrary commands |
| KWin/plugin fails to load | Verify ABI/version/plugin path/candidate hash; don't substitute installed binary silently |
| Timeout with progress | Inspect last completed assertion and process stage; don't automatically lengthen timeout |
| Repro assertion fails after successful setup | Candidate behavior/test-contract investigation; retain failed-before evidence |
| Portal/FUSE/PipeWire warnings | Do not assume fatal or harmless: determine whether required capability/assertion failed |

Never kill all kwin_wayland, dbus-daemon, Xwayland or Plasma processes. Any cleanup
must identify the owned private test process/tree; if uncertain, stop. Never
chmod/chown the real /run/user directory or expose D-Bus over TCP to make tests run.

## When J manually runs the test

Do not ask J to repeat a completed test just because B could not launch it.
First obtain/read the printed evidence directory, exact command/build, exit code
and final output. The current private harness already writes logs. Check that
they correspond to the intended source and candidate, not an older passing run.
If J only reports “done,” record **executed; result pending**, then ask for the
evidence path or final result, not a rerun. User-reported pass may be recorded as
such; don't relabel it agent-verified or physical acceptance of unrelated routes.

If a new manual run is necessary, provide ONE exact reviewed command, say whether
it targets private or live state, and what output/path to return. No sudo for
ordinary D-Bus/model/private compositor tests. Do not conceal restart, installation
or live input in a “test” command. Reject policy-bypass workarounds; if platform
policy disallows the operation altogether, stop rather than outsourcing evasion.

## Retry / reporting discipline

- One minimal failure capture → appropriate permission request → one retry after
  the grant or environment correction. Stop repeating unchanged denied commands.
- Preserve build artifacts while diagnosing access; don't rebuild everything to
  retry a socket connection. Rebuild only after relevant source/build changes.
- Report separately: build pass, private runtime pass/fail, live safety verified/
  unverified, and J's physical pass/fail. Never turn an infrastructure skip green.
- Include the first error, permission response, evidence path and specific needed
  access in an escalation. No speculative production patch for infrastructure.

## Prompt J can send B

```text
Read docs/TEST-ENVIRONMENT-PROCEDURE.md before retrying tests. Separate sandbox
transport failures from candidate failures. Use the currently available permission
request mechanism for local socket access; don't assume A-Team's grant applies
to this task. Keep private KWin tests on their isolated bus and live safety checks
on my existing user bus. Do not change production code, weaken assertions, use
sudo, restart services or disable sandboxing to fix an access error.
I already ran the requested test manually. Consume its evidence first; ask only
for the result/log path if missing. Resume the assigned engineering packet once
the evidence is classified. Report infrastructure blockers separately from bugs.
```

General approval/socket policy is informed by [official OpenAI documentation](https://learn.chatgpt.com/docs/agent-approvals-security).
Exact request_permissions shape comes from this session's available tool, and
the D-Bus caveat and harness details come from local source/observed tests.
Different managed environments may enforce additional socket restrictions.
