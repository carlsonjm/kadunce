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
| `noteLaunchOrigin(applicationIds, x, y, width, height)` | the icon launched from |
| signal `placementSettled(requestToken, windowId, placed)` | where the request ended |

Points are global logical pixels. `applicationIds` names the application,
desktop file first; `windowId` is its KWin internal id, or empty if not running.

## Where a point leads

- Over the dock's band, at the bottom of any display: `none`.
- On the display that owns cards (`CARD-LIFECYCLE.md` §2): a side edge pairs
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

A running application's window is placed at once, as `placementSettled` reports. With an empty `windowId`, Kadunce waits up to 10 seconds for the
application's first window and returns `true`; the requester launches it.
Kadunce admits that window like any other arrival and places it, and
`placementSettled` reports it. A wait that times out, is cancelled or is
replaced by a newer launch settles with `placed` false.

A card asked onto another display goes there as the Active card carried there
would, from where it was before it was a card. An ordinary window on another
display asked onto the card display becomes a card there, and is then placed
as a card already there would be. Kadunce refuses, with `false`, a Bento pane
asked anywhere but its own display.

## Launch origin

The dock names a tapped icon's rect before launching; a first window opening
as the only card within 10 s grows from it.

## Bringing apps back

Kadunce remembers where every application stood, and puts the applications
back there after the next sign-in when a driving program asks. Shuffle Core
asks the person and launches the applications; Kadunce never launches one.

| Call | Result |
| --- | --- |
| `workspaceMemoryProtocolVersion()` | `1` |
| `savedSession()` | `available`, and with it `count`, `apps`, `missing`, `when`, `clean` and `windows` |
| `bringBack(requestToken)` | `accepted`, `launch`, `alreadyOpen` and `count`, or `reason`: `nothing` or `busy` |
| `cancelBringBack(requestToken)` | applications not yet back are missed |
| `dismissSavedSession()` | the session is no longer offered |
| `openTogether()` | each set as `apps`, `count` and `lastSeen` |
| `forgetWorkspaceMemory()` | the record and every count are cleared |
| signal `appBroughtBack(requestToken, appId, windowId, placed)` | one application's first window went to its place, or could not |
| signal `bringBackFinished(requestToken, placed, missed)` | the bring-back is over |

`apps`, `missing`, `launch` and `alreadyOpen` are desktop ids, `missing` the
apps of `apps` not running when asked; `when` and `lastSeen` are
Unix seconds; `clean` is false when the record is the last periodic one, as
after a crash.

### What is remembered

Every minute, and once more as the session ends, Kadunce writes `$XDG_DATA_HOME/kadunce/workspace-memory.json`. For
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
  A pane whose partner never returns is a card. The card that was in use
  is in use again at the end.
- On another workspace, it goes there and onto its display, and becomes a card
  when that workspace is shown.
- A display that is gone falls back to the display in use, and a workspace
  that is gone to the one shown. An application already running stays where
  it is; its later windows open as any does.

While offered or coming back, its workspaces stay (`TABLE.md`).

### Opened together

Applications opened by hand within five minutes of the first window after
sign-in form one set, kept in `$XDG_DATA_HOME/kadunce/open-together.json` with
its count. A set needs two applications. Nothing is gathered when switched on with windows open, nor are applications brought back counted. The 32 most recently seen sets are kept.

## Opened from a card

A window that one application's card opens in another application can land
where the person chose: in front of that card's Stack, beside the card, or on
its own. Nothing is followed unless set. What is followed, and the pairs, are
kept in `kaduncerc` `[Stacks]` as `Opened` and `Pairs`.

| Call | Result |
| --- | --- |
| `openedWindows()` | `follows`: `off`, `pairs` or `every`; and `pairs`, the pairs kept |
| `setOpenedWindows(follows, pairs)` | keeps both, followed from the next window; a pair that does not read is dropped |
| `stacksOpenedWindows()` | whether anything is followed |
| `setStacksOpenedWindows(stacks)` | follows `every` or `off`, keeping the pairs |

A pair is text, `from>to=place`: the application of the card, the application
of the window it opens or `*` for any, each a desktop file id, and `front`,
`beside` or `alone`. `org.kde.dolphin>org.gnome.Loupe=front` stands Loupe in
front of Files. The pair naming both applications wins over the pair naming
any. With `pairs`, applications in no pair open as any window does; with
`every`, they stand in front, and a pair still says otherwise.

- `front`: it joins the card's Stack in front and is the card in use; the
  card waits behind it.
- `beside`: the two share the screen, the new window at the right and in use.
  Beside a pane, the layout takes it as it takes any window (§8).
- `alone`: it opens as its own card and nothing stands it beside the card, on
  a Connected display too. From a pane it opens as any window does.

Kadunce reads where a window opened from only from what the desktop knows. An
application that opens another asks the compositor for a launch ticket (an
xdg-activation token), which the compositor gives only for your latest press,
tap or key, and the window that opens presents it. A window joins when it
presents a ticket the compositor gave, and the press that ticket was given
for landed on a card: read as the ticket is given where the compositor
announces that, and otherwise as the window opens. A ticket a later one was
seen before that press is for an earlier press, and names no card. How soon a
window opens after another never counts, nor which process started it.

A window is not placed by its pair when:

- it presents no ticket, or one the compositor refused, as a command typed in a
  terminal gets;
- the press landed on a panel, the dock, Search or the desktop, or was a key
  one of the desktop's shortcuts took;
- a program asked for it by name: a placement request, Search's launch, a
  saved layout or a bring-back, each of which names its own place;
- it belongs to the application of the card it opened from;
- it opened on another display than that card;
- it does not fit the card's pane, or a window is being carried.

Where a window in front stands (`CARD-LIFECYCLE.md` §6, §5):

- From the Active card, it is the Active card in front of that card's Stack,
  on a Connected display too, where it would otherwise stand beside the card.
- From a pane of a Connected layout, it takes that pane and the card waits
  behind it with the rest of the pane's Stack; no other pane moves. Closing
  it brings the card back to the pane.
- From a pane of any other layout it opens as any window does (§8).

A window whose ticket follows it by up to a second joins once the ticket
comes; it has stood as its own card or pane until then. Taking it out of the
Stack (`INPUT.md`) undoes the join, and nothing joins it again. An
application already open that shows the link in a window it has opens no
window, so nothing joins. Each window's origin is written in the journal on a
line beginning `window origin:`, switched on or off.

## Saved layouts

A driving program keeps named desks: groups of applications on one workspace,
each a lone card or one Stack, with the display it stood on and, for a pane
of a layout, its shape there. Kadunce reads the desk shown and arranges a
desk from groups; it never launches an application. Shuffle Core keeps the
layouts, launches what is not running and asks Kadunce for the rest.

| Call | Result |
| --- | --- |
| `layoutProtocolVersion()` | `1` |
| `currentLayout()` | `workspace`, `groups`, `everywhere` and `displays` |
| `arrangeLayout(requestToken, groups)` | `accepted`, `launch` and `alreadyOpen`, or `reason`: `busy` or `empty` |
| `cancelLayout(requestToken)` | applications not yet come are missed |
| signal `layoutAppPlaced(requestToken, appId, placed)` | one application's window went to its place, or could not |
| signal `layoutFinished(requestToken, placed, missed)` | the arrangement is over |

`groups` is an array of dictionaries (`aa{sv}`), `apps` desktop ids, a Stack's
front first, and `stack` true for two or more applications sharing a Stack.
A group read from the desk has its place: `display`, the display's name; for
a pane, `pane` with `x`, `y`, `width` and `height`, its shape in shares of the
room its display gives panes; and `pictureX`, `pictureY`, `pictureWidth` and
`pictureHeight`, where its front window was drawn in shares of the display,
for a picture only; and `shown` for a pane, the card in use, or a window
in view. A group sent without a place goes to the display in use.

### Reading the desk

`currentLayout` reads the workspace shown, display by display: a display's
cards left to right, each Stack front first, then its panes, each with the
cards waiting behind it as one Stack, then other windows, front-most first.
`groups` is the display in use, `everywhere` every display, the one in use
first, and `displays` each one's `name`, `x`, `y`, `width`, `height`, `inUse`. A window whose application has no desktop file is left out, and an
application appears once, in its first group.

### Arranging

Groups are cleaned first: blank ids and an application already named go, and a
group that is not a Stack stands each application alone. An application
running anywhere uses its front-most window, a shown one before a minimized
one; every other is in `launch`, and its first window is claimed as it opens.
Each window goes to the workspace in use and its group's display, or the
display in use when that one is not connected, and becomes a card there.
A Stack forms in the listed order as its applications come; on a display
showing its layout, its first pane stays in front, its other panes join it in
order, and its cards wait behind them. Once every
application has come, or after 60 seconds, the groups lead the row in order,
the card `shown` in use, else the first, and the cards outside the layout follow as they stood;
each display's row is put in order, the display in use last. Then a display
with two or more of the layout's panes is stood whole: a pane the layout does
not name waits in Spread, and the layout's panes take their kept shapes as a
divider let go leaves them. Where that cannot be done whole, the panes stay
as the display laid them out and the log says why.
A window not in place within 5 seconds is reported not placed. A Stack Cards cannot form at once, as while Search is still closing,
is tried again for six seconds; the log names why. Applications a layout opens are not counted as opened by hand.

One arrangement runs at a time, never with a bring-back: either answers
`busy` while the other runs. An empty token or no applications answers
`empty`. Nothing is closed.
