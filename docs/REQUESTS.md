# Placement requests

Another program can carry an application to a point and ask Kadunce to open it
there. The Shuffle dock uses this when an app is carried up out of its band.
Kadunce decides where the point leads and owns every window it places; the
requester only names the application and the point.

## Endpoint

```text
service:   org.kde.KWin
path:      /Kadunce
interface: studio.warbler.Kadunce
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
  A side edge on a display Kadunce does not yet own makes it the Active card.
- On any other display: a side or top edge begins or joins that display's
  layout as a carried window would (§ 10, § 11), and anywhere else opens the
  application there.

## Placing

A request is placed only where it was last aimed; a point that aims anywhere
else, or a request with no aim, is refused with `false`.

A running application's window is placed at once and `placementSettled`
reports it. With an empty `windowId`, Kadunce waits up to 10 seconds for the
application's first window and returns `true`; the requester launches it.
That window is admitted as every arrival is, then placed, and
`placementSettled` reports it. A wait that ends unanswered, is cancelled or is
replaced by a newer launch settles with `placed` false.

Kadunce refuses, with `false`, a window it cannot place by request: a card
asked onto another display, a window on another display asked onto the card
display, and a Bento pane asked anywhere but its own display.
