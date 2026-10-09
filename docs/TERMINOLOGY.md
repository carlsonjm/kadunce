# Shuffle terminology

The suite's language contract: the terms every component uses and the rules for
applying them. It covers Kadunce, Tettegouche, Temperance, Gooseberry,
Shuffle, the Shuffle Keyboard and Split Rock.

## Approved language

| Term | Meaning | Status |
| --- | --- | --- |
| Shuffle for Plasma | Public product descriptor | Locked |
| Workspace | One KDE virtual desktop as Table shows it: its cards, layouts and windows, across every display | Current |
| Named workspace | A workspace you named; it stays when empty | Current |
| Cards | Kadunce's name on a Shuffle install, and its tray switch | Current |
| Card | An application window Kadunce holds on the card display | Locked |
| Active card | The card in use, shown alone | Locked |
| Spread | The ordered row of cards, Stacks and the Bento group you browse | Locked |
| Stack | Cards grouped by hand into one Spread entry; stepping through it goes round it as a ring | Locked |
| Bento | Two or more windows shown together in one layout | Locked |
| Bento layout, pane, divider | One display's layout, each window in it, and the boundary you drag | Current |
| Bento group | A Bento layout as one card in Spread | Current |
| Sleeping card | A minimized card, drawn dimmed in Spread | Current |
| Card display | The display a touchscreen drives; the only one that holds cards | Current |
| Plasma desktop | The ordinary KDE desktop outside cards | Current |
| Table | Every workspace as tabs pulled down from the top edge, the chosen one's cards hanging below; it moves cards between workspaces | Locked |
| Tab, menu bar, `+` | A workspace's pill in Table; Table left open by a flick, `Meta+W` or the corner; the tab that makes a workspace | Current |
| Search | Tettegouche's launcher and search, opened from the dot | Locked |
| Apps | Search's drawer of applications, A to Z | Current |
| Files | Search's file manager, which opens every folder | Current |
| Ambient | What is under way or waiting on you now: media, transfers, jobs, an application's question, a shared screen | Locked |
| Island | One kind of Ambient activity in the panel; what does not fit folds into a counted bubble | Current |
| Status Bar | Consumer-facing Temperance system and status surface | Locked |
| Ticker | The Status Bar's line of what just happened | Current |
| Notifications & Events | The history the bell opens | Current |
| Control Center | Temperance quick system controls | Current |
| System Tray | Temperance's organized presentation of Plasma tray entries | Current |
| Shuffle Keyboard | Touch keyboard, editing surface and precision input | Locked |
| Hide key | The key that puts the keys away | Current |
| Precision surface | The whole keyboard latched as a trackpad, from the trackpad mark at the space bar's end | Locked |
| Shuffle Lock | Privacy-first presentation over trusted system lock and authentication | Locked |
| Bottom Surface | Single layout authority for Status Bar, Shuffle Dock, Ambient and the Keyboard boundary | Current |
| Shuffle Dock | Minimal task and application presentation inside Bottom Surface | Current |
| Notes | Gooseberry's name on a Shuffle install | Current |
| Split Rock | The desktop assistant; inside Shuffle, Genie | Working |

## Retired language

Never appears in live source, live documentation or user-visible text.

| Retired | Replacement |
| --- | --- |
| Card Line | Spread |
| Card Spread | Spread |
| Browse everything | Apps |
| Explore files, Tette Files | Files |
| Pinned workspace | Named workspace |
| Scrub column | Hide key; height and history moved to the dock's handle |
| WebOS, Project WebOS, Palm, ChromeOS | no replacement; unrelated products |
| Itasca (as a product or repository name) | Shuffle; Itasca remains only the visual-language name |

`docs/archive/` is exempt. Archived evidence preserves the language of its own
candidate, and rewriting it would destroy provenance.

## Three layers, three rules

Terminology applies differently by layer. Conflating them causes unnecessary
breaking changes.

### 1. Consumer language

Everything a user reads: READMEs, UI strings, descriptions, application entries,
support material, the public site. Must use the approved terms exactly. Retired
terms are defects here.

### 2. Component and internal names

Kadunce, Tettegouche, Temperance, Gooseberry, the Shuffle Keyboard and Split
Rock remain the component and repository names, and outside Shuffle the
open-source components keep their own names. Internal symbols may use
component vocabulary that never reaches a user, such as `CardStageController`
or `DesktopStageController`.

Internal symbols must not use *retired* vocabulary. `CardLineModel` becomes
`SpreadModel`; `CardStageController` may stay, because Card Stage is internal
architecture rather than a retired product term.

### 3. Package and interface identity

Reverse-DNS identifiers, D-Bus service and interface names, plugin ids, desktop
entry ids, and any scriptable method name. Changing these breaks installed
packages and cross-component calls, so they change only in a coordinated,
versioned release, never as part of a vocabulary pass.

Current identity is mostly shared across the suite: `co.goodinput.*` names
Kadunce, Temperance and the Tettegouche plugin, and `io.github.carlsonjm.*`
still names Tettegouche's desktop entries and Gooseberry. One coordinated
change unifies them (`ROADMAP.md`).

## Enforcement

A terminology regression breaks a check rather than waiting for a reader to
notice. Kadunce's `tests/verify-source.sh` and `tests/verify-docs.py` fail when
retired language returns, `docs/archive/` and the guard file itself exempt, and
Tettegouche's `tests/verify-source.sh` fails on retired identity in its source.
Temperance has no guard yet.

Kadunce's guard permits two layer-3 spellings and nothing else: the
`showCardLine` scriptable method and the `cardLine` workspace-context value,
both consumed by Tettegouche. The `co.goodinput` rename left them in place; a
later versioned migration retires them together (`ROADMAP.md`).
