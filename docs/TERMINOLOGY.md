# Shuffle terminology

Shuffle is the suite these three repositories form, and this is its suite-wide
language contract. Terms are approved by J; this document records them and the
rules for applying them. It covers Kadunce, Tettegouche and Temperance.

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
| Precision surface | Full keyboard footprint acting as pointer and scroll input, entered from the space bar | Locked concept |
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

Current identity is inconsistent and is not shared across the suite:

- `studio.warbler.*` — Kadunce, Temperance and the Tettegouche plugin
- `io.github.carlsonjm.*` — the Tettegouche desktop entry
- `studio.warbler.Kadunce.showCardLine` — a `Q_SCRIPTABLE` method carrying a
  retired term on the public surface
- Tettegouche hard-codes `studio.warbler.Kadunce` in seven call sites

Unifying these under one namespace is a single coordinated change across all
three repositories, with a protocol version bump and a documented migration. Temperance already demonstrated the cost when its package identity
changed in 1.1.0 and existing panel widgets had to be removed and re-added.

## Enforcement

`tettegouche/tests/verify-source.sh` already fails the build when retired
identity appears in the source tree. That mechanism is the model: a terminology
regression should break a check, not wait for a reader to notice.

- [x] Extend the guard to Kadunce.
- [ ] Extend the guard to Temperance.
- [x] Add retired product vocabulary to the pattern once the rename lands, so
  the retired term cannot return.
- [x] Exclude `docs/archive/` and the guard file itself from the pattern.

Kadunce's guard permits three layer-3 spellings and nothing else: the
`showCardLine` scriptable method, the `cardLine` workspace-context value, and
the persisted `Kadunce Card Line` global-shortcut identity. The first two are
consumed by Tettegouche; the third is stored in the user's shortcut
configuration, so renaming it discards a configured binding. Block 10b retires
all three together with a documented migration.
