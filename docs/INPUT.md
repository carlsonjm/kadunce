# Input

Every gesture, tap, click, wheel and key Kadunce answers, and what you see
happen; anything else stays with the application. A gesture Kadunce refuses
springs back with a note saying why. `CARD-LIFECYCLE.md` says what each
result does to a window.

| Task | Touch | Keyboard |
| --- | --- | --- |
| Spread | Swipe up from the bottom edge | `Meta+S` |
| Active card | Tap a card in Spread | `Enter` in Spread |
| Bento | Drag a window to the left or right edge | `Meta+B` |
| Table | Pull down from the top edge | `Meta+W` |
| Plasma desktop | Switch Cards off in the tray | `Meta+Esc` |

Kadunce's keys sit on `Meta` and work only while it runs. `Ctrl` stays with
applications, `Alt+Tab` with KDE, and `Meta+G` and `Meta+E` open Tettegouche's
Apps and Files.

## Spread

| Input | What happens |
| --- | --- |
| Swipe up from the bottom edge | Spread follows your finger; a quick flick opens it fully. |
| Three fingers down on the touchscreen, or four on a touchpad | Spread opens past halfway, the Active card shrinking into the row under your fingers. |
| `Meta+S` | Spread opens, or closes back to the Active card. A Bento layout shrinks into the row as its group card. |
| Drag the row sideways, by finger or mouse | The row follows, coasts after a flick and springs back at either end. |
| Wheel over the row | The row moves one card. |
| `Left`, `Right`, `Up`, `Down` in Spread | The row, or the centred Stack, moves one card. |
| `Meta+Left`, `Meta+Right` in Spread | The row moves to the card before or after. |
| Tap or click empty space, or press `Escape` | You go back to the card last open, or the layout you opened Spread from. |
| Flick a card up | Its application closes, or one that asks first comes forward. A short lift springs back. Never the Bento group. |
| Pull down on the Bento group | The pane under your finger leaves the layout as a card. |
| Hold a card | It rises to be carried; Spread stays three across. Held off the middle, the row slides under it. |
| Pull a held card down | The row zooms out to one set view until pushed back up; not over the Bento group. |
| Let a held card go in a gap | It lands there as the row grows back to three across. |
| Let a held card go on another card, once that card has risen | The two become a Stack. On the Bento group it takes the pane under your finger and the layout opens. |
| Let a held card go on another display | It becomes that display's card, or joins its layout. |
| Tap or click a card's Gooseberry note or stack | They come up until tapped again, the stack fanned; hold one, or the stack for its top note, and let go on another card to stick it. |
| Right-click | Nothing: Spread has no window menu. |
| Tap or click outside Search and the on-screen keys | Search closes; a card tapped beside it opens. A stroke leaves it open; the keys type into it. |
| Plasma's bottom or top touch edge, where no touchscreen drives a display | The bottom opens Spread; the top closes it. Three fingers up bring Table. |

A swipe that starts on the dock stays with the dock, and while the on-screen keys
are up, the bottom edge is theirs.

### Stacks

| Input | What happens |
| --- | --- |
| A slow sideways stroke begun on the centred Stack | Its cards come to the front in turn. A quick stroke moves the row instead. |
| Wheel over the centred Stack | Steps through its cards. |
| Tap or click a card fanned behind the centred Stack's front | It comes to the front, and Spread stays open. |
| Pull a Stack's card down, or `Meta+Shift+Down` on the centred one | It leaves the Stack and becomes a card just after it; the Stack keeps its place and order. |
| Hold a Stack's card | It lifts out and the Stack parts at an outlined place; sideways moves it, left toward the back. Let go and the card goes there, the front card showing; where it began, nothing changes. |

## Active card

| Input | What happens |
| --- | --- |
| Tap or click a card in Spread | It grows into the Active card; the Bento group opens as its layout. |
| `Enter` in Spread | The centred card opens. |
| Let a held card go at the top edge | It becomes the Active card. |
| Drag a window by its title bar to the top edge | It becomes the Active card, a Bento pane leaving its layout. The first time, every other window on that display becomes a card too. |
| Drag a window onto the Spread display | It becomes a card. |
| Tap, click or swipe inward in the gap beside the Active card | The card before (left) or after (right) becomes Active, round a Stack's ring. The pointer: a page tab. |
| Wheel in the gap beside the Active card | Up for the card before, down for the card after. |
| `Meta+Left`, `Meta+Right` | The card before or after opens, round a Stack. `Meta+Up` and `Meta+Down` do nothing here. |
| `Meta+Tab`, `Meta+Shift+Tab` | The next or previous card of the Stack. |
| Keyboard in Control Center or the tray, or a tap in a terminal | The keys rise for that window; the Active card makes room. |
| Point in any other gutter | An arrow; nothing resizes there. |

## Bento

| Input | What happens |
| --- | --- |
| Drag a window by its title bar to the left or right edge of a display | It pairs into Bento on your side with the Active card, or for the Active card the card used before it, the upper half larger. With none, it becomes the Active card. Connected: `CARD-LIFECYCLE.md` §11. |
| `Meta+B` | Starts or ends Bento on Spread's display; on Connected every card takes a pane. |
| `Meta+Shift+B` | Monitors switch between filling themselves and your `Meta+T` zones, as the tray's switch does. |
| Drag a window to an edge of a display kept as windows (`CARD-LIFECYCLE.md` §11) | Alone it takes half the display at a side, or the Active card's size on top; with more, all join one layout, the dragged one on your side. |
| Hold a divider briefly, or press it with the mouse, then drag | Both panes resize; letting go keeps the new split. |
| Drag a Bento pane onto another pane of its layout | The two swap places. Connected, rested until two outlines show, it joins that pane's Stack. |
| Minimize a pane | It leaves the layout as a sleeping card. A layout left with one pane ends, that pane a card. |
| Press `Escape`, or add a finger, while dragging a window | The drag is cancelled. |

## Connected

On a display sized Connected (`CARD-LIFECYCLE.md` §11):

| Input | What happens |
| --- | --- |
| Tap a card, or `Enter` | It joins the layout beside the card in use; full, the pane used longest ago leaves. A pane opens the layout. |
| `Meta`'s arrows on a pane | The focus goes that way, past the edge to that display. |
| `Meta+Shift+Left`, `Right` on a pane or lone card | It swaps with the pane there, or takes the room, or goes to that display. |
| `Meta+[`, `Meta+]` | The divider beside the pane moves. |
| `Meta+Tab`, `Meta+Shift+Tab` on a pane, or a touchscreen's bottom swipe | Its Stack steps in place. |
| `Meta+Shift+Up` on a pane | Its card goes one deeper, placed when `Meta` is let go; with no Stack, it joins the pane used before. |
| `Meta+Shift+Down` on a pane | Its card leaves its Stack for a pane; with none, the layout. |
| Hold a pane's title bar, then drag up or turn the wheel | Its Stack's title bars show; its card passes them and goes in where let go. |

## Table

| Input | What happens |
| --- | --- |
| Pull down from the top edge: above the Active card, or anywhere along it elsewhere | Table comes down, a tab per workspace; sliding across previews each on every display. |
| Keep pulling past the tabs, then slide sideways | The tab under your finger stays chosen, its cards hanging below, each previewed as you slide. |
| Lift on a tab | That workspace opens as it was left. |
| Lift on a card | It opens as the Active card in its workspace. |
| Pull on past the cards | The card under your finger lifts to be moved. |
| Let a lifted card go on a tab | Its window moves to that workspace. |
| Let a lifted card go on `+` | A new workspace, named for the application, takes it. |
| Let a lifted card go on its own place | It opens. |
| Push back up to the edge and lift | Table closes; nothing changes. |
| Flick down quickly | The tabs stay open as a menu bar, nothing chosen. |
| In the menu bar: tap a tab twice | The first tap previews it; the second enters it. |
| In the menu bar: tap a card, or the preview | What it shows opens. |
| In the menu bar: drag a card | It is carried like a lifted card. |
| In the menu bar: hold a tab | Its name can be typed, as a right-click does. |
| Tap or lift on `+` | An empty workspace opens; its first application names it. |
| Clear a workspace's name | Table offers to remove it (`TABLE.md`). |
| `Meta+W` | Table opens as a menu bar on the display under the pointer, or closes. |
| Push the pointer into a display's top-left corner | The menu bar opens there; the rest of the edge stays with windows. |
| Hover over, or click, a tab or a card | It previews, or opens, a card as the Active card. |
| Wheel | Steps across the tabs, or the chosen tab's cards. |
| `Left`, `Right`, `Down`, `Up` | The tab or card beside is chosen; down enters the tab's cards, up returns. |
| `Enter` or `Space` | The chosen tab or card opens, a card as the Active card. |
| `Escape` | Table closes; a name being typed keeps the old one. |
| `F2` | The chosen workspace's name can be typed; `Enter` keeps it. |
| Three fingers up on the touchscreen, or four on a touchpad | Table comes down as a menu bar past halfway, on the Spread display or from a touchpad under the pointer; again closes it. |

## Plasma desktop

| Input | What happens |
| --- | --- |
| Switch Cards off in the tray | Every window returns to the ordinary Plasma desktop, then Kadunce unloads. |
| `Meta+Esc` | Every window returns to the Plasma desktop until a new window starts cards again. |
| Drag the Active card or a Bento pane to the bottom edge | It returns to the Plasma desktop where let go; other cards wait in Spread. |
| Switch Cards on in the tray | Every display's windows become cards, the one in use Active. |
