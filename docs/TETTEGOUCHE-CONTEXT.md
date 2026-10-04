# Kadunce → Tettegouche context contract

Kadunce owns KWin discovery, card identity, stack relationships, workspace
presentation, output sessions, and device posture. Tettegouche owns invocation,
querying, ranking, and the choice to focus an existing application or launch a
new one.

The workspace snapshot is deliberately read-only. Tettegouche requests it when
its launcher opens, on workspaceContextChanged signals, and before selection;
it does not poll raw KWin state or mutate Kadunce's models.
An independent, explicitly versioned guest-session protocol lets a compatible
launcher temporarily occupy Spread's center without becoming a real card.

## Endpoint

The installed KWin effect exports this session D-Bus method:

```text
service:   org.kde.KWin
path:      /Kadunce
interface: studio.warbler.Kadunce
method:    workspaceContext
result:    compact UTF-8 JSON string
```

After resolving an existing result from that snapshot, Tettegouche may ask
Kadunce to activate its exact window:

```text
service:   org.kde.KWin
path:      /Kadunce
interface: studio.warbler.Kadunce
method:    activateApplicationWindow(windowId)
result:    true only when that live application window was activated
```

The command accepts only a current application `windowId` from the snapshot.
Kadunce restores minimized windows and routes card-backed windows through its
existing activation handling, so selection, stack membership, and presentation
remain under Kadunce's authority.

The payload identifies itself with schema
`studio.warbler.kadunce.workspace-context` and integer `version: 1`. Consumers
must reject unknown major versions instead of guessing at fields.

## Version 1 payload

- `focus`: focused window identity, application identity, title, and whether
  Kadunce currently owns it as a card; otherwise `null`.
- `applications`: current-desktop application windows, including minimized
  windows. Each entry exposes KWin's window-lifetime UUID, desktop application
  identity, title, output, focus/minimize state, and current card membership.
- `cardStage`: whether Card Stage is active, its presentation, the selected
  card UUID, and ordered selected-stack member UUIDs. The presentation is one
  of `inactive`, `cardLine` (Spread), `bento` (the display shows its Bento
  layout while Card Stage holds its cards hidden), `desktop` (the card display
  shows the Plasma desktop while Card Stage holds its cards hidden), or
  `active`. `cardLine` is a
  frozen interface value that Block 10b renames with a versioned migration.
- `desktopStage`: whether any output currently owns a Bento session.
- `lastActivated`, optional on each application: an in-memory, monotonic
  sequence for this effect's lifetime. Snapshots without it keep
  focused/selected/frontmost ordering.
- `displayContext`: hardware posture when supplied by the optional Z13 helper,
  the selected edge backend, and every output's role, geometry, and Bento
  ownership.

`windowId` and `cardId` use KWin's internal UUID and are stable for the life of
that window. `cardIndex`, stack position, and titles are presentation metadata;
Tettegouche must not persist them as identity.

## Query example

```bash
qdbus6 org.kde.KWin /Kadunce studio.warbler.Kadunce.workspaceContext
```

Version 1 keeps no persisted activity history; `lastActivated` lasts only as
long as the effect. Tettegouche can identify an open result from the snapshot
and activate that exact window through the separate command above; unmatched
results still use Plasma's normal application launch action.

## Launcher guest protocol 3

Tettegouche must first call `launcherGuestProtocolVersion`. It may request guest
mode only when the result is exactly `3`; a missing or different result means it
must retain its standalone surface. This prevents an older Kadunce build from
receiving a guest it cannot present.

```text
launcherGuestProtocolVersion() -> integer
beginLauncherGuest(uniqueOwner) -> compact JSON reply
updateLauncherGuest(horizontalDelta)
finishLauncherGuest(horizontalDelta) -> committed boolean
prepareLauncherGuestLaunch(applicationIds, requestToken) -> accepted boolean
cancelLauncherGuestLaunch()
endLauncherGuest()
```

An accepted begin reply contains `protocol: 3`, `accepted: true`, the target
`output`, and `card` and `active` geometries. Kadunce reserves a centered guest
footprint that is six percent of the work area narrower than a normal card, and
moves both real neighbors inward by the matching three-percent inset. It does
not insert the launcher into `SpreadModel`; real cards remain its sole mutable
model state. Tettegouche owns and renders the interactive center card, while
Kadunce mirrors its horizontal drag onto the adjacent real cards.

Kadunce watches the launcher's unique session-bus owner and restores Spread
if that process disappears. A committed handoff selects the incoming real card.
A press outside the launcher and the keys is Kadunce's (`INPUT.md` § Spread):
it never reaches the application under it, so it cannot promote that
application to Active. Any rejected begin request leaves both applications in
their existing standalone behavior.

Before launching an application that does not yet have a live window,
Tettegouche calls `prepareLauncherGuestLaunch`. Kadunce then holds the guest
while the application creates a window. A shown, ready-for-painting window matching an exact
normalized desktop identity or declared StartupWMClass completes the
handoff: Kadunce asks Tettegouche to animate out, then promotes the arriving
window to Active. Tettegouche cancels the pending state after its bounded
timeout if no application window appears.

New-card admission preserves the guest and previous card selection instead of
promoting a window immediately. Window addition, painting readiness, identity
changes, and activation all feed the same one-shot completion gate. Web content
or full application loading is not a readiness requirement.

Kadunce completes that transition by calling `completeGuestLaunch(requestToken)` on the
unique `/Launcher` owner supplied at begin time. The well-known Tettegouche
service is not used for lease ownership or completion.

Tokens identify individual requests. Generation guards prevent delayed settles
from ending newer leases. Both sides bound launch waiting to ten seconds. Unknown
application identities time out rather than accept unrelated activations.

`workspaceContextChanged` signals added, closed, and activated windows. Tette
ignores out-of-order snapshot replies. `bridgeUnavailable` is emitted before
effect unload; service-owner loss is watched separately. Either clears guest
input masking and pending launch/exit motion, returning Tette to standalone search.
Neither side restarts the compositor or automatically renegotiates a lost lease.

`completeGuestNavigation(slot)` on the same owner, slot `-1` for the visible
left neighbor and `1` for the right, remains in protocol 3, but Kadunce does not
currently call it: a tap on a neighbor closes the launcher. The guest's motion
is `ITASCA-VISUAL-LANGUAGE.md` § Kadunce geometry.


## Companion guests

Any application may hold Spread's center the same way, answered on an object
of its own rather than Tettegouche's. It checks
`companionGuestProtocolVersion` first and asks only when the result is exactly
`1`.

```text
companionGuestProtocolVersion() -> integer
beginCompanionGuest(uniqueOwner, objectPath, interfaceName) -> compact JSON reply
```

The reply is the launcher guest's, with `protocol: 1`. Kadunce answers on
`objectPath` and `interfaceName` with the launcher guest's `dismissGuest` and
`completeGuestLaunch(requestToken)`; every other call, launch preparation
included, is the launcher guest's. Two rules differ:

- A companion is accepted only while Spread is presented. Over an Active card
  it is refused and Spread is not opened for it: the companion draws its own
  card there, and an application it launches takes the Active card's place,
  with Spread never shown (`CARD-LIFECYCLE.md` §8, While an individual card is
  Active).
- Spread has one center. A guest asking while another holds it takes it, and
  Kadunce sends the one it replaces `dismissGuest` first; the launcher guest
  follows the same rule.

A guest's surface gives up the keyboard as it closes, and KWin hands focus back
to the card that had it before. In Spread that restoration, within a second of
any guest closing, is not a request to open the card: Spread stays, so a
companion opened as Search closes can still take the center.

A companion is a layer-shell surface, as the launcher is. An ordinary window
is a card, so it cannot hold the center.
