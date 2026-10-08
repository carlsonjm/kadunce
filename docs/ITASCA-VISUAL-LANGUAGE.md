# Itasca visual language

**Status:** Accepted suite reference. This defines shared visual grammar without
replacing component-specific interaction contracts.

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

- Corners come in three tiers. Paper takes 8 px: windows and cards, the sheets
  that take a card's place, and everything laid in any surface, such as rows,
  tiles, highlights and cards in a list. Notes that float above everything and
  close take 12 px: pop-ups, menus, alerts and the dock sheet. Anything whose
  job is to be pressed is a pill.
- Nothing but a pill is rounder than the surface that holds it.
- Keyboard keys are paper, since round keys read as dots; Table's cards keep
  their own corners.
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
  normally remain at least 40–44 px. Icons set closer so they read as one group,
  as Temperance's status icons are 34 px apart, answer the panel's full height
  instead. Test a control by pointer as well as by touch.
- Responsive layouts remove optional information before crushing useful spacing.
- If content fits naturally, show it. Do not leave usable space empty while
  eliding or hiding information.

## Color roles

These semantic values apply to suite-owned presentation.

| Role | Current baseline | Purpose |
|---|---|---|
| Surface | `#141414` | Primary dark popup/card surface |
| App chrome | `#1C1C1C` | Applications' title bars, toolbars and sidebars, one step above content |
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
| Accent | Ghost White, or your Plasma accent | Active state, progress, high-value signal |
| Dark accent foreground | `#102729` | Content on a bright accent fill |
| Error | `#FFB5A8` | Actionable failure copy |

- Accent communicates state; it is not general decoration or the default hover.
- Prefer opacity steps of soft white over unrelated grays on translucent surfaces.
- Keep primary text and glyphs near-white; use opacity for supporting context.
- Application artwork and source icons may retain native color. Suite chrome stays
  monochrome unless state requires accent or error color.

### Terminal palette

For a terminal that wants the suite's look; Shuffle's install leaves the
person's own terminal alone. Each color comes from a role above.

| Slot | Normal | Bright | Role |
|---|---|---|---|
| Black | `#141414` | `#5A5A5A` | Surface; strong border |
| Red | `#FF8A7A` | `#FFB5A8` | Warm red; error |
| Green | `#00D3B8` | `#5CEBD4` | The suite's teal, success |
| Yellow | `#F2A65A` | `#FFC078` | Amber, warning |
| Blue | `#7AA7FF` | `#A5C3FF` | A soft blue that reads on black |
| Magenta | `#C49BFF` | `#DCC2FF` | Lavender |
| Cyan | `#5ED8F0` | `#9BE8F7` | Cyan |
| White | `#AFAFAF` | `#F8F8FF` | Secondary text; Ghost White |

The background is `#141414` and solid; text and cursor are Ghost White, with
`#141414` under the cursor; a selection is 24% white, `#4C4C4C`, under Ghost
White. Programs print through these slots, not fixed colors, so they follow the
terminal's palette.

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

## Motion and animation

Motion explains state, causality, spatial continuity, or completion. It should not
decorate an idle interface or delay an action the system already understands.

### Timing tiers

| Tier | Duration | Use |
|---|---:|---|
| Immediate feedback | 80–120 ms | Press response, tiny color/opacity acknowledgement |
| Micro transition | 120–160 ms | Hover, focus, toggle fill, icon state change |
| State transition | 160–200 ms | Reveal/hide, popup opacity, compact content replacement |
| Spatial transition | 220–280 ms | Drawer, card, sheet, responsive geometry, mode continuity |
| Confirmation hold | 700–900 ms | Brief authoritative success/check state before removal |

Durations describe perceived motion. Use platform-scaled/Kirigami durations where
they preserve these relationships and respect the user's animation settings.

### Easing

- Use ease-out cubic for entrances and direct manipulation settling.
- Use ease-in-out cubic for reversible geometry or state morphs.
- Exits may be slightly faster than entrances; they must not feel abrupt.
- Linear motion is reserved for continuous progress or a genuinely constant-rate
  indicator.
- Springs are reserved for physical or spatial continuity, such as an arriving
  floating surface. Avoid springing routine hover, text, or status changes.

### Choreography

- Animate the container or relationship before individually animating its details.
- Keep related controls together during motion; never scatter a cluster to fill
  changing width.
- Responsive disclosure should fade/settle optional information without moving the
  stable actionable core unnecessarily.
- Small sequencing delays may clarify order, but repeated stagger should stay
  subtle and normally below 60 ms.
- A source-authoritative completed activity may become a check for 700–900 ms,
  then leave. Filesystem quiet time alone cannot claim success.
- Progress changes should remain legible and stable; do not bounce or overshoot a
  factual value.

### Interruption and ownership

- Every transition must be safe to interrupt, reverse, or retarget from its current
  visible state.
- User input wins immediately over decorative or settling animation.
- The owner of the state owns completion. Presentation may interpolate known state
  but must not invent progress, success, or failure.
- Source loss removes stale activity without playing a false success animation.
- Repeated events update an existing surface when identity is stable instead of
  replaying the full entrance.

### Reduced motion

- Follow the platform animation scale and accessibility preference where exposed.
- Motion that carries on from the hand (a flicked row, a row parting under a
  carried card, a card springing back or thrown away) keeps the hand's pace at
  every speed, and at instant lands where it was going on the next frame.
- Under reduced motion, preserve state communication with short opacity/color
  changes and final geometry; remove travel, overshoot, and stagger.
- Never make animation the only indication of state.

## Kadunce geometry

- Kadunce cuts every application window and dialog to the 8 px paper tier,
  with no outline, whether it is a card, a pane or loose on the desktop, and
  whether it has a title bar or draws its own. A window that casts its own
  shadow, or is maximized or full screen, keeps its corners, and Kadunce trims
  nothing while the separately installed Rounded Corners effect runs.
- The Active card's gutter is output-relative, 10 px by default and bounded to
  6–48 px, the same on every edge.
- Spread with two entries draws its centre card at 64% of the work area and the
  shoulders at 54%; with three or more, every card is 54%.
- A search launcher hosted in Spread arrives over a 220 ms settle that changes
  no final geometry. Dragging previews only the first 18% of its travel: the
  shoulder lifts 16 px and leans by less than a degree, then falls flat into the
  centre while the opposite neighbour stays still.
- A card's stuck notes in Spread show as one 64 by 44 px mini note, 12 px
  inside the card's bottom-right corner: the top note in its colour with a 6 px
  corner, a soft shadow and its first words in two lines of 9 px demibold. When
  there are several, the next note's edge peeks 4 px above it a shade deeper,
  and the count sits in 8 px at 65% in the mini note's bottom-right corner.
  Fanned, each note is 136 by 88 px. Both keep their size on every card.

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
- Use **lowercase** for the time's day period: `9:41 a.m.`.
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
- Keep KDE Breeze/provider lookup for application, file, weather, and other
  externally owned identity.
- Use identical semantic names across repositories even when each repository
  vendors only its required subset.

Temperance's accepted bell uses Lucide Bell geometry, redrawn locally for its
animated clapper and slash. It is the first documented suite-specific exception.

### Protected custom icon work

These established visuals are product identity or behavior, not migration targets:

- Tette Dot / launcher identity.
- Temperance's animated Bell and custom Weather glyph.
- Kadunce's stacked-card tray mark.

Lucide must not replace, redraw, simplify, or absorb these components. Preserve
their geometry and behavior unless a focused product redesign changes the contract.

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
11. Can every animation be interrupted or retargeted without jumping or lying?

## Durable decisions

- Lucide is the canonical suite action family.
- `#F8F8FF` is named **Ghost White** and remains the primary foreground.
- The animated Temperance bell remains a documented Lucide-derived exception.
- Tette Dot, Bell, Weather, and Kadunce's stacked-card tray mark remain protected.
- Corners come in three tiers: 8 px paper, 12 px notes, pills for actions.
- Each repository owns its pinned Lucide subset; there is no runtime cross-repo
  asset dependency.

Soft primary (`#F2FFFFFF`) remains a separate foreground level. App-specific outer
silhouettes may differ when they encode product identity or interaction.
