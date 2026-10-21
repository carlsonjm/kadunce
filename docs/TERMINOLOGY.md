# Terminology

The words Kadunce uses for what it shows, so documents, on-screen text and
code comments say the same thing.

## Terms

| Term | Meaning |
| --- | --- |
| Card | An application window Kadunce holds on a display |
| Active card | The card in use: alone on a display sized Solo, the pane in use on one sized Connected (§ Naming rules) |
| Spread | The ordered row of cards and Stacks you browse; each Bento pane is a card in it |
| Stack | Cards you grouped by hand into one Spread entry; stepping through it goes round it as a ring |
| Sleeping card | A minimized card, drawn dimmed in Spread |
| Bento | Two or more windows shown together in one layout |
| Bento layout, pane, divider | One display's layout, each window in it, and the boundary you drag between panes |
| Primary display | The display that takes an unplugged display's cards and answers keys when no window is in use: the one you chose, else the touchscreen, else the largest |
| Plasma desktop | The ordinary KDE desktop outside cards |
| Workspace | One KDE virtual desktop as Table shows it: its cards, layouts and windows, on every display |
| Named workspace | A workspace you named; it stays when empty |
| Table | Every workspace as tabs pulled down from the top edge, the chosen one's cards hanging below; it moves cards between workspaces |
| Tab, menu bar, `+` | A workspace's pill in Table; Table left open by a flick, `Meta+W` or the corner; the tab that makes a new workspace |
| Cards | The name Kadunce shows in the system tray, and its on and off switch; Shuffle uses the same name |
| Search | Tettegouche's launcher, which can sit in the middle of Spread |
| Stuck notes | Gooseberry's notes attached to a window, which Spread draws on that window's card |

## Naming rules

- **What people read** (the README, on-screen words, application entries) uses
  the terms above exactly.
- **Code names** can stay. Names nobody sees, such as
  `DesktopStageController` or `CardStageController`, need not match.
- **Installed identifiers** (reverse-DNS ids, D-Bus names, plugin and desktop
  entry ids, scriptable method names) change only in a coordinated, versioned
  release, because changing one breaks installed copies and the programs that
  call them. Kadunce's are under `co.goodinput.Kadunce`.
- **Display sizing** is an internal term. A display is sized **Solo**, one card
  at a time over the whole work area and a Bento pair at most, or
  **Connected**, room for more than two panes, where a picked card joins the
  layout (`CARD-LIFECYCLE.md` §11). Solo, Connected and "display sizing" stay
  out of on-screen words; there a display simply shows one card or a layout.

## Enforcement

`tests/verify-source.sh` and `tests/verify-docs.py` fail when a retired word
comes back, including Spread's earlier name and Solo's earlier name. Two
installed spellings of Spread's earlier name remain allowed: the `showCardLine`
scriptable method and the `cardLine` value in the workspace context, both read
by Tettegouche (`TETTEGOUCHE-CONTEXT.md`). A later versioned release renames
them together.
