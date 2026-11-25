# Input

Every gesture and key Kadunce handles, and what happens. Anything else goes to the application. A gesture Kadunce refuses
springs back with a note why. `CARD-LIFECYCLE.md` says what each
result does to a window.

| Task | Touch | Keyboard |
| --- | --- | --- |
| Spread | Swipe up from the bottom edge | `Meta+S` |
| Active card | Tap a card in Spread | `Enter` in Spread |
| Bento | Drag a window to the left or right edge | `Meta+B` |
| Table | Pull down from the top edge | `Meta+W` |
| Plasma desktop | Switch Cards off in the tray | `Meta+Esc` |

Kadunce's keys sit on `Meta`, resting while a shortcut is recorded. `Ctrl` stays with
applications, `Alt+Tab` with KDE; `Meta+G` and `Meta+E` open Tettegouche's
Apps and Files.

## Spread

| Input | What happens |
| --- | --- |
| Swipe up from the bottom edge | Spread follows your finger; a quick flick opens it fully. |
| Three fingers down on the touchscreen, or four on a touchpad | Spread opens under your fingers. |
| `Meta+S` | Spread opens, or closes back to the Active card. A Bento layout shrinks into the row as its panes' cards. |
| Drag the row sideways | The row follows, coasts after a flick and springs back at either end. |
| Wheel over the row | The row moves one card. |
| `Left`, `Right`, `Up`, `Down`, `Meta+Left`, `Meta+Right` in Spread | The row, or the centred Stack, moves one card. |
| Tap or click empty space, or `Escape` | Back to where Spread opened. |
| Type | Search opens holding what you typed. |
| Flick a card up | Its application closes, or one that asks first comes forward. A short lift springs back. |
| Pull down on a pane's card | That pane leaves its layout as its own card. |
| Hold a card | It rises to be carried; Spread stays three across. Held off the middle, the row slides under it. |
| Pull a held card down | The row zooms out to one set view until pushed up. |
| Let a held card go in a gap | It lands there as the row regrows to three across. |
| Let a held card go on another card, once it has risen | They become a Stack; on a pane's card, it fills that pane. |
| Let a held card go on another display | It becomes that display's card, or joins its layout. |
| Tap or click a card's Gooseberry note, dot or stack | Its card opens, a tapped note ready to type; hold a note, or the stack, and let go on another card to stick it. |
| Tap or click outside Search and the on-screen keys | Search closes; a card tapped beside it opens. A stroke leaves it open; the keys type into it. |
| Plasma's bottom or top touch edge, where no touchscreen drives a display | The bottom opens Spread; the top closes it. Three fingers up bring Table. |

A swipe begun on the dock stays with it; while the on-screen keys are up, the
bottom edge is theirs.

### Stacks

| Input | What happens |
| --- | --- |
| A slow sideways stroke begun on the centred Stack | Its cards come to the front in turn. A quick one moves the row. |
| Wheel over the centred Stack | Steps through its cards. |
| Tap or click a card fanned behind the centred Stack's front | It comes to the front; Spread stays open. |
| Pull a Stack's card down, or `Meta+Shift+Down` on the centred or Active one | It leaves the Stack as a card after it; the Stack keeps its place and order. |
| Hold a Stack's card | It lifts out and the Stack parts at an outlined place; sideways moves it, left toward back. Let go, it goes there. Held still longer, the whole Stack gathers and carries. |

## Active card

| Input | What happens |
| --- | --- |
| Tap or click a card in Spread | It grows into the Active card; a pane's card opens its layout there. |
| `Enter` in Spread | The centred card opens. |
| Let a held card go at the top edge | It becomes the Active card. |
| Drag a window by its title bar to the top edge | It becomes the Active card, a pane leaving its layout; the first time, every window there becomes a card. |
| Drag a window onto a card display | It becomes its card. |
| Swipe in from a touchscreen's side edge | Sends that edge's key from Settings › Shortcuts › Edge swipes; with none, the window keeps it. |
| Tap, click or swipe inward in the gap beside the Active card, or another display's lone card | The card before (left) or after (right) slides in, round a Stack's ring. The pointer: a page tab. |
| Wheel in the gap beside the Active card | Up: the card before; down: the one after. |
| `Meta+Left`, `Meta+Right` | The card before or after opens, round a Stack. |
| `Meta+Tab`, `Meta+Shift+Tab` | The next or previous card of the Stack. |
| `Meta+Ctrl+Shift+Left`, `Right` | It and its Stack go to the workspace beside. |
| Keyboard in Control Center or the tray, or a tap in a terminal | The keys rise for that window; the Active card makes room. After typing on an attached keyboard, taps bring none until it goes. |
| Point in any other gutter | An arrow; nothing resizes there. |

## Bento

| Input | What happens |
| --- | --- |
| Drag a window by its title bar to the left or right edge of a display | Within 96 px on a monitor, it pairs on your side with the Active card, or for the Active card the one used before, the upper half larger; with none, it becomes the Active card. Connected: `CARD-LIFECYCLE.md` §11. |
| `Meta+B` | Starts or ends Bento on the display in use. Connected, every card takes a pane, an Active card before a layout too, Spread closing first. |
| `Meta+Shift+B` | Displays kept as windows switch between filling themselves and your `Meta+T` zones. |
| Drag a window to an edge of a display kept as windows | Alone it takes half at a side, or the Active card's size on top; with more, all join one layout. |
| Hold or click a divider, then drag | Both panes resize, rows per column; where lines meet, both ways. |
| Drag a Bento pane onto another pane of its layout | They swap. Connected, held until it leans in, it joins that pane's Stack. |
| Minimize a pane | It leaves the layout as a sleeping card; a one-pane layout ends. |
| `Meta+Ctrl+Shift+Left`, `Right` | The pane alone goes to that workspace. |
| `Escape` or another finger while dragging | The drag cancels. |

## Connected

On a display sized Connected (`CARD-LIFECYCLE.md` §11):

| Input | What happens |
| --- | --- |
| Tap a card, or `Enter` | It joins the layout beside the card in use; full, the pane used longest ago leaves. A pane opens the layout. |
| `Meta`'s arrows on a pane | The focus goes that way, past the edge to that display. |
| `Meta+Shift+Left`, `Right` on a pane or lone card | It swaps with the pane there, or takes the room, or goes to that display. |
| `Meta+[`, `Meta+]` | The pane's divider moves. |
| `Meta+Tab`, `Meta+Shift+Tab` on a pane, or a touchscreen's bottom swipe | Its Stack steps in place. |
| `Meta+Shift+Up` on a pane | Its card goes one deeper, placed on releasing `Meta`; with no Stack, it joins the pane used before. |
| `Meta+Shift+Down` on a pane or lone card | It leaves its Stack for its own pane; else waits in Spread. |
| Hold a pane's title bar, then drag up or turn the wheel | Its Stack's title bars show; its card passes them and goes in where let go. |
| Wheel while dragging on a monitor | It resizes in place, its app keeping that width; whole, it fills. Slow turns pause at common widths. |
| Drag from another display onto a pane | Rest in middle: Stack; top, bottom: column; side: new column. |
| Click a picture in room a side snap left | It fills that room; `Escape` declines. |

## Table

| Input | What happens |
| --- | --- |
| Pull down from the top edge: above the Active card, else anywhere along it | Table comes down, a tab per workspace; sliding across previews each. Panels keep their taps. |
| Pull past the tabs, slide sideways | That tab's cards hang, each previewed. |
| Lift on a tab or card | It opens: a workspace as left, a card as Active. |
| Pull on past the cards | The card under your finger lifts. |
| Let a lifted card go on a tab | Its window moves to that workspace. |
| Let a lifted card go on `+` | A new workspace takes it. |
| Let a lifted card go on its own place | It opens. |
| Rest on a Stack | Its cards hang, front one dotted; lift one to open it, or pull it out. |
| Hold a lifted card on another card | They stack, it in front, in any workspace. |
| Push back to the edge and lift | Table closes, unchanged. |
| Flick down | The tabs stay as a menu bar. |
| In the menu bar: tap a tab twice, a card or the preview once | It enters, or opens. |
| In the menu bar: drag a card | It is carried like a lifted card. |
| Tap or lift on `+` | Empty workspace and saved Layouts hang under it; with none, New joins the tabs. |
| Clear a workspace's name | Table offers to remove it (`TABLE.md`). |
| `Meta+W` | Table opens as a menu bar on the pointer's display, or closes. |
| Point into a display's top-left corner | The menu bar opens there; the rest of the edge stays the windows'. |
| Hover or click a tab or card | It previews, or opens. |
| Wheel | Steps across tabs or the chosen tab's cards. |
| Arrow keys | The tab or card beside is chosen; down enters a tab's cards or a Stack's, up returns; `Meta+Shift+Down` takes one out. |
| `Enter` or `Space` | The chosen tab or card opens. |
| `Escape` | Table closes; a carried card or typed name goes back. |
| `F2`, or in the menu bar hold or right-click a tab | Its name can be typed; `Enter` keeps it. |
| `Shift`+arrows on a chosen card, then let go | It rides the tabs and `+`, moving where let go. |
| Three fingers up on a touchscreen, four on a touchpad | Past halfway, the menu bar comes down there; again closes it. |

## Plasma desktop

| Input | What happens |
| --- | --- |
| Switch Cards off in the tray | Every window returns to the Plasma desktop, then Kadunce unloads. |
| `Meta+Esc` | Every window returns to the Plasma desktop until a new window starts cards. |
| Drag the Active card or a pane to the bottom edge | It returns to the Plasma desktop where let go; other cards wait in Spread. |
| Switch Cards on in the tray | Every display's windows become cards, the one in use Active. |
