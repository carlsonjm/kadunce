# Kadunce → Tettegouche context contract

Kadunce owns KWin discovery, card identity, stack relationships, workspace
presentation, output sessions, and device posture. Tettegouche owns invocation,
querying, ranking, and the choice to focus an existing application or launch a
new one.

The first boundary is deliberately read-only. Tettegouche requests a snapshot
when its launcher opens; it does not poll raw KWin state or mutate Kadunce's
models.

## Endpoint

The installed KWin effect exports this session D-Bus method:

```text
service:   org.kde.KWin
path:      /Kadunce
interface: studio.warbler.Kadunce
method:    workspaceContext
result:    compact UTF-8 JSON string
```

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

Version 1 intentionally has no mutation method and no recent-activity history.
The first Tettegouche launcher can rank an existing focused/card-backed window
from this snapshot. A focus-or-launch command and activity history should be
added only when their product behavior is defined.
