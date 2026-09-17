# Native carry contact boundary

September 11, 2026. `CarryContacts.h` is a headless observation ledger, currently
used only by its test. It neither intercepts events nor adopts native moves.

## Source audit

Installed KWin headers show:

- `input_event_spy.h`: spies observe events before filters. This is the appropriate
  observation point; WorkspaceInputRouter may not receive native-owned events.
- `input_event.h`: pointer buttons carry device/native-button identity; touch
  down/motion/up carry a touch ID but no device pointer. Do not fabricate physical
  touchscreen identity. A future adapter must explicitly represent seat scope.
- `window.h`: interactiveMoveResizeStarted has no initiating-device argument.
  A held contact alone does not prove it initiated the move. Keyboard commands
  and client-side asynchronous move requests need separate correlation.
- Spies have no touchCancel callback. Cancellation needs a verified companion
  path before production use; an effects-order filter alone may be insufficient.

## Implemented boundary

The ledger accepts adapter-issued lifetime device/seat tokens and native contact
IDs. It offers a candidate only with one trustworthy held contact. Tickets are
ledger-local and invalidated by membership changes, reset, release, cancellation
and device removal. Motion updates position without changing the ticket. Reused
IDs cannot revive old tickets. Duplicate downs/invalid motion taint a contact
until release/reset; they do not erase a held contact and make another device
appear unambiguous. Touch cancellation preserves held pointer state and vice versa.

This is candidate tracking, **not initiating-owner discovery**. The adapter must
establish move correlation separately and fail closed when events are missing or
ambiguous. Do not wire soleCandidate directly into NativeMoveTakeover::adopt.

## Private compositor experiment

`tests/unload-probe/ContactProbe.h` observes pointer device tokens and seat-scoped
touch contacts through a spy. Its separate order-minus-one filter observes touch
cancel and always returns false. This priority is experimental, not installed.
The client optionally calls Qt's startSystemMove from a real delivered press.
Matching requires a sole candidate, native move state, the request's implicit-grab
serial, and the matching client surface. Keyboard-style MoveOp has no such request.

Initial API finding: XdgToplevelWindow::shellSurface and its metaobject are not
exported by the installed libkwin, despite available headers. Direct use failed
plugin loading and was removed. The private test instead observes newly created
XdgToplevelInterface objects through the server's QObject-owned shell. That is
source-version-dependent discovery, not a production interface. It does not cover
windows predating the observer, XWayland, or server-side decoration moves.

Native move-start runs BEFORE our moveRequested listener. Production must not
release the source through its legacy start handler before correlating this
request. Simply connecting the probe to the current Effect would still be wrong.
Pointer focus is already cleared at request observation; capture the pressed
surface during the input spy's press callback, not from post-start focus.

Passing private evidence: `/tmp/kadunce-unload-test.iSBrNg/session.log`.
Both Qt client pointer/touch requests match their serial/contact and surface;
mixed pointer/touch requests reject. MoveOp while holding a pointer has no client
request and does not match. Touch cancel preserves held pointer state, suppresses
late-up resurrection, and permits a fresh reused touch ID. Device removal clears
the candidate. After observer destruction the client receives a new press/release.
This is not a physical keyboard shortcut test or a display-handoff test.
Existing snap/takeover regression also passes:
`/tmp/kadunce-unload-test.lyvK3E/session.log`.

Source inspected: KDE KWin v6.7.5
[touch input](https://github.com/KDE/kwin/blob/v6.7.5/src/touch_input.cpp),
[input filters](https://github.com/KDE/kwin/blob/v6.7.5/src/input.cpp),
[xdg move handling](https://github.com/KDE/kwin/blob/v6.7.5/src/xdgshellwindow.cpp),
and [shell ownership](https://github.com/KDE/kwin/blob/v6.7.5/src/xdgshellintegration.cpp).
Touch cancel clears KWin's active points before filters and suppresses later ups.
Earlier gesture/drag filters can consume cancellation, confirming that a normal
effects-order observer is insufficient. Lock-screen and competing-cancel-filter
integration are not exercised by this private test.

## Exported lookup and system decoration follow-up

`native/src/NativeMoveProtocol.h` replaces QObject shell discovery. It enumerates
only the owning Wayland client's resources through wl_client_for_each_resource,
filters xdg_toplevel, uses the exported XdgToplevelInterface::get and verifies the
exact surface. Returned handles are weak. This runs on the compositor thread when
attaching to a window, never per motion/frame. Null/non-xdg surfaces reject. The
probe links Wayland::Server explicitly; production will need that dependency when
it begins using the helper. It does not require unexported XdgToplevelWindow symbols.

The private system-decoration test uses actual Breeze titlebar mouse/touch events.
The probe records a sole contact during decoration event dispatch, clears that
candidate at the next event-loop turn, and checks the touch decorationPressId at
native start. It observes only: no native move is canceled or adopted. This proves
the motion-start path, not delayed-hold starts, XWayland, alternate decorations,
or complete keyboard/system-menu arbitration. These must reject safely until proven.
The private test pins org.kde.kdecoration2/library to org.kde.breeze because the
inherited Aurorae configuration produced a zero-height titlebar. No live theme edits.

Passing evidence: `/tmp/kadunce-unload-test.3kHhcV/session.log`. Covers existing
windows, observer recreation, two exact surfaces in one client, client/protocol
destruction and replacement client. Both system-titlebar mouse/touch motion and
the previous app serial/cancel/mixed-input cases pass. Twelve standard tests and
source/control/live checks pass. No installation, repair refresh or push.

## Next bounded integration

Connect verified press provenance to source reservation and a guarded coordinator,
with explicit rejection for unsupported starts. Suppress legacy start/finish only
for an accepted handoff; then connect painting and release drain together.
No production takeover until that complete path is verified.

Headless tests cover the ledger; the private experiment covers app-requested moves.
Installed plugin, repair and installer remain unchanged.

Build and all twelve CTest cases pass in `/tmp/kadunce-restoration-build`.
Source/control checks and read-only live safety check pass. Initial sandbox bus
denial was resolved with local socket permission; no safety-control mutation.
