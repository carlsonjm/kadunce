# Roadmap context

The reasoning, measurements and planning controls behind `ROADMAP-CC.md`, kept
under the same block headings and in the same order. `ROADMAP-CC.md` is the
checklist and says what to do; this says why, and what a task needs to know
before it starts. Read the section for the block you are working on, not the
whole file.

A finished block keeps only what it settled and any finding open work still
needs. How finished work was found, measured and accepted is in Git and in the
snapshot of this file listed in `archive/README.md`.

When a task finishes, tick it in `ROADMAP-CC.md`. A measurement, rejected
approach or physical verdict that later work needs goes here under its block;
the rationale for a settled decision goes to `DECISIONS.md`, and current
behavior to `CURRENT_STATE.md`. Update a block for a changed invariant or scope,
the next blocked dependency, or an estimate that new evidence makes defensible.
No sprint narration, worker summaries, candidate hashes or repeated test logs.

## Product boundary

- Kadunce, Tettegouche and Temperance remain open-source component repositories.
- A downstream integration repository consumes pinned component releases and
  owns assembly, packaging and support material. Nothing in a component
  repository depends on it.
- Table and Shuffle Keyboard are required 1.0 capabilities.
- KWin, Plasma and the system input stack remain authoritative for windows,
  virtual desktops and text delivery.

## Execution policy

- J approves product behavior and visual direction before implementation begins.
  Engineering may present evidence, constraints and alternatives; an unapproved
  proposal does not become a candidate.
- One implementation owner per repository. A second worker is read-only review or
  a disjoint file set.
- Reproduce a defect and measure the property controlling it before changing it.
  A fix whose symptom cannot be reproduced is not yet a fix.
- Batch two or three related physical checks into one candidate. Freeze passed
  items; later candidates touch only failures.
- Separate feasibility, minimal prototype and product implementation.
- Keep rejected candidates out of `main`; preserve useful evidence in `archive/`.
- Update durable documentation only for accepted or rejected behavior, an
  architecture or scope change, a sequencing blocker, installed provenance, or a
  published freeze.

## Planning and budget controls

Two production samples from 17 September set the baseline. The accepted Bento
group-card block took about four hours and 1,260 purchased credits ($50.40 at
2,500 credits per $100) for one architecture solve, one unapproved false start,
one provisional acceptance and several physical adjustments. The whole day took
2,421 credit equivalents ($96.84), about half of one observed weekly included
limit. For scheduling only, treat one weekly limit as roughly 5,000 credits or
$200, and four as roughly $800 a month. This is a planning proxy, not a billing
guarantee.

- Schedule every architecture change or architecture-level fix inside the weekly
  included limit, reserving one focused weekly block before starting. Do not fund
  exploratory architecture with purchased credits or a small residual balance.
- Enter with an approved interaction model and explicit physical acceptance
  checks. If the model is undecided, spend only on evidence and alternatives.
- Budget a comparable architecture solve at no less than 4-6 focused hours and
  one protected weekly block until accepted work gives a better baseline.
- Purchased credits may flex for bounded procedural work --- documentation,
  packaging, installer corrections, mechanical cleanup, or an isolated fix with a
  known controlling property and stop condition --- under a fixed packet,
  acceptance test and spend ceiling. Stop when evidence turns it into
  architecture or product design.
- An estimate schedules work; it does not justify stopping an agent before a
  coherent implementation or a safely preserved candidate exists.
- Preserve unfinished architecture candidates on named WIP branches. Promote only
  physically accepted behavior to `main`.
- Re-estimate after each accepted major slice from elapsed time, credits, false
  starts and physical candidate count.

## Working model

`ROADMAP-CC.md` § Working model owns what a local and a cloud session can each
reach.

## Ordering rationale

The 19 September ownership review found four things that shaped the early
blocks. Ownership had no single representation, prepared tickets carried
presentation, and source-order checks rejected correct code; Blocks 1a and 2
closed those. The fourth still stands: the Shuffle dock is infrastructure rather
than packaging, because it settles Tettegouche's Ambient and ticker width and
gives the Keyboard its mount, so it sits ahead of both rather than downstream of
the components.

Live validation the same day split out two more. Gesture evidence meant nothing
while a boot race chose the edge backend, which put 1e ahead of Block 3's
physical checks; and Bento shape selection proved to be layout work resting on
the ownership paths, which made it Block 3b.

Table waits until the component engines meet their contracts, because proving it
against a shell that does not yet satisfy `CARD-LIFECYCLE.md` proves it against a
moving target. The Keyboard was meant to wait the same way; it was built outside
this order instead, J accepted it as working on 21 September, and the audit
below places it.

The 23 September product and experience audit reordered the open blocks by what
a person meets first rather than by which component owns the work. With the
ownership engine done, the gaps sat between the engine and the person: search
that could not be typed by touch, dialogs the card rules described and the build
did not handle, a contract promising more displays and desktops than the
structure held, and consumer expectations the rules had traded away. Blocks 13 to
16 hold what it found. It also cut items whose premise had gone: the keyboard
overlay retired most of 12a, and Block 7b answers Block 4's reordering. From then
on the table in `ROADMAP-CC.md` is in priority order and a block's number is only
its name. `EXPERIENCE-AUDIT.md` numbers the findings, and a line tagged
`(audit N)` answers finding N.

## Block 1 — Refactor enablement

Done. KWin starts before the Z13 posture service on every boot, so the effect
constructs on Plasma's edges and adopts the direct router when the kit's posture
file appears; the journal line `adopted direct Z13 system edges` is the evidence
that a cold boot reached it. KWin keeps the previous plugin image mapped across
an unload, so `install.sh` loads the effect by name, compares the image KWin
reports through `loadedPluginProvenance` with the file it placed, and says
plainly when a restart is owed. The checks assert behavior rather than where a
symbol sits (`OwnershipHandoff.h`), and `docs/README.md` § One owner per
invariant names each invariant's owner. The terminology pass covered Kadunce
only: Tettegouche still carries the retired term in consumer strings and
documents, and neither it nor Temperance has the vocabulary guard.

## Block 2 — Ownership foundation

Done, and verified on a restarted compositor. Prepared tickets carry intent
rather than a model, ownership and presentation have separate revisions,
`CardOwnership.h` sees all three owners, and `CardOwnershipLedger` accepts only
the six directed transitions and evaluates `CARD-LIFECYCLE.md` §14 in one place.
`DECISIONS.md` records each, from § A prepared ticket carries intent, not a model
to § A prepared type is not always an ownership transition. A violation the
observer reports is a real defect, not a solver decision.

## Block 3 — Ownership behavior

Done, physically accepted on 21 September. Ownership holds at three owners, six
transitions and two violation rules. Deliberate edge entry, the pairing grammar,
deleted overflow, every arrival answered by the layout, a layout ending into card
ownership, top-edge extraction, the sleeping pane and a refused snap that
changes nothing each have their rationale in `DECISIONS.md`, from § A side snap
admits one card to § A minimized pane leaves through a third door.

Two findings outlive it. A probe that stubs a check the gesture makes is not
coverage of that check, so prefer the real collaborator. And an application that
declares a large minimum size claims the wide pane even where a person expected
the narrow one; the answers are Block 4's Spread drop and Table.

## Block 3b — Bento layout grammar

Done, accepted on 21 September within two panes. Each display has one pane cap
that every admission path reads, orientation comes from the work area, no shape
decision reads the output's name, and side contact chooses the shape. The tablet
keeps a two-pane maximum by J's choice on 19 September; the three-pane shapes and
their contact mapping stay dormant behind `BentoContactGrammarPaneCap`.
`DECISIONS.md` § The tablet keeps two panes; the three-pane grammar stays dormant
records why, and what reviving them reopens. The block is pure value code under
the headless suite.

## Block 10a — Integration repository

Done, 21 September: `carlsonjm/shuffle`, private. Its `docs/BOUNDARY.md` states
the one-way rule that no component depends on anything downstream, why an
installation without the dock is supported rather than degraded, and what the
Keyboard's licence reaches once a bundle ships. It was pulled ahead of Block 10
because the Bottom Surface lives there.

## Block 13 — What a person meets first

**Status:** Next. Each item is something a person reaches in the first minutes on
the tablet, and each is reproduced before it is changed.

Done, in brief. Search by touch: Tettegouche's launcher moved to the top layer,
so KWin's keyboard, in the overlay layer, stacks above it, and Kadunce no longer
reads a touch on the keys as leaving its hosted launcher; `keyboard-search-runtime`
gates both. Starting in cards: `DECISIONS.md` § Switching Kadunce on puts the
tablet in cards, gated by `start-cards-runtime`. The first carry: any window
closing anywhere advanced the layout side's generation, which a waiting carry
checks, so the first drag after sign-in or a toggle met popups closing and was
refused. Only a window a layout holds advances it now, the move trace names the
check that refuses a pickup, and `first-carry-runtime` gates it. Dialogs:
`DECISIONS.md` § A dialog waits with its application, gated by `dialog-runtime`
and `dialog-waiting-runtime`.

**A waiting dialog on its card, and travelling with it.** A dialog whose
application is not in front is hidden in KWin --- not drawn, not touchable, never
focused --- and its application raises Plasma's attention flag. Kadunce tells
the application's activation from the person's by order: KWin activates a new
window before announcing it, so an activation before or in the same turn as the
window is the application's. Hiding a focused modal would have KWin focus it
again, so focus moves first to the card in front, or nowhere. In the survey of
J's own applications nothing filled the screen; a Zen dialog took priority and
card swipes waited until it was answered, which J accepts. A Wayland dialog that
names no parent is still admitted as a card; none has been found.

**A window hidden from the task switcher.** `CURRENT_STATE.md` records one of
KWin's own windows, captionless and marked to skip the taskbar and switcher, held
as a card. `CARD-LIFECYCLE.md` §4 already excludes such a window by what it is,
so following the contract is the fix and identifying it is not needed first. The
reason it was left alone is the thing to watch: a window Kadunce stops hiding is
painted wherever it stands, so the exclusion has to be seen on the tablet.

**The keys and the first text field.** On 22 September the first text field
tapped in a session flashed and did not raise the Keyboard, and the second raised
it; it has not been reproduced. KWin's rule has that shape: it shows a forced
input panel only after the panel has been allowed, allows it only on a text-input
update while touch was the last input, and never revokes it, so the first text
field touched unlocks the panel for the rest of the session. A mouse or touchpad
click never raises it, which is Plasma's touch-only setting and correct. The
handle gets past the unlock on a cold start by holding the focus for the length
of a raise with a one-pixel overlay that asks as a text field does.

**The keys after a card is chosen.** A card the stage focuses keeps the keyboard
down for one second, which is also when a quick person taps a text box in the
card they just chose. `DECISIONS.md` § The keyboard comes up for the text, not
for focus stands; the task is that a deliberate tap still raises the keys. The
touch observer that once answered this was in the candidate withdrawn on 23
September, so the answer is not to restore it unchanged.

## Block 14 — Ownership scope

**Status:** Next: a window KWin moves onto the monitor joins its layout. The
24 September hand test failed two checks, recorded below.

`CARD-LIFECYCLE.md` §3 and §11 gave every display and every virtual desktop its
own ownership session; the build had one card workspace, bound to the output
named like a built-in panel, and nothing reacted to a desktop switch. J ruled
both halves on 23 September, recorded in `DECISIONS.md` § Table is required and
uses KDE virtual desktops and § Cards follow the touchscreen.

Built under those rulings. Cards and layouts stay on the desktop where they
started and every other desktop is plain Plasma until Table, gated by
`desktop-switch-runtime` and `desktop-switch-bento-runtime`; once no card or
layout is left, ownership moves to the next desktop that starts one. The card
display is chosen as KWin places the touchscreen: the display the user assigned
it to, else one whose physical size matches the digitizer's, else the built-in
display, else the first. The Z13's digitizer (284 x 187.6 mm against a 288 x 180
mm panel) is placed by the built-in rule. A touchscreen assigned in System
Settings is matched by display name, because KWin's newer match by display
identifier is not exposed to an effect. A touchscreen or display coming or going
re-decides it, and cards on a display that stops being the card display return
to the desktop rather than moving. Only a kernel touchscreen counts, so the
nested harness gives `Virtual-0` a stand-in; `TouchDisplayTest` states the rule
per machine, and `no-touch-runtime` guards the no-touchscreen machine but passes
on the earlier build too. A monitor plugged in or out is `DECISIONS.md` § A
display coming or going keeps every card on the card display, gated by
`output-unplug-runtime`. A bottom swipe with the dock on the monitor had been
going to Plasma's desktop background, a layer surface; the desktop is now
excluded as panels are, gated by `desktop-bezel-runtime`.

**The monitor.** J ruled it on 23 September (`DECISIONS.md` § A display without
cards organizes everything it shows): it organizes every window it shows, and a
window without room goes to the dock. What is built already fills the layout from
every eligible window, so the work is where overflow goes (today it moves to the
tablet as a card), the eight-pane cap (`BentoCuratedPaneCap`; the patterns past
eight exist in `makeBentoLayout`), a window released loose beside a layout, and
the one-window snaps. On a machine with no touchscreen the same rule holds.

**Hand test, 24 September.** One window on the monitor passed from a side and
from the top, and switching off with windows parked returned every one. Two
failed. A window parked or minimized on the monitor was missing from the dock,
because the dock filtered hidden windows and Plasma's task model reads hidden
from minimized on Wayland; the fix passed by hand. Kadunce's half is gated by
`monitor-return-runtime`. Why J could not reach the tray until unplugging is
unexplained. Minimizing the monitor's only visible pane leaves it empty even
when a parked window waits. Past
eight panes, a card carried to the monitor previewed only the Active size at the
top and nothing at the sides, where the ruling gives it one slot and parks that
slot's occupant. `monitor-full-runtime` does this on a 2560x1440 monitor and
takes a slot from both edges; the symptom is unreproduced. It found a snap to
the right edge landing on the left, because the library pattern's large slot is
on the left; the pattern is now mirrored to the snapped side. Each carry preview
onto a display without cards now logs the window's minimum size, the residents'
and the answer, so the next hand test names what decided it. A carried window
wider than half the monitor fits only alone, which is the ruled Active size.

## Block 9 — Shuffle Keyboard

**Status:** In progress. J accepted the build on 21 September as working, not
finished. It lives in `shuffle-keyboard/`, a private fork of KDE's
`plasma-keyboard` 6.7.5 that carries KDE's licence, so whatever ships from it
ships under that licence, which the downstream paid-feature assumption has to
answer. `SHUFFLE-KEYBOARD-1.0-CONCEPT.md` is the product contract and the fork's
`docs/FEASIBILITY.md` and `docs/PHYSICAL_ACCEPTANCE.md` are implementation
evidence; where they disagree the concept governs.

The base is Plasma Keyboard over KWin: a touched key reaches Qt Virtual
Keyboard's input engine, its input-method-v1 client hands the result to KWin, and
KWin delivers it through the application's own text-input path. Shuffle owns
layout, gestures, height, panel handoff and presentation and adds no input
engine. `docs/FEASIBILITY.md` records why Qt Virtual Keyboard alone, Fcitx5 and
synthetic per-application input were not taken.

**The rebuild.** Daily use proved four of the concept's hypotheses wrong, and its
§ Superseded by use records them so they are not rebuilt. J approved their
replacement on 22 September: the space bar as the pointer, armed by distance
rather than a timer; two scrub columns in the width the side pads wasted, history
left and key height right; and the Keyboard on Kadunce's 10 px gutter with a
hard edge. It is unbuilt, so the proof and verification items below measure the
superseded build until it lands. J's open asks are an even gap at the sides and
retiring the top grab. He asked for a gap above the keys so the Keyboard would
read as pushing the window; since the overlay it pushes nothing and the pan
already stops a gutter above the keys, so that is dropped unless he wants it for
its look. Pin two questions for the pass: whether the notch hold reads as
feedback or as lag without haptics, and whether a density falloff adds anything
once the gutter and scrub columns are in. J expects both are already answered.

**Where KWin seats the keys.** KWin 6.7.5 seats an input panel on the bottom of
the work area and moves it again only when its size or input region changes, the
outputs change, or the text cursor moves --- never when the work area grows. The
dock gives up its strut after the Keyboard is seated, so the Bottom Surface
publishes `reserving` and the Keyboard is placed again when it turns false. The
nudge sends one frame a pixel short, because clearing and restoring the input
region at once reaches KWin as no change. `shuffle-keyboard/tests/verify-seat.sh`
measures it and the downstream contract's § Keyboard boundary states it; a
rebuild that changes the Keyboard's size or edges keeps both halves.

**Putting the keys away.** A tap on the top grab closes the Keyboard and a drag
on it resizes. The concept gives that edge to show and hide alone, the downstream
contract records J wanting the keys dragged down, and no document says which the
edge becomes once the grab retires.

**Height and reservation.** The concept's height column still promises that the
window above grows by what the Keyboard gives back, and its engineering
constraints list workspace reservation. The keys cover and never reserve
(`DECISIONS.md` § The keyboard overlays; a covered line pans inside a still
card), so a shorter Keyboard covers less rather than giving a window room.

**Proving the build.** The four-row layout, cascading controls, adjustable height
and the switch to the precision surface are implemented and in daily use. They
were used against Plasma's own bottom panel; the dock they were to be proven
against was accepted on 22 September, so the proof now waits only on the rebuild.
The fork's `docs/PHYSICAL_ACCEPTANCE.md` is the cross-toolkit pass and has not
been run against the suite checkout; it also still expects behavior the dock
changed (audit 39).

**Copy and paste.** The concept returned them to Ctrl chords, which a hand on
glass does not make. Its lesson that one control should not carry two functions
stands; copy and paste are what it cost.

Laying the Keyboard over cards is done. Covered text beyond the Active card is
Block 15's question.

## Block 5 — Bottom Surface

**Status:** In progress. The dock, its sheet, the Keyboard boundary, the handle,
the bezel rule and the gradient are accepted; `ROADMAP-CC.md` orders the rest.

The Bottom Surface is the one layout authority for the Status Bar, Shuffle Dock,
Ambient and the Keyboard boundary, so those surfaces stop negotiating width with
each other. It lives downstream, and Tettegouche and Temperance stay fully
functional without it: an installation without the dock is supported, not
degraded. It is a Plasma panel Shuffle configures and owns outright, hosting
Temperance, with the tray and Kadunce's disable control, and Ambient;
`DECISIONS.md` § The Bottom Surface is a Plasma panel that hosts the components
records the split. `shuffle/docs/BOTTOM-SURFACE-CONTRACT.md` is the interface and
`BOTTOM-SURFACE-MATERIAL.md` beside it carries the approved values.

**Presentation.** Approved by J on 22 September as visual direction, conditional
on the checks below. The region is one material, full at the screen edge and
falling to nothing by the reserved line, never painting above it. It blurs,
drains and darkens the backdrop so the application icons are its only colour,
fills to solid black across the reservation when the backdrop would compromise
contrast, and never lets the reservation follow where the material becomes
invisible. `DECISIONS.md` § The bottom region is one material that fades to
nothing records why. The containment draws it as a plain gradient. The
compositor effect that would add blur and drain is parked (`DECISIONS.md` § The
region's material is a compositor effect of its own, parked); taking it up again
starts by showing that blur and drain can be told from the gradient at all. The
seam that stands the containment's gradient down while an effect draws already
exists, so the two cannot both paint.

**The dock's features.** Its floor is Plasma's Icons-only Task Manager
(`DECISIONS.md` § The platform's answer wins where it has one). J approved on
24 September the Shuffle look for what the stock dock draws on hover: the canvas
at https://claude.ai/artifact/KM8NUM8Cit47ZHeReMufUp. A long press or right-click
opens one sheet as wide as its window pictures: the last-used one ringed,
then window actions as pills, size first, more while the line has room, the
rest behind a more control, then the app's shortcuts behind one row. Pills act
on every window, as Plasma's do; a pinned app's pin stands solid (J, 24
September). Roles are named as `TaskManager.AbstractTasksModel.X`; read
through a property holding the type, the shell returns the window's title. On the icon, white
marks what is open (a dot per window up to three, the bar for the one in front)
and the accent only means attention, a count, progress or starting.

**Allocation.** The dock is fixed to the output's centre and grows symmetrically,
and each flank spends its own side (`DECISIONS.md` § The dock's centre is fixed,
and each flank spends its own side); an asymmetric result is intended.

**Checks owed.** The fade was measured on the tablet panel with no step or
contour, so the band height the contract marks pending is no longer blocked.
Still owed:

- Ambient's ticker stays legible against a bright wallpaper at the thin end of
  the gradient, which has the least material and the most light behind it. J's
  own background is the case to test. Window contact is one input to the solid
  fill, not the whole rule. A third-party containment cannot reach
  `touchingWindow`, which drives Plasma's adaptive opacity inside the shell's
  `Panel.qml`; `TasksModel` region filtering is what replaces it.
- A reduced-transparency preference replaces the gradient with a solid fill.
  `ITASCA-VISUAL-LANGUAGE.md` covers reduced motion and not this; closing the gap
  is this block's.
- A smear line where a faded tint meets an unfaded blur belongs to the parked
  effect and returns with it.

No prior art carries these. Progressive blur is established inside application
content, and the system chrome Shuffle is measured against uses a uniform
translucent material rather than a density gradient.

**The Keyboard's arrival.** The Keyboard comes all the way down and the dock
steps aside for it (`DECISIONS.md` § The dock steps aside for the keys), but the
keys arrive at once while the dock slides out beneath them, so the wallpaper
flashes through between. J named motion that makes the change one action as what
stands between this and flagship quality. The order between the two is the
contract's: presentation out then reservation, and the reverse coming back. The
dock stepping aside is not yet visible, because the Keyboard spans the output and
covers it; the rebuild's side gutter shows it, which is why this waits on Block
9. A cold-start raise from the handle also hides and shows the Keyboard once
within about four milliseconds as the focus holder takes the focus; J has not
reported seeing it, and motion should be judged with it in mind.

**Width.** Temperance sized itself by measuring from its nearest neighbour on the
left, which is correct on an ordinary panel and measures across the dock here, so
it asked for most of the output. Three things now answer that. The flank is the
authority on its own width and clamps a component to its side permanently,
whatever the component does. The surface publishes the dock's extent per output
and Temperance measures to that edge, discovering it at run time, following the
change signal, refusing an unknown major version and doing without it on a plain
panel. And Ambient and the status cluster are placed on their own sides, left and
right, by applet plugin name. None is retested on hardware; once the flanks hold,
the fixes they make redundant retire (audit 20). The extent's other consumer is
the Keyboard's handle, which takes its width from it.

**Clock and calendar** move to Temperance as new work. Temperance's README still
keeps the clock a separate widget (audit 39).

**Done, in brief.** The dock is centred, stores its pinned list with the surface,
carries the platform's own task menu from the native component the stock task
manager uses, reorders by a sideways carry stored at each place it crosses, lists
an application's windows in its menu, and brings back the one used last on a tap
(`DECISIONS.md` § The platform's answer wins where it has one). KWin grants the
window-list protocol only to the shell, so the dock's contents can be reviewed
only on a panel. Two menu gaps remain, neither blocking: an application's own
jump-list entries, which no model role carries, and the move-to-desktop and
move-to-activity submenus. The Keyboard boundary is the contract's `yieldRegion`
and `releaseRegion`, with one holder tracked by bus name; yielding gives up the
reservation, not only the paint, and `shuffle/tests/verify-boundary.sh` runs both
production halves. The handle is a six-pixel bar on the band that reserves its
own height one layer below the surface, and a pull anywhere on the dock raises
the Keyboard, because 21 of 28 measured pulls landed on the dock rather than the
bar. Spread opens only from the bezel (`DECISIONS.md` § Spread opens from the
bezel, not from the dock). The dock grows uncapped until Block 12c.
The compositor closes the handle's surface when its display leaves, so the
handle is rebuilt whenever a display comes or goes, on Kadunce's card display or
else the dock's, and shows only while the dock is there. Physically passed 24
September.

**Hand test, 24 September.** The sheet, its actions and bringing back a
minimized monitor window passed. The dock once stepped aside whenever Zen
focused its own field with no keys shown. A tap on an app in
front puts it away only when it has one window; with several it moves to the
next, which is Plasma's own default at 6.7.5, and J kept the platform's
behavior.

Plasma supplies the rest of the hosting: `X-Plasma-ContainmentType: Panel`,
`PanelView::updateExclusiveZone` reserving exactly the thickness in the normal
hiding mode and nothing in autohide, and no background drawn for a
`NoBackground` containment. Plasma Mobile's QML-only `taskpanel` is the example
to copy. These were read from the Plasma 6.7.5 sources, not measured: the
nested-session probe that asked them took the machine down, and
`TEST-ENVIRONMENT-PROCEDURE.md` governs.

**Exit gate:** the dock is physically centred and grows symmetrically at tablet
and monitor widths, the Status Bar and Ambient each compose into the space their
side leaves them, and every check owed passes on the tablet panel.

## Block 15 — Consumer fundamentals

**Status:** Ready: options for J. Each item is a design conversation with J
before any code, and an accepted answer becomes a task in the block that owns it.
J named the first three as where to start.

- **Seeing the result before letting go.** A side snap's partner comes from a
  Spread order nobody sees, and the contract records that showing it is not
  required. The upper or lower half of an edge silently picks the larger or
  smaller pane. The tablet cannot show which pane an arrival displaces. A refused
  gesture does nothing at all, so it reads as broken.
- **Remembering.** Starting in cards is done (Block 13). Stacks, pairs and order
  are still gone after sign-out, restart or a reload, because Kadunce keeps no
  persistence of its own (`DECISIONS.md` § Persistent membership is not promised
  across unload).
- **Closing by touch.** No gesture closes an app from Spread. Upward travel there
  already means leaving a stack or, at the top edge, becoming Active.
- **Text the keys cover.** Only the Active card pans. Bento panes, ordinary
  windows and clients that report no cursor stay covered, and scrolling does not
  move a message box or prompt that sits at the bottom of its window. The Ghostty
  room and tap taken out on 23 September are an option again: the drop refusal
  that condemned them was a fault on `main`, since fixed in Block 13.
- **The top edge.** Pulling down returns to the current card, which a tap in
  Spread already does. The pull a person expects from the top brings
  notifications and quick settings.
- **Posture.** The model is decided by screen, the touchscreen's display holding
  cards and every other display composing, while the tablet becomes a laptop
  when its keyboard is attached. The posture file only chooses the edge
  recognizer.
- **Pointing.** The precision surface lives in the Keyboard, so a right-click in
  a desktop application costs raising the keys.
- **One home.** Applications are found in the dock, in Spread, in the search
  launcher and in Ambient.
- **Unmeasured.** No portrait pass and no physical sleep-and-wake review is on
  record.

## Block 4 — Kadunce manipulation

**Status:** In progress. A placement a client moves out of no longer costs the
layout (`DECISIONS.md` § A placement that does not settle sheds), and a stacked
card is released by pulling it up out of its stack (`CARD-LIFECYCLE.md` §9).
Reordering moved to Block 7b and the motion items to Block 16 on 23 September.

**A Spread drop that names the pane it replaces.** The Bento group is drawn as a
live picture of the layout with its panes in position, so it is already a map:
dropping a card on one half states the side the tablet has no screen edge for,
and the person sees the target before letting go. J shelved it from Block 3 on
20 September because §8's automatic rule answers every arrival without it; it
waits until MVP criteria are met, and its place first in the block is its
priority when the block is picked up. It is also the tablet's answer to the
wide-pane quirk in Block 3.

**First entry adopts every eligible window.** The Card half is built and needs
confirming rather than writing: `adoptDisplayWithActive` commits the carry before
publishing anything, so a refused entry leaves every window Native rather than
half of them owned, and `isApplicationWindow` limits the sweep to the current
desktop and activity. The Bento half concerns only a display that cannot own
cards, since the tablet's shortcut names two windows, and waits on Block 14's
monitor answer.

**Displace by side.** When a snap reaches a full layout, the pane that yields
should be the one holding the side the card was released into (§5), so a person
sees which pane yields while dragging. A full layout already yields rather than
parks, and the publisher evicts whichever pane the solve leaves out; which pane
that is still comes from `chooseBentoSideAdmission`'s resident order rather than
the contacted side. It serves only a display where a side release into a live
layout exists; §8 answers every arrival that states no side, which is every
arrival the tablet makes.

## Block 16 — Motion

**Status:** Ready. Motion had been split across Block 4 (arrival, displacement,
cancellation, neighbours, reduced motion), Block 5 (the Keyboard's arrival), 12a
(the Active card) and 7b (the deck), each able to set its own timings.
`ITASCA-VISUAL-LANGUAGE.md` § Motion and animation already states the tiers,
easing, choreography and reduced motion; this block holds every custom motion to
it. The Keyboard's arrival is built in Block 5, where J named it, and reviewed
here beside the rest. Panned contents jump rather than slide; that is the one
motion the keyboard overlay added.

## Block 6 — Tettegouche completion

**Status:** Width work waits on Block 5; the rest is ready.

- **Search and Files under the keys.** Written when the Keyboard reserved
  workspace and Tettegouche would have answered a smaller area. The keys now lie
  over the screen and change no work area, so what has to hold is that the search
  field and its results stay above them. J confirmed on 21 September that
  Tettegouche is not optimized for a virtual keyboard. The Keyboard's Meta key
  already resolves the user's live bare-Meta binding through KDE's global-shortcut
  service, so the seam exists; Tettegouche's composition is what has to answer.
- **Ambient validation** against `AMBIENT-CONTRACT.md`: MPRIS, Plasma jobs, Tette
  operations, Downloads arrivals and concurrent density.
- **A waiting dialog in Ambient.** Kadunce marks an application whose dialog
  waits with Plasma's attention flag and depends on nothing that shows it
  (`DECISIONS.md` § A dialog waits with its application). J chose Ambient as the
  place it shows: a waiting dialog is ongoing state with an owner and a truthful
  action, which is Ambient's admission rule, while Temperance keeps passing
  events. Whether the dock also shows the flag is unmeasured, because the hand
  test's helper ran from a terminal and had no dock icon.
- **Touch-first Ambient and Files.** Ambient's play, skip and cancel are 20px
  icons in a 42px strip whose own tap opens details, so a near miss opens the
  wrong thing; Temperance holds its actions to 44px and Ambient has no such rule.
  Files selects on a tap and opens on a double tap, and a 180ms hold both selects
  and opens the action menu, which is desktop grammar.
- Transfers keep the no-silent-overwrite guarantee. Files' properties, previews,
  recursive search, Recent and bounded error states are separately accepted
  packets. Storage lifecycle must not weaken KIO ownership or truthful reporting.

**Exit gate:** each slice has focused automated checks and an installed
acceptance pass, and one provider failure does not block unrelated slices.

## Block 7 — Temperance event boundary

**Status:** Ready; the smallest remaining component block.

- A system event is admitted only with defined identity, freshness, priority,
  deduplication and dismissal.
- Tettegouche keeps live progress and actions, and neither side retains the same
  completion indefinitely.
- The ticker's next-notification control is enabled only while a hover reveals
  it, so a finger can open history with the bell but cannot page.
- Log Out is one tap with no confirmation, an easy accidental tap on a tablet.
- Presenter switching waits until its process-wide lifecycle is proven.

## Block 6b — Maintainability and lifecycle audit

**Status:** Ready. Bounded on purpose: it removes duplication that carries real
reliability or maintenance risk and does not reopen stable architecture for
style. `EXPERIENCE-AUDIT.md` finding 17 names timers that guess intent, among
them the handle's 300ms fallback and download rows that vanish after a quiet
period, and finding 39 lists the stale documents across the suite.

## Block 7b — Spread deck motion

**Status:** Waits on Blocks 5, 6 and 7, at J's sequencing on 21 September.

The Spread row is discrete in both directions. Browsing classifies a swipe only
once the finger lifts, then plays a fixed 220ms page; a carried card tracks the
finger while the row behind it stays still. Nothing follows the hand, so there is
no speed, no coasting and no sense of touching the deck rather than instructing
it. A deck that is thrown is the model J named on 21 September from an earlier
tablet product. Its vertical half is built: §9's pull up out of a stack and §10's
top edge are that throw.

The long reorder sweep and the edge-dwell strip both exist to compensate for a
row that cannot be dragged. A reorder commits at 82% of a card's pitch (705px of
an 860px pitch on the tablet), and paging arms on a 300ms dwell inside a 115px
edge zone, so a sweep long enough to reorder ends where a pause pages. A third
distance cannot fix it (`DECISIONS.md` § Gestures split by distance need the band
between them); a row that follows the finger has no thresholds to separate. Until
then reordering keeps the long sweep, and J has not reported hitting the edge
zone in practice.

- **The row's shape.** It wraps, so it has no beginning or end; the named model
  had two ends with a spring at each, which is how a hand knows where it is
  without looking. Put to J on 21 September and deferred with the block.
- **Retiring edge-dwell paging** keeps the wrap handling `RowPageMotion.h`
  proved: a shoulder leaves and re-enters at the edges rather than flying across
  the centre card.
- **A held card drawn from under the finger.** It is drawn from its slot, so the
  same travel shows a different picture depending on where it was picked up;
  physical review found that before any threshold was blamed.
- **Something that reports the order.** The index `workspaceContext` reports is
  stable metadata that does not move when the order does, so no reordering change
  has been observable to an automated check. `rowPositionForId` was added on
  `wip/reorder-push-20260921` for that; recover it rather than rediscover it.

Input routing is local-session work, and physical review is the gate: the whole
block is a hand judgment that no automated check substitutes for.

## Block 9b — Shuffle Lock

**Status:** Waits on Block 9. No contract, concept or implementation exists in
any repository. J confirmed on 23 September that it ships in the consumer bundle
whatever else 1.0 holds, and judges it an afternoon's work once the Keyboard is
rebuilt.

Shuffle Lock is a privacy-first presentation over the system lock. It needs the
Keyboard, because authenticating on a tablet without peripherals requires it.
The contract comes first and is Open decision 2 in `ROADMAP-CC.md`: what is
concealed, what is shown, and what a person can do before authenticating. KDE's
screen locker stays the authentication authority; Shuffle presents and never
handles credentials or replaces the lock.

**Exit gate:** the lock conceals workspace content, authenticates through the
system locker, takes Keyboard input reliably, and a failure in Shuffle's
presentation never leaves a session unlocked or unrecoverable.

## Block 10 — Downstream assembly and installation

**Status:** Waits on Block 9.

KDE publishes no stable KWin effect ABI, so a Plasma update can leave the
installed plugin unloadable. That failure is already safe: KWin declines the
plugin, the tray switch persists and the desktop keeps working. The guided repair
rebuilds a retained source snapshot on the user's machine, which needs a full
C++, Qt, KDE Frameworks and KWin toolchain there. J approved shipping the plugin
as a package from a project pacman repository, built per KWin release in CI
(`DECISIONS.md` § A consumer gets a working plugin from the package manager). It
belongs here because until the consumer bundle exists there is no repository to
publish to and no consumer to protect; the guided repair serves development
machines, which have the toolchain anyway.

The product needs a version identity distinct from its component versions, and
the Kadunce disable control has to work at every step of install, upgrade,
rollback and uninstall. The Keyboard's KDE licence reaches whatever ships from
its fork (Block 9).

## Block 10b — Package and interface identity

**Status:** Ready; coordinated across all three repositories.

Layer 3 of `TERMINOLOGY.md`. Two namespaces are in use, `studio.warbler.*` and
`io.github.carlsonjm.*`, neither shared across the suite, and Tettegouche
hard-codes `studio.warbler.Kadunce` in seven call sites. The rename takes in the
`showCardLine` method, the `cardLine` context value and the persisted
global-shortcut identity, and bumps the workspace-context schema and
launcher-guest protocol versions together with Tettegouche's supported versions.
Temperance 1.1.0 showed that a package identity change makes users remove and
re-add the widget, which is what the migration has to spare them.

**Exit gate:** one namespace, no retired vocabulary on any public surface, and a
clean install and upgrade on a machine that has the previous identities.

## Block 11 — Host and public site

**Status:** Waits on Block 10. The site demonstrates the released product and
gives a complete supported installation path.

## Block 8 — Table

**Status:** In 1.0, decided by J on 23 September, replacing the tentative 1.1
ruled earlier that day. It keeps its place in the order, because the one-desktop
rule Block 14 built is stable enough to carry the plan until then. Reference:
`KADUNCE-TABLE-1.1-CONCEPT.md`.

A destination Spread needs a card set per virtual desktop, which the structure
does not have, so giving each desktop its own cards is this block's architecture
rather than feasibility. Two earlier answers are to be reopened here. A window a
layout cannot show moves to the display that can hold it as a card, the only
settling place this build has; J accepted that on 20 September for this version
and expects Table to offer more places to settle a misplaced card. And an
application that declares a large minimum and takes the wide pane against a
person's intent is partly Table's to answer.

**Exit gate:** a person enters Table, moves a real managed window between
existing KDE virtual desktops, enters the destination Spread, and sees correct
desktop membership without regressing normal KDE switching or display ownership.

## Block 12 — Post-MVP debug

Real work that does not gate MVP, held in one place so it is not lost.

### 12a. Keyboard and the dock's room

Deferred past MVP by J on 21 September, and re-cut on 23 September when the
keyboard overlay removed most of its premise: nothing lifts a card, and Kadunce
places nothing on a keyboard change, so the stage-gutter defect it opened with no
longer arises. Two checks are left. The Active card's pan places from the card's
frame at keyboard-open rather than from the grown work area, and Bento and Spread
were accepted untouched, so what remains is to confirm nothing else grows into
the dock's room while the dock steps aside (`DECISIONS.md` § A dock that steps
aside is not a display that grew). And the strip just above a panned card still
passes a tap to the part of the client the pan hid.

### 12b. Edge gestures on any touchscreen

Added by J on 22 September: the gestures have to work for anyone who installs
Shuffle on a touchscreen, not only on the development tablet.

Kadunce chooses between two recognizers by device. With no Z13 kit it registers
Plasma's own touch borders, bottom for Spread and top for Active, which every
Plasma touchscreen already has and each user configures. When the kit's posture
file appears it hands both edges back to Plasma and uses its own router. The kit
supplies no edge detection; its file only says this is the tablet. So the path
every other user gets has not been exercised since the tablet began adopting the
direct router within seconds of every boot, and the rule that picks between them
names one device. The 20-pixel bezel rule from Block 5 lives in the direct
router; the choice is whether it becomes the one rule on every touchscreen with
Plasma's borders as the fallback, or Plasma's borders become the rule and the
direct router retires. Proving Plasma's borders includes a reload and a cold
boot.

### 12c. Dock overflow

Deferred from Block 5 on 22 September: the dock grows uncapped, because a cap
earns nothing at MVP working-set sizes, and the flanks clip, so an overgrown dock
crowds them rather than corrupting the layout. Revisit with a measured working
set, not a predicted one.

## Open product decisions

`ROADMAP-CC.md` § Open decisions owns the list and each resolution. The rationale
for a settled one is in `DECISIONS.md`; the context for an open one is under the
block that carries it.
