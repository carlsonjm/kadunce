# Shuffle terminology

Shuffle is the suite these three repositories form, and this is its suite-wide
language contract. Terms are approved by the maintainer; this document records
them and the rules for applying them. It covers Kadunce, Tettegouche and
Temperance.

## Approved language

| Term | Meaning | Status |
| --- | --- | --- |
| Shuffle for Plasma | Public product descriptor | Locked |
| Workspace | Consumer-facing Kadunce system; the spatial working environment | Locked |
| Card | A normal application window managed spatially by Workspace | Locked |
| Active | The Card currently being used | Locked |
| Card Spread / Spread | The surrounding ordered field of Cards used for browsing. Card Spread is the full name of the action; Spread is the short form and both are approved | Locked |
| Stack | Related individual Cards grouped for sequential paging | Locked |
| Bento | Multiple simultaneously visible Cards composed into one layout | Locked |
| Bento group / grouped Card | Bento's single representation when viewed in Spread | Working |
| Table | Spatial level above Workspace presentation, organizing real KDE virtual desktops | Locked for 1.0 |
| Search | Consumer-facing Tettegouche launcher and search experience | Locked |
| Browse everything | Alphabetical application catalogue inside Search | Current |
| Explore files / Files | Tettegouche's integrated file-management experience | Current |
| Ambient | Ongoing context and activity: media, transfers, jobs | Locked |
| Status Bar | Consumer-facing Temperance system and status surface | Locked |
| Control Center | Temperance quick system controls | Current |
| System Tray | Temperance's organized presentation of Plasma tray entries | Current |
| Shuffle Keyboard | Touch keyboard, editing surface and precision input | Locked for 1.0 |
| Scrub column | Near-invisible vertical control on each side of the Keyboard: history left, key height right | Working |
| Precision surface | Full keyboard footprint acting as pointer and scroll input, entered from the latch at the space bar's end | Locked concept |
| Shuffle Lock | Privacy-first presentation over trusted system lock and authentication | Locked for 1.0 |
| Bottom Surface | Single layout authority for Status Bar, Shuffle Dock, Ambient and the Keyboard boundary | Working |
| Shuffle Dock | Minimal task and application presentation inside Bottom Surface | Working |

## Retired language

Never appears in live source, live documentation or user-visible text.

| Retired | Replacement |
| --- | --- |
| Card Line | Spread |
| WebOS, Project WebOS, Palm, ChromeOS | no replacement; unrelated products |
| Itasca (as a product or repository name) | Shuffle; Itasca remains only the visual-language name |

## Three layers, three rules

Terminology applies differently by layer. Conflating them causes unnecessary
breaking changes.

### 1. Consumer language

Everything a user reads: READMEs, UI strings, descriptions, application entries,
support material, the public site. Must use the approved terms exactly. Retired
terms are defects here.

### 2. Component and internal names

Kadunce, Tettegouche and Temperance remain the open-source component and
repository names. Internal symbols may use component vocabulary that never
reaches a user, such as `CardStageController` or `DesktopStageController`.

Internal symbols must not use *retired* vocabulary. `CardLineModel` becomes
`SpreadModel`; `CardStageController` may stay, because Card Stage is internal
architecture rather than a retired product term.

### 3. Package and interface identity

Reverse-DNS identifiers, D-Bus service and interface names, plugin ids, desktop
entry ids, and any scriptable method name. Changing these breaks installed
packages and cross-component calls, so they change only in a coordinated,
versioned release, never as part of a vocabulary pass.

Current identity is not shared across the suite: `studio.warbler.*` names
Kadunce, Temperance and the Tettegouche plugin, and `io.github.carlsonjm.*` the
Tettegouche desktop entry. Block 10b unifies them in one coordinated change
across all three repositories.

## Enforcement

A terminology regression breaks a check rather than waiting for a reader to
notice. Kadunce's `tests/verify-source.sh` and `tests/verify-docs.py` fail when
retired language returns, the guard file itself exempt, and
Tettegouche's `tests/verify-source.sh` fails on retired identity in its source.
Temperance has no guard yet (Block 10b).

Kadunce's guard permits two layer-3 spellings and nothing else: the
`showCardLine` scriptable method and the `cardLine` workspace-context value,
both consumed by Tettegouche. Block 10b retires them together with a
documented migration. A third, the `Kadunce Card Line` global-shortcut
identity, went with Kadunce's `Ctrl` keys on 28 September.
