# Architecture

## Product boundary

KWin owns real windows. Kadunce owns a logical card model, output-local
workspace sessions, input routing, and the compositor presentation derived
from them.

Card Line does not store or apply off-screen client coordinates. The Desktop
Stage uses real, reversible geometry because its windows remain simultaneously
interactive. A cross-display handoff changes physical ownership only after a
committed destination accepts the transaction.

## Golden rule

The tablet is the attention stage. The external display is the composition
stage. Neither presentation should imitate the other. When no external
display is detected, the tablet may temporarily become the composition stage,
but Card Stage is released before Desktop Stage takes ownership.

## Current implementation

The native KWin plugin currently has four runtime owners:

- `WorkspaceInputRouter`, which owns gesture recognition, pointer/touch
  transactions, dwell timers, and semantic command dispatch without access to
  compositor state;
- `CardStageController`, which owns Active, Card Line, stacks, selection,
  admission, and reversible card transactions through a typed host boundary;
- `DesktopStageController`, which owns external-display layout sessions,
  admission, rail resizing, handoff, settle retries, and exact restoration;
- `Effect`, which coordinates KWin lifecycle, output discovery, context export,
  shortcuts, cross-controller routing, and the remaining card renderer.

`CardLineModel`, `CardLineLayout`, and `BentoLayout` are isolated, headlessly
tested domain primitives. Tettegouche consumes Kadunce state only through the
versioned read-only context contract in `TETTEGOUCHE-CONTEXT.md`.

## Refactoring destination

The accepted behavior will be separated behind four owners while the KWin
effect becomes a thin lifecycle coordinator:

1. `CardStageController` — Active, Card Line, stacks, selection, and reversible
   card transactions. **Extracted.** It communicates only through the typed
   `CardStageHost` boundary and exposes a read-only render view.
2. `DesktopStageController` — layout sessions, minimum-size admission, rail
   resizing, cross-display acceptance, and restoration. **Extracted.** It
   communicates only through the typed `DesktopStageHost` boundary.
3. `CardRenderer` — painting, transformations, fan aperture, and HUD geometry.
4. `WorkspaceInputRouter` — gesture ownership, pointer transactions, timers,
   and semantic destinations. **Extracted.** It communicates only through the
   typed `WorkspaceInputTarget` boundary.

The coordinator owns KWin registration, output discovery, context publication,
and controller orchestration. Controllers use direct typed calls; there is no
event bus or duplicate state authority. `CardRenderer` remains the next
isolated extraction.

## Handoff contract

1. Input identifies a semantic destination.
2. `CardStageController` creates a reversible transfer token.
3. `DesktopStageController` accepts or rejects it.
4. The source commits removal only after acceptance.
5. Rejection restores exact stack order, selection, and source state.

## Invariants

1. A client has one physical KWin output.
2. Card Line never mutates real client geometry.
3. Every physical mutation has one owning restore snapshot.
4. Rendering consumes state but does not mutate controllers.
5. Input routes semantic commands but does not edit models directly.
6. Other applications consume context without entering Kadunce's card registry
   or compositor ownership model.
7. Disable restores clients before unloading the effect.
