# Kadunce → Tettegouche context contract

Kadunce owns KWin discovery, card identity, stack relationships, workspace
presentation, output sessions, and which router owns the system edges. Tettegouche owns invocation,
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
interface: co.goodinput.Kadunce
method:    workspaceContext
result:    compact UTF-8 JSON string
```

After resolving an existing result from that snapshot, Tettegouche may ask
Kadunce to activate its exact window:

```text
service:   org.kde.KWin
path:      /Kadunce
interface: co.goodinput.Kadunce
method:    activateApplicationWindow(windowId)
result:    true only when that live application window was activated
```

The command accepts only a current application `windowId` from the snapshot.
Kadunce restores minimized windows and routes card-backed windows through its
existing activation handling, so selection, stack membership, and presentation
remain under Kadunce's authority.

### Who may call

Anything on the session bus may ask what Kadunce shows, claim a top edge or
the context line for itself, and change the settings. Only the programs the
suite's surfaces live in may bring anything forward: `activateApplicationWindow`,
`raiseKeyboard`, `keyboardHeading`, `showCardLine`, `showActive`, `toggleTable`,
`previewDesktop`, `commitDesktopPreview`, the placement calls,
`toggleBentoOnOutput` and `handoffBentoLeadToOutput`. Those are `plasmashell`,
`kscreenlocker_greet`, `tettegouche`, `shuffle-keyboard` and `plasma-keyboard`,
installed under `/usr` or in the person's `~/.local/bin`, as the caller's
process names its program. Any other caller is answered
`org.freedesktop.DBus.Error.AccessDenied` and nothing changes. The guest calls
have their own rule (§ Launcher guest protocol 3).

The payload identifies itself with schema
`co.goodinput.kadunce.workspace-context` and integer `version: 1`. Consumers
must reject unknown major versions instead of guessing at fields.

## Version 1 payload

- `focus`: focused window identity, application identity, title, and whether
  Kadunce currently owns it as a card; otherwise `null`. While a Gooseberry
  note over a window has the keys, `focus` names that window, with `notes`
  `true`: it stays the window in use.
- `applications`: current-desktop application windows, including minimized
  windows. Each entry exposes KWin's window-lifetime UUID, desktop application
  identity, title, output, focus/minimize state, and current card membership.
- `cardStage`: whether Card Stage is active, its presentation, the selected
  card UUID, and ordered selected-stack member UUIDs. The presentation is one
  of `inactive`, `cardLine` (Spread), `bento` (the display shows its Bento
  layout while Card Stage holds its cards hidden), `desktop` (the card display
  shows the Plasma desktop while Card Stage holds its cards hidden), or
  `active`. `cardLine` is a
  frozen interface value that a later versioned migration renames.
- `desktopStage`: whether any output currently owns a Bento session.
- `table`: whether Table is open (`open`). A change is signalled as Table
  opens and as it closes.
- `cardStage.spreadEntry`: in Spread, the entry it has chosen, the one in the
  middle; otherwise `null`. It has the card's `cardId`, the `name` Spread
  writes under it (for a pane's card, its application then `with` the
  panes beside it), `bento` (whether it is a pane's card), its Stack's
  `stackPosition` and `stackSize` (1-based, `1` of `1` for a lone card; while a card is held in its own Stack, the
  place it will land in), and whether it is `sleeping`.
- `notice`: what Kadunce is saying for a moment, or `null`: `kind` is
  `refused` for a gesture that sprang back, `zones` for the monitors' zones
  switched with Meta+Shift+B, or `returnToDesktop` while a card held over the
  bottom edge would leave for the desktop; with its `text` in words and the
  `output` it is said on, empty for every display.
- `lastActivated`, optional on each application: an in-memory, monotonic
  sequence for this effect's lifetime. Snapshots without it keep
  focused/selected/frontmost ordering.
- `displayContext`: the selected edge backend (`z13-direct`, a frozen value
  meaning Kadunce's own router, whenever a touchscreen drives a display, or
  `plasma-native`), every output's role (`tablet` for the one card display),
  geometry, and Bento ownership, and `spreadDisplay`, the display the person
  chose for Spread, empty while Kadunce chooses.

`windowId` and `cardId` use KWin's internal UUID and are stable for the life of
that window. `cardIndex`, stack position, and titles are presentation metadata;
Tettegouche must not persist them as identity.

## Query example

```bash
qdbus6 org.kde.KWin /Kadunce co.goodinput.Kadunce.workspaceContext
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

`uniqueOwner` is the caller's own unique connection name: a begin naming any
other is refused, and every later call above is answered only from the guest's
own connection, so no program can steer or end another's guest, or have
Kadunce call a third party. Kadunce watches the launcher's unique session-bus
owner and restores Spread if that process disappears. A committed handoff selects the incoming real card.
A press outside the launcher and the keys is Kadunce's (`INPUT.md` § Spread):
it never reaches the application under it, so it cannot promote that
application to Active. Kadunce accepts the launcher only while Spread is presented,
and never opens Spread for it (`CARD-LIFECYCLE.md` §11). Any rejected begin
request leaves both applications in their existing standalone behavior; a
rejection may name the `output` the launcher floats on, where the person is
working. Until the launcher then signals `shown(false)` on `/Launcher`, Spread
opening, however it opens, calls its `open` there: it asks to begin again and
joins as one opened in Spread.

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

`workspaceContextChanged` signals added, closed, and activated windows, and
the first frame drawn after the reported presentation changes. Tette
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
included, is the launcher guest's. Two rules apply:

- Over an Active card a companion, refused as the launcher is, draws its own
  card, and an application it launches takes the Active card's place, with
  Spread never shown (`CARD-LIFECYCLE.md` §8, While an individual card is
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

## Top-edge claims

Pulling down from the top edge brings Table, and a contact on a panel there is
the panel's until it pulls. A surface whose own pull means something else, a
status bar's say, keeps a strip of the edge from Table:

```text
claimTopEdge(key, outputName, x, y, width, height) -> bool
clearTopEdgeClaim(key)
```

The strip is in that display's own logical coordinates. A contact starting in
it is the surface's however it moves, and never Table's. A caller holds its
strips under keys of its own; a new claim under the same key replaces the old,
and every strip a caller holds goes when it leaves the bus. Kadunce refuses a
claim with no key, display or area.

## Context-line claims

A surface that shows the workspace's context in words, a status bar's line,
can take over the words Kadunce draws on a display:

```text
claimContextLine(key, outputName) -> bool
clearContextLineClaim(key)
```

While a claim on a display holds, Kadunce draws no words there: no names or
Stack places under Spread's cards, no note for a refused gesture, no
`return to desktop` on the outline of a card leaving at the bottom edge, and
sleeping cards stand in Spread at full strength rather than dimmed. The
outlines, ladders and previews stay, since a hand is aimed at them. With any
claim held, the zones switched with Meta+Shift+B are not sent to Plasma's
popup. Everything left undrawn is in the workspace context instead
(`cardStage.spreadEntry` and `notice`), and each change to it, the entry
chosen as Spread moves, its Stack place as a card is reordered, a notice
beginning or ending, sends `workspaceContextChanged` once. A notice lasts as
long as the note it replaces would have, about two and a half seconds.

Claims are kept as top-edge claims are: under the caller's own keys, a new
claim under the same key replacing the old, all of a caller's going when it
leaves the bus. Kadunce refuses a claim with no key or display. With no claim,
Kadunce draws as it always has, and the context carries the same fields.

## Stuck notes from Gooseberry

Kadunce also reads from Gooseberry, the optional notes application, which may
stick notes to a window. Gooseberry is found at run time and never required or
started: while its name is absent from the session bus, Spread draws no notes
and every other behaviour is unchanged.

```text
service:   io.github.carlsonjm.gooseberry
path:      /StuckNotes
interface: io.github.carlsonjm.Gooseberry.StuckNotes
```

| Kadunce uses | When |
| --- | --- |
| `Windows()` | Gooseberry appears on the bus, a signal comes from a sender not yet heard from, or Spread opens |
| signal `WindowsChanged(windows)` | It replaces everything Kadunce holds; Kadunce keeps no notes of its own |
| `Write(noteId, windowId, caption, app)` | A note drawn on a card is tapped: the card opens with the window's notes up and that note ready to type on the window. A build without it answers with an error, and Kadunce then asks `Show` |
| `Show(windowId, caption, app)` | A card's stack is tapped: the card opens with the window's notes up |
| `StickTo(noteId, windowId, caption, app)` | A carried note is let go on another card |
| `Pause(paused)` | `true` as Spread opens, so notes out over a window do not float over the cards without being put away, and as a window is picked up to be carried, so they do not stay behind under the hand; `false` as Spread closes or the carried window lands. A build without it answers with an error, which is ignored. Kadunce does not wait for it: from the frame Spread opens until it closes, it leaves undrawn the notes over a window: Gooseberry's layer surface, named by KWin after its program `gooseberry` since a layer surface carries no app id, that takes no keys or covers less than its display, since its quick-note card covers the whole display and takes them. KWin's open and close animations are held off that surface while Spread is shown, so it never shrinks or fades over a card |

Every call is asynchronous, asks the bus not to start Gooseberry, and is never
waited for while drawing. An entry belongs to a card when one of its
`windowIds` is the card's window UUID, compared as a UUID with or without
braces; `Write`, `Show` and `StickTo` name the window by that UUID with braces, its
title, and its desktop file name. Kadunce reads each entry's count, whether its
notes are `shown` and, if so, `folded` into a dot, and its notes' ids, titles and colours, top note first, with
each note's place, `x` and `y` as shares of the window, and size, `width` and
`height` in the window's pixels, where Gooseberry gives them. A place that is
absent, negative or unreadable leaves the note unplaced, as with a Gooseberry
that sends none.

In Spread each note placed on its window stands on the card in the same place,
as big as there, both scaled with the card and kept inside it, the newest on
top; one never resized there is drawn at the size Gooseberry starts a note at.
Each frame it stands on the window as KWin draws it on the card that frame, so
it rides the card as Spread opens, closes and the row moves, and is never
drawn in the corner on its way to its place.
The notes never placed show as one mini note at the card's bottom-right
corner, the same size on every card: the top one in its colour with its first
words, the next one's edge peeking above it and their count quietly in its
corner when there are several.
A shown entry `folded` shows only its dot, as Gooseberry draws it on the
window: a disc in the top note's colour, the count on it, hung from the top
note's top-right corner, its place scaled with the card, its size the same
on every card. A tap on it asks `Write` for the top note; holding it carries that note.
Notes in place show in full while Gooseberry says they are shown over the
window, and faint while they are put away. A stuck note is typed on its window,
never in Spread: a tap on a note opens its card, as a tap anywhere on the card
does, and asks `Write`, so the note is out on the window with the others,
ready to type; a tap on the stack opens the card and asks `Show`. Gooseberry
stays paused until the card is Active, and then draws the notes over it.
From the frame Spread closes onto a card whose notes are shown, or were just
asked up, Kadunce goes on drawing them over the card where Gooseberry draws
them, and leaves Gooseberry's surface undrawn with its animations held off,
until that surface stands on the card's window or 800 ms pass; nothing then
slides or fades in after the card settles.
Nothing comes up over the card itself. Holding a note carries it to another
card (`INPUT.md` § Spread).
This takes up Gooseberry's requests for a card's notes on its corner in Spread
and for carrying a note between cards. Spread never moves or resizes a window
for a note, and the Active card shows none.
