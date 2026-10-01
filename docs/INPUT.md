# Input

Every gesture, tap, click, wheel and key Kadunce answers, and what you see
happen. Anything else you do in an application stays with that application;
`CARD-LIFECYCLE.md` says what each result does to a window.

| Task | Touch | Keyboard |
| --- | --- | --- |
| Spread | Swipe up from the bottom edge | `Meta+S` |
| Active card | Tap a card in Spread | `Enter` in Spread |
| Bento | Drag a window to the left or right edge | `Meta+B` |
| Table | Pull down from the top edge | `Meta+W` |
| Plasma desktop | Switch Kadunce off in the tray | `Meta+Esc` |

Kadunce's keys sit on `Meta` and work only while it runs. `Ctrl` stays with
applications, `Alt+Tab` with KDE, and `Meta+G` and `Meta+E` open Tettegouche's
Browse everything and Files.

## Spread

| Input | What happens |
| --- | --- |
| Swipe up from the bottom edge | Spread opens and follows your finger; a quick flick opens it all the way. |
| Three fingers down on the touchscreen, or four on a touchpad | Spread opens. From the Active card, the card shrinks into the row under your fingers and goes back if let go before halfway; from a Bento layout or the desktop, Spread opens past halfway. KDE's Overview is set aside while Kadunce runs. |
| `Meta+S` | Spread opens, or closes back to the Active card. A Bento layout joins the row as one group card. |
| Drag the row sideways, by finger or mouse | The row follows, coasts after a flick, settles on a card and springs back at either end. |
| Wheel over the row | The row moves one card. |
| `Left`, `Right`, `Up`, `Down` in Spread | The row, or the centred Stack, moves one card. |
| `Meta+Left`, `Meta+Right` in Spread | The row moves to the card before or after. |
| Tap or click empty space, or press `Escape` | You go back to where you were: the card last open, or the layout you opened Spread from. |
| Flick a card up | Its application closes; one that asks first comes forward with its question. A short lift springs back. The Bento group never closes this way. |
| Pull down on the Bento group | The pane under your finger leaves the layout and becomes a card of its own. |
| Hold a card | It rises under your finger to be carried; Spread stays three across. Held off the middle, the row slides under it, faster further out. |
| Pull a held card down | The row zooms out to one set view; push it back up and it returns to three across. |
| Let a held card go in a gap | It lands there as the row grows back to three across around it. |
| Let a held card go on another card, once that card has risen | The two become a Stack. On the Bento group the pane under your finger gives way and the card slides under; let go and the layout opens with it there. |
| Let a held card go on another display | It becomes a Bento pane or an ordinary window there. |
| Right-click | Nothing: Spread has no window menu. |
| Tap or click outside Search and the on-screen keys | Search closes. A stroke that moves does not close it, and a touch on the keys types into it. |
| Plasma's bottom or top touch edge, on a touchscreen other than the ROG Flow Z13's | The bottom opens Spread; the top closes it. |

A swipe that starts on the dock stays with the dock, and while the on-screen keys
are up, the bottom edge is theirs.

### Stacks

| Input | What happens |
| --- | --- |
| A slow sideways stroke begun on the centred Stack | Its cards come to the front in turn. A quick stroke moves the row instead. |
| Wheel over the centred Stack | Steps through its cards. |
| Tap or click a card fanned behind the centred Stack's front | It comes to the front, and Spread stays open. |
| Pull a Stack's card down | It leaves the Stack and becomes a card just after it; the Stack keeps its place and order. A pull that stops short springs back. |
| Hold a Stack's card | It lifts out, and the Stack parts at an outlined place where it would go; sideways moves that place, left toward the back. Let go and the card takes it, and the card then in front shows; where it began, nothing changes. |

## Active card

| Input | What happens |
| --- | --- |
| Tap or click a card in Spread | It opens as the Active card; the Bento group opens as its layout. |
| `Enter` in Spread | The centred card opens. |
| Let a held card go at the top edge | It becomes the Active card. |
| Drag a window by its title bar to the top edge | It becomes the Active card, and a Bento pane leaves its layout. The first time on the card display, every other window there becomes a card too. |
| Drag a window onto the card display | It becomes a card. |
| Tap, click or swipe inward in the gap beside the Active card | The card before it (left gap) or after it (right gap) becomes Active. A Stack is a ring: its cards come in turn and round again; Spread or the dock leave it. |
| Wheel in the gap beside the Active card | Up for the card before, down for the card after. |
| `Meta+Left`, `Meta+Right` | The card before or after opens, round a Stack as a swipe goes. Only while cards are shown; elsewhere these keys stay KDE's. |
| `Meta+Up`, `Meta+Down` | The previous or next card of the current Stack. Only while cards are shown. |
| Tap Keyboard in Control Center or the tray | The keys rise for the window in use, and the Active card makes room. |
| Point or drag in the gutter around a card or Bento pane | The pointer stays an arrow, and nothing resizes the window from outside its edge. |

## Bento

| Input | What happens |
| --- | --- |
| Drag a window by its title bar to the left or right edge of the card display | The window and a partner become a Bento pair, the window on the side you let go. The partner is the Active card or, when the window is the Active card, its nearest neighbour on that side of the row. The edge's upper half gives it the larger pane, the lower half the smaller. With no partner, it becomes the Active card. |
| `Meta+B` | Starts or ends Bento on the largest attached monitor, or on the card display when none is attached. There it pairs the Active card, on the left, with the next card to its right. |
| Drag a window to an edge of a display that cannot hold cards | One window alone takes half the display at a side, or the Active card's size at the top. With two or more there, every window joins one layout, the dragged one on the side you let go; one with no room waits in the dock. |
| Hold a divider briefly, then drag | The panes on both sides resize; letting go keeps the new split. |
| Drag a Bento pane onto another pane of its layout | The two swap places. Let go on its own place and nothing changes. |
| Minimize a pane | It leaves the layout as a sleeping card. A layout left with one pane ends, and that pane becomes a card. |
| Press `Escape`, or add a finger, while dragging a window | The drag is cancelled and the window stays as it was. |

## Table

| Input | What happens |
| --- | --- |
| Pull down from the top edge | Table opens. |
| `Meta+W` | Table opens, or closes. |
| Push the pointer into a display's top-left corner | Table opens there. |
| Three fingers up on the touchscreen, or four on a touchpad | Table opens; again, it closes. |
| `Escape` | Table closes and nothing changes. |

Table is part of Shuffle for Plasma; a Kadunce build without it doesn't answer
these inputs.

## Plasma desktop

| Input | What happens |
| --- | --- |
| Switch Kadunce off in the tray | Every window returns to the ordinary Plasma desktop, then Kadunce unloads. |
| `Meta+Esc` | Every window on every display and desktop returns to the ordinary Plasma desktop. The next window to open on the card display starts cards again. |
| Drag the Active card or a Bento pane to the bottom edge | It returns to the ordinary Plasma desktop where you let it go; other cards wait in Spread. |
| Switch Kadunce on in the tray | The card display's windows become cards, the one in use the Active card. |
