# Input ownership

Input routing recognizes intent and dispatches semantic commands. It does not edit
card models, perform native placement, or infer ownership from paint state.

## Route table

| Sequence | Owner until termination |
| --- | --- |
| Native desktop move/resize | KWin, unless exact Kadunce takeover proof succeeds |
| Pointer press on a Plasma panel | Plasma through release |
| Contact inside a companion guest | Guest through release |
| Contact outside an open guest on the tablet | Kadunce; stationary release may dismiss, movement cancels dismissal |
| Contact on Card Line or Active chrome | Kadunce through the semantic transaction |
| Provisional bottom-edge touch | Client until deliberate upward intent and successful native cancellation |
| Foreign or unmatched release | Original route; it cannot activate a card |

## Device-local state

Touch and pointer state are independent. Canceling one device cannot clear the
other device's hold, grab, timer, or forwarded ownership. Each timer and delayed
action carries the initiating device and generation. A callback revalidates both
before acting.

Canceled contacts remain in a drain set until physical release, so a release cannot
fall through to a new route. Forwarded pointer ownership retains all held buttons;
releasing one button does not end the route while another remains held.

## Native takeover proof

Kadunce may take over an ordinary native move only when all of these still match:

- exact weak window identity;
- initiating pointer button or touch identity and source surface;
- the same native move/resize lifetime;
- controller/workspace generation and output topology;
- an eligible move rather than resize, keyboard, or unsupported request;
- a valid deliberate Kadunce destination.

Wayland application moves use the xdg-toplevel serial. Xwayland client-side moves
correlate `_NET_WM_MOVERESIZE` after KWin has accepted the request. The observer does
not consume or replay the client request. Failed or ambiguous proof remains native.

## Edge and panel rules

Ordinary application contact is passed through until deliberate reserved-edge intent
is established. Panel input always remains with the panel. The physical bottom edge
may be a drop destination even when a dock occupies that area; this does not grant
Kadunce ordinary dock hit testing.

Automatic electric-border tiling/maximize settings are suppressed in memory while
the effect is active and restored on unload. Explicit Shift custom tiling and manual
or keyboard operations stay native.

## Cancellation and teardown

Source close, output loss, topology change, manual takeover, view release, effect
unload, or competing input cancels affected actions and timers. A card grab rolls
back before destination acceptance. After accepted logical transfer, interruption
belongs to the destination and cannot restore stale source state.

On effect teardown, cancel and destroy the router while controllers remain alive;
then restore windows. No timer, native observer, or callback may survive its owner.

## Test boundary

Headless and private-compositor tests prove routing, drain, and callback rules. They
do not prove hardware event ordering, a real panel/guest surface, or live compositor
unload. Use `../tests/unload-probe/README.md` and `TEST-ENVIRONMENT-PROCEDURE.md` for
isolated tests. Live disable or input injection requires explicit authorization.
