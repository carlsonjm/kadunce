# Kadunce → Tettegouche context contract

Kadunce owns KWin discovery, card identity, stack relationships, workspace
presentation, output sessions, and device posture. Tettegouche owns invocation,
querying, ranking, and the choice to focus an existing application or launch a
new one.

The workspace snapshot is deliberately read-only. Tettegouche requests it when
its launcher opens; it does not poll raw KWin state or mutate Kadunce's models.
An independent, explicitly versioned guest-session protocol lets a compatible
launcher temporarily occupy Card Line's center without becoming a real card.

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
- `cardStage`: whether Card Stage is active, its `inactive`, `cardLine`, or
  `active` presentation, the selected card UUID, and ordered selected-stack
  member UUIDs.
- `desktopStage`: whether any output currently owns a Bento session.
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

Version 1 intentionally has no recent-activity history. Tettegouche can identify
an open result from the snapshot and activate that exact window through the
separate command above; unmatched results still use Plasma's normal application
launch action.

## Launcher guest protocol 1

Tettegouche must first call `launcherGuestProtocolVersion`. It may request guest
mode only when the result is exactly `1`; a missing or different result means it
must retain its standalone surface. This prevents an older Kadunce build from
receiving a guest it cannot present.

```text
launcherGuestProtocolVersion() -> integer
beginLauncherGuest(uniqueOwner) -> compact JSON reply
updateLauncherGuest(horizontalDelta)
finishLauncherGuest(horizontalDelta) -> committed boolean
endLauncherGuest()
```

An accepted begin reply contains `protocol: 1`, `accepted: true`, the target
`output`, and `card` and `active` geometries. Kadunce reserves the center but
does not insert the launcher into `CardLineModel`; real cards remain its sole
mutable model state. Tettegouche owns and renders the interactive center card,
while Kadunce mirrors its horizontal drag onto the adjacent real cards.

Kadunce watches the launcher's unique session-bus owner and restores Card Line
if that process disappears. A committed handoff selects the incoming real card,
and input directed at another application dismisses the guest before normal
Card Line interaction continues. Any rejected begin request leaves both
applications in their existing standalone behavior.
