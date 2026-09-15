# Itasca visual language

**Status:** Accepted suite reference derived from the Kadunce, Tettegouche, and
Temperance interfaces. This defines shared visual grammar and guides future audit
and alignment work without replacing app-specific interaction contracts.

## Intent

Itasca should feel quiet, spatial, and touch-native. Shape, spacing, and contrast
should explain what an object does before copy has to explain it. Preserve the
character already visible in Tette's launcher, Temperance's rail and popups, and
Kadunce's cards rather than adopting a generic desktop theme wholesale.

## Shape means function

### Pills are controls

A pill means the user can act, choose, switch, filter, or change state.

- Radius equals half the control height.
- Related controls stay adjacent. A media transport is one compact cluster.
- Icon-only controls may be circular when their hit area is square.
- Selected controls may use a quiet fill. Hover may add a lighter fill. Focus may
  add an outline.
- A passive label must not look like a pill merely because it is short.

Examples: search, Do Not Disturb, power actions, sort choices, file actions,
previous/play/next, Cancel, and popup action buttons.

### Rounded boxes are information

A rounded rectangle means content, context, or a bounded region of information.

- Use 14 px corners for compact cards and menus, 16 px for list/result cards, and
  18 px for substantial popup surfaces and alert cards.
- Large spatial sheets may retain an accepted app-specific outer radius when
  changing it would alter the product silhouette.
- Information boxes may contain pills. The outer box remains distinct from the
  controls within it.
- Do not make informational status button-shaped unless the whole object acts.

Examples: notification cards, alert cards, file tiles, menus, transfer details,
and expanded system surfaces.

### Rounded continuity

- Align the centerlines of adjacent controls.
- Use one radius treatment for all members of a control family.
- Keep a minimum 4 px visual gap between separate pills; use 6–8 px when controls
  are not one tight transport cluster.
- A highlight follows the object's silhouette instead of introducing another
  unrelated corner radius.
- Surplus width belongs after a complete activity or group, never between children.

## Spacing hierarchy

Use a 4 px base rhythm. These are semantic roles, not an immediate migration list.

| Role | Value | Use |
|---|---:|---|
| Hairline | 2 px | Baseline correction, stacked-label gap, optical offset |
| Tight | 4 px | One control cluster, icon/badge relationship |
| Compact | 8 px | Icon-to-label, compact internal grouping |
| Standard | 12 px | Card padding, ordinary row grouping |
| Comfortable | 16 px | Larger control inset, related content groups |
| Section | 20 px | Pane columns and major local groups |
| Surface | 24 px | Popup breathing room and section separation |

- Keep spacing within a group smaller than spacing between groups.
- Preserve touch targets when glyphs are compact. Panel icon hit areas should
  normally remain at least 40–44 px.
- Responsive layouts remove optional information before crushing useful spacing.
- If content fits naturally, show it. Do not leave usable space empty while
  eliding or hiding information.

## Color roles

These consolidate values already used by Tette and Temperance. Code migration is
a later engineering block.

| Role | Current baseline | Purpose |
|---|---|---|
| Surface | `#141414` | Primary dark popup/card surface |
| Raised control | `#242424` | Resting filled control |
| Divider / quiet border | `#333333` | Low-emphasis separation |
| Strong border | `#5A5A5A` | Focusable edge or control outline |
| Primary text/glyph — Ghost White | `#F8F8FF` | Main labels and icons |
| Soft primary | `#F2FFFFFF` | Large content surfaces where primary is too bright |
| Secondary text | `#A8FFFFFF` | Supporting labels and metadata |
| Edge hint | `#88FFFFFF` | Spatial invitations and quiet edge affordances |
| Subtle fill | `7–8% white` | Resting information-card separation |
| Hover fill | `12–13% white` | Pointer hover and light emphasis |
| Selected fill | `24% white` | Current result, row, or local option |
| Accent | Plasma/user accent | Active state, progress, high-value signal |
| Dark accent foreground | `#102729` | Content on a bright accent fill |
| Error | `#FFB5A8` | Actionable failure copy |

- Accent communicates state; it is not general decoration or the default hover.
- Prefer opacity steps of soft white over unrelated grays on translucent surfaces.
- Keep primary text and glyphs near-white; use opacity for supporting context.
- Application artwork and source icons may retain native color. Suite chrome stays
  monochrome unless state requires accent or error color.

## Highlights and state

| State | Treatment |
|---|---|
| Resting control | Transparent or `#242424`, based on whether its boundary must be visible |
| Hover | 12–13% white fill, 100–150 ms ease-out |
| Pressed | Slightly stronger fill; no dramatic scale change |
| Selected/current | 24% white fill, or accent for durable active state |
| Keyboard focus | Near-white/strong-border outline following the same silhouette |
| Disabled | Reduced contrast without implying selection |
| Attention | Small accent, badge, or purposeful motion |
| Success | Brief check only when completion is authoritative |
| Error | Warm error copy/icon and direct recovery action when available |

Hover scale may reach roughly 1.06 for isolated panel icons. Do not combine scale,
strong fill, outline, and color change for one ordinary hover.

## Type hierarchy and casing

Use the system UI family. Create hierarchy with size, weight, opacity, and spacing.

| Role | Treatment |
|---|---|
| Primary content | 15–16 px, regular/medium, primary color |
| Section/card title | 15–16 px, demi-bold only when needed |
| Supporting metadata | 11–13 px, regular/medium, secondary color |
| Compact panel content | Established panel size; restrained weight |
| Large transition label | 20–24 px, demi-bold, short text only |
| Tiny state tag | 10–11 px, bold, lightly tracked; rare |

### Casing

- Use **sentence case** for headings, menus, buttons, settings, and descriptive
  labels: `Open system settings`, `Keep for review`, `New folder`.
- Use **lowercase** for quiet spatial invitations that act like environmental
  hints: `browse everything`, `explore files`.
- Use **ALL CAPS** only for a tiny established state tag such as `OPEN`; never for
  headings or ordinary actions.
- Preserve casing from people, applications, files, songs, artists, and sources.
- Use title case only for proper names. Existing Title Case controls can migrate
  to sentence case when their surface is next touched.

## Icons

**Lucide** is the canonical family for suite-owned action chrome. Repositories
vendor a pinned subset of the SVGs they consume. The selected baseline is Lucide
Static 1.46.0 under the ISC license, including its inherited Feather MIT notice.

- Use rounded, optically balanced geometry on a consistent grid.
- Use monochrome/current-color SVGs for suite chrome.
- Test optical size and stroke weight at actual panel scale.
- Keep media transport ordered previous, play/pause, next.
- Do not ship raw Unicode symbols as control icons.
- Do not mix families inside one control cluster.
- Draw custom glyphs only for genuine suite-specific gaps.
- Keep KDE Breeze/provider lookup for application, file, weather, tray, and other
  externally owned identity.
- Use identical semantic names across repositories even when each repository
  vendors only its required subset.

Temperance's accepted bell uses Lucide Bell geometry, redrawn locally for its
animated clapper and slash. It is the first documented suite-specific exception.

## Responsive composition

Width reveals information, not capability.

- Measure the real available span.
- Reserve the actionable core and the spacing that keeps it legible.
- Add context in product-priority order at natural widths.
- Use the exact remaining width before eliding.
- Remove optional information only when its useful minimum cannot fit.
- Keep activities compact and left-packed; place unused width afterward.
- Concurrent activities retain recognizable cores.

Ambient media is the reference:

`[previous · play/pause · next]  Song  Artist  runtime`

## Review checklist

1. Can shape alone distinguish controls from information?
2. Are related controls grouped and aligned?
3. Does spacing show which elements belong together?
4. Does the layout use available space before truncating content?
5. Are hover, selection, focus, and attention visibly different?
6. Does copy follow the casing rule for its role?
7. Do colors come from semantic roles rather than one-off decoration?
8. Does the result remain coherent at tablet and monitor widths?
9. Are icons from one approved family at a consistent optical size?
10. Does motion explain state or continuity rather than decorate the interface?

## Accepted decisions and remaining audit questions

- Lucide is the canonical suite action family.
- `#F8F8FF` is named **Ghost White** and remains the primary foreground.
- The animated Temperance bell remains a documented Lucide-derived exception.
- Each repository owns its pinned Lucide subset; there is no runtime cross-repo
  asset dependency.

The audit may recommend whether soft primary (`#F2FFFFFF`) should remain a
separate foreground level and whether app-specific outer silhouettes should
converge. Those are review findings, not permission for broad repainting.
