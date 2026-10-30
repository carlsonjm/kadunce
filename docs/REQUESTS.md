# Placement requests

Another program can carry an application to a point and ask Kadunce to open it
there. The Shuffle dock uses this when an app is carried up out of its band.
The requester names the application and the point. Kadunce decides where the
point leads and owns every window it places.

## Endpoint

```text
service:   org.kde.KWin
path:      /Kadunce
interface: co.goodinput.Kadunce
```

| Call | Result |
| --- | --- |
| `placementProtocolVersion()` | `1` |
| `aimPlacement(x, y)` | `card`, `left`, `right`, `top`, `display` or `none`, and the destination outline is shown there |
| `clearPlacementAim()` | the outline goes |
| `placeApplication(applicationIds, windowId, x, y, requestToken)` | `true` when the request is accepted |
| `cancelPlacement(requestToken)` | a waiting launch is dropped |
| signal `placementSettled(requestToken, windowId, placed)` | where the request ended |

Points are global logical pixels. `applicationIds` names the application, its
desktop file first; `windowId` is the window's KWin internal id, or empty when
the application is not running.

## Where a point leads

- Over the dock's band, at the bottom of any display: `none`.
- On the display that owns cards (`CARD-LIFECYCLE.md` § 11): a side edge pairs
  the application into Bento as the Active card carried there would
  (§ 3, § 10); anywhere else, the top edge included, makes it the Active card.
  A side edge on a display Kadunce does not yet own makes it the Active card,
  except on a Connected display, where it takes that place alone.
- On any other display: a side or top edge begins or joins that display's
  layout as a carried window would (§ 10, § 11), and anywhere else opens the
  application there.

## Placing

Kadunce places a request only where it was last aimed. It returns `false` for
a point that aims anywhere else, and for a request with no aim.

A running application's window is placed at once and `placementSettled`
reports it. With an empty `windowId`, Kadunce waits up to 10 seconds for the
application's first window and returns `true`; the requester launches it.
Kadunce admits that window like any other arrival and places it, and
`placementSettled` reports it. A wait that times out, is cancelled or is
replaced by a newer launch settles with `placed` false.

A card asked onto another display goes there as the Active card carried there
would, from where it was before it was a card. An ordinary window on another
display asked onto the card display becomes a card there, and is then placed
as a card already there would be. Kadunce refuses, with `false`, a Bento pane
asked anywhere but its own display.

## Bringing apps back

Kadunce remembers where every application stood, and puts the applications
back there after the next sign-in when a driving program asks. Shuffle Core
asks the person and launches the applications; Kadunce never launches one.

| Call | Result |
| --- | --- |
| `workspaceMemoryProtocolVersion()` | `1` |
| `savedSession()` | `available`, and with it `count`, `apps`, `when`, `clean` and `windows` |
| `bringBack(requestToken)` | `accepted`, `launch`, `alreadyOpen` and `count`, or `reason`: `nothing` or `busy` |
| `cancelBringBack(requestToken)` | applications not yet back are missed |
| `dismissSavedSession()` | the session is no longer offered |
| `openTogether()` | each set as `apps`, `count` and `lastSeen` |
| `forgetWorkspaceMemory()` | the record and every count are cleared |
| signal `appBroughtBack(requestToken, appId, windowId, placed)` | one application's first window went to its place, or could not |
| signal `bringBackFinished(requestToken, placed, missed)` | the bring-back is over |

`apps`, `launch` and `alreadyOpen` are desktop ids; `when` and `lastSeen` are
Unix seconds; `clean` is false when the record is the last periodic one, as
after a crash.

### What is remembered

Every minute, and once more as the session begins to end, before cards are
given back, Kadunce writes `$XDG_DATA_HOME/kadunce/workspace-memory.json`. For
each window of an application with a desktop file it keeps the desktop id, the
display's name, the workspace's id, whether it was a card or a pane, its side
or row position, its size class and whether it was in use: no title, document
or workspace name. The record of the previous sign-in is the one offered,
until it is brought back or dismissed.

### Coming back

The requester calls `bringBack`, then launches each id in `launch`. Kadunce
waits up to 60 seconds for each application's first window:

- On the workspace shown, it is placed as a placement request on its display
  would be: a card, or a pane on its side once its partner has also arrived.
  A pane whose partner does not come back is a card. The card that was in use
  is in use again at the end.
- On another workspace, it goes there and onto its display, and becomes a card
  when that workspace is shown.
- A display that is gone falls back to the display in use, and a workspace
  that is gone to the one shown. An application already running stays where
  it is. Later windows of the same application open as any window does.

While a session is offered or being brought back, its workspaces are not
dissolved (`TABLE.md`).

### Opened together

The applications opened by hand within five minutes of the first window after
sign-in form one set, kept in `$XDG_DATA_HOME/kadunce/open-together.json` with
how often it has been seen. A set needs two applications. Kadunce switched on
with windows already open gathers nothing, and applications brought back are
not counted. The 32 most recently seen sets are kept.

## Saved layouts

A driving program keeps named desks: groups of applications on one workspace,
each a lone card or one Stack. Kadunce reads the desk shown and arranges a
desk from groups; it never launches an application. Shuffle Core keeps the
layouts, launches what is not running and asks Kadunce for the rest.

| Call | Result |
| --- | --- |
| `layoutProtocolVersion()` | `1` |
| `currentLayout()` | `workspace`, and `groups`, each `apps` and `stack` |
| `arrangeLayout(requestToken, groups)` | `accepted`, `launch` and `alreadyOpen`, or `reason`: `busy` or `empty` |
| `cancelLayout(requestToken)` | applications not yet come are missed |
| signal `layoutAppPlaced(requestToken, appId, placed)` | one application's window went to its place, or could not |
| signal `layoutFinished(requestToken, placed, missed)` | the arrangement is over |

`groups` is an array of dictionaries (`aa{sv}`), `apps` desktop ids, a Stack's
front first, and `stack` true for two or more applications sharing a Stack.

### Reading the desk

`currentLayout` reads the workspace shown on the display in use: its cards left
to right, each Stack front first, then its panes and other windows, front-most
first. A window whose application has no desktop file is left out, and an
application appears once, in its first group.

### Arranging

Groups are cleaned first: blank ids and an application already named go, and a
group that is not a Stack stands each application alone. An application
running anywhere uses its front-most window, a shown one before a minimized
one; every other is in `launch`, and its first window is claimed as it opens.
Each window goes to the workspace and display in use and becomes a card there.
A Stack forms in the listed order as its applications come. Once every
application has come, or after 60 seconds, the groups lead the row in order,
the first one in use, and the cards outside the layout follow as they stood.
A window that does not reach its place within 5 seconds is reported not
placed. Applications a layout opens are not counted as opened by hand.

One arrangement runs at a time, and never with a bring-back: either answers
`busy` while the other runs. An empty token or no applications answers
`empty`. Nothing is closed.
