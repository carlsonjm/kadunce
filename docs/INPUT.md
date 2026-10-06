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
Apps and Files.

## Spread

| Input | What happens |
| --- | --- |
| Swipe up from the bottom edge | Spread opens and follows your finger; a quick flick opens it all the way. |
| Three fingers down on the touchscreen, or four on a touchpad | Spread opens. From the Active card, the card shrinks into the row under your fingers and goes back if let go before halfway; from a Bento layout or the desktop, Spread opens past halfway. |
| `Meta+S` | Spread opens, or closes back to the Active card. A Bento layout joins the row as one group card. |
| Drag the row sideways, by finger or mouse | The row follows, coasts after a flick, settles on a card and springs back at either end. |
| Wheel over the row | The row moves one card. |
| `Left`, `Right`, `Up`, `Down` in Spread | The row, or the centred Stack, moves one card. |
| `Meta+Left`, `Meta+Right` in Spread | The row moves to the card before or after. |
| Tap or click empty space, or press `Escape` | You go back to where you were: the card last open, or the layout you opened Spread from. |
| Flick a card up | Its application closes; one that asks first comes forward with its question. A short lift springs back. The Bento group never closes this way. |
| Pull down on the Bento group | The pane under your finger leaves the layout and becomes a card of its own. |
| Hold a card | It rises under your finger to be carried; Spread stays three across. Held off the middle, the row slides under it, faster further out. |
| Pull a held card down | The row zooms out to one set view, and returns when pushed back up; not over the Bento group. |
| Let a held card go in a gap | It lands there as the row grows back to three across. |
| Let a held card go on another card, once that card has risen | The two become a Stack. On the Bento group the pane under your finger gives way and the card slides under; let go and the layout opens with it there. |
| Let a held card go on another display | It becomes a Bento pane or an ordinary window there. |
| Right-click | Nothing: Spread has no window menu. |
| Tap or click outside Search and the on-screen keys | Search closes; a card tapped beside it opens. A stroke does not close it, and the keys type into it. |
| Plasma's bottom or top touch edge, on a touchscreen other than the ROG Flow Z13's | The bottom opens Spread; the top closes it. Three fingers up bring Table. |

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
| Tap or click a card in Spread | It grows into the Active card; the Bento group opens as its layout. |
| `Enter` in Spread | The centred card opens. |
| Let a held card go at the top edge | It becomes the Active card. |
| Drag a window by its title bar to the top edge | It becomes the Active card, and a Bento pane leaves its layout. The first time on the card display, every other window there becomes a card too. |
| Drag a window onto the card display | It becomes a card. |
| Tap, click or swipe inward in the gap beside the Active card | The card before it (left gap) or after it (right gap) becomes Active. A Stack is a ring: its cards come in turn and round again; Spread or the dock leave it. |
| Wheel in the gap beside the Active card | Up for the card before, down for the card after. |
| `Meta+Left`, `Meta+Right` | The card before or after opens, round a Stack as a swipe goes. Only while cards are shown; elsewhere these keys stay KDE's. |
| `Meta+Up`, `Meta+Down` | The previous or next card of the current Stack. Only while cards are shown. |
| Tap or click Keyboard in Control Center or the tray | The keys rise for the window in use, and the Active card makes room. |
| Point or drag in the gutter around a card or Bento pane | The pointer stays an arrow, and nothing resizes the window from outside its edge. |

## Bento

| Input | What happens |
| --- | --- |
| Drag a window by its title bar to the left or right edge of the card display | The window and a partner become a Bento pair, the window on the side you let go. The partner is the Active card or, for the Active card itself, its nearest neighbour on that side. The edge's upper half gives it the larger pane, the lower half the smaller. With no partner, it becomes the Active card. |
| `Meta+B` | Starts or ends Bento on the largest monitor, or on the card display when none is attached, pairing the Active card with the next card to its right. |
| `Meta+Shift+B` | Monitors switch between filling themselves and your `Meta+T` zones, as the tray's switch does. |
| Drag a window to an edge of a display that cannot hold cards | One window alone takes half the display at a side, or the Active card's size on top. With two or more, all join one layout, the dragged one on your side; one without room waits in the dock. Switched to zones, those drawn with `Meta+T` are the layout, with KDE's edges. |
| Hold a divider briefly, or press it with the mouse, then drag | Both panes resize; letting go keeps the new split. |
| Drag a Bento pane onto another pane of its layout | The two swap places; dropped on its own place, nothing changes. |
| Minimize a pane | It leaves the layout as a sleeping card. A layout left with one pane ends, and that pane becomes a card. |
| Press `Escape`, or add a finger, while dragging a window | The drag is cancelled. |

## Table

| Input | What happens |
| --- | --- |
| Pull down from the top edge: above the Active card, or anywhere along the top in Spread, a Bento layout or the desktop | Table comes down as a row of tabs, one per workspace; sliding across them previews each workspace on every display; over Spread, from its cards. |
| Keep pulling past the tabs, then slide sideways | The tab under your finger stays chosen, its cards hanging below; sliding across them previews each. |
| Lift on a tab | That workspace opens as it was left. |
| Lift on a card | It opens as the Active card in its workspace. |
| Pull on past the cards | The card under your finger lifts to be moved. |
| Let a lifted card go on a tab | Its window moves to that workspace. |
| Let a lifted card go on `+` | A new workspace, named for the application, takes it. |
| Let a lifted card go on its own place | It opens. |
| Push back up to the edge and lift | Table closes and nothing changes; a lifted card goes back and the tabs stay. |
| Flick down quickly | The tabs stay open as a menu bar, and nothing is chosen. |
| In the menu bar: tap a tab, then tap it again | The first tap previews it; the second enters it. |
| In the menu bar: tap a card, or the preview | What it shows opens. |
| In the menu bar: drag a card | It is carried like a lifted card. |
| In the menu bar: hold a tab | Its name can be typed, as a right-click does. |
| Tap or lift on `+` | An empty workspace opens; its first application names it. |
| Clear a workspace's name | Table offers to remove it (`TABLE.md`). |
| `Meta+W` | Table opens as a menu bar on the display under the pointer, or closes. |
| Push the pointer into a display's top-left corner | The menu bar opens there. The rest of the top edge stays with windows. |
| Hover over a tab or a card | It previews. |
| Click a tab or a card | It opens, a card as the Active card. |
| Wheel | Steps across the tabs, or across the chosen tab's cards. |
| `Left`, `Right` | The tab or card before or after is chosen. |
| `Down`, `Up` | Into the chosen tab's cards, or back up to the tabs. |
| `Enter` or `Space` | The chosen tab or card opens, a card as the Active card. |
| `Escape` | Table closes and nothing changes; a name being typed keeps the old one. |
| `F2` | The chosen workspace's name can be typed; `Enter` keeps it. |
| Three fingers up on the touchscreen, or four on a touchpad | Table comes down as a menu bar once the swipe passes halfway, on the card display, or under the pointer from a touchpad; again closes it. |

## Plasma desktop

| Input | What happens |
| --- | --- |
| Switch Kadunce off in the tray | Every window returns to the ordinary Plasma desktop, then Kadunce unloads. |
| `Meta+Esc` | Every window everywhere returns to the plain Plasma desktop. The next window on the card display starts cards again. |
| Drag the Active card or a Bento pane to the bottom edge | It returns to the ordinary Plasma desktop where you let it go; other cards wait in Spread. |
| Switch Kadunce on in the tray | The card display's windows become cards, the one in use the Active card. |
